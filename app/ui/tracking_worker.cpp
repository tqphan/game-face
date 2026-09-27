#include "tracking_worker.h"

#include <input_devices_simulator/script.h>
#include <input_devices_simulator/simulator.h>

namespace ids = input_devices_simulator;

TrackingWorker::TrackingWorker()
    : engine_([this](const std::string& command) { execute(command); })
{
}

TrackingWorker::~TrackingWorker()
{
    releaseAll();
}

QString TrackingWorker::start(const TrackerConfig& config)
{
    game_face::FaceTrackerOptions options;
    options.library_path = config.library_path;
    options.model_path = config.model_path;
    options.mode = game_face::FaceTrackerOptions::Mode::Video;
    options.min_face_detection_confidence = config.detection_confidence;
    options.min_face_presence_confidence = config.presence_confidence;
    options.min_tracking_confidence = config.tracking_confidence;

    auto tracker = game_face::FaceTracker::create(options);
    if (!tracker)
        return QString::fromStdString(tracker.error().message);
    tracker_ = std::move(*tracker);
    engine_.reset();
    clock_.start();
    return {};
}

void TrackingWorker::stop()
{
    releaseAll();
    engine_.reset();
    tracker_.reset();
}

void TrackingWorker::setProfile(const game_face::Profile& profile, bool keep_state)
{
    if (keep_state) {
        engine_.updateProfile(profile);
    } else {
        releaseAll();
        engine_.setProfile(profile);
    }
}

void TrackingWorker::setInjectionEnabled(bool enabled)
{
    if (enabled == injection_enabled_)
        return;
    if (!enabled)
        releaseAll();
    injection_enabled_ = enabled;
    if (enabled) {
        // Re-arm so conditions that are already true fire now that input is on.
        engine_.reset();
        if (!simulator_)
            simulator_ = std::make_unique<ids::Simulator>();
        if (simulator_->permission() == ids::Permission::Denied
            && simulator_->requestPermission() != ids::Permission::Granted && onError) {
            onError(tr("game-face needs permission to simulate input (macOS: Accessibility; "
                       "Linux: write access to /dev/uinput)."));
        }
    }
}

void TrackingWorker::process(const QImage& image)
{
    FrameResult result;
    if (tracker_) {
        if (!image.isNull()) {
            const game_face::RgbImageView view{image.constBits(), image.width(), image.height(),
                                               static_cast<int>(image.bytesPerLine())};
            const double now = static_cast<double>(clock_.elapsed());
            auto face = tracker_->detect(view, static_cast<std::int64_t>(now));
            if (!face) {
                if (onError)
                    onError(QString::fromStdString(face.error().message));
            } else if (face->face_found) {
                result.face_found = true;
                result.scores = game_face::toScores(*face);
                result.landmarks = std::move(face->landmarks);
                // As in the original app, bindings only see frames with a face,
                // so keys pressed before the face was lost stay pressed.
                engine_.process(result.scores, now);
            }
        }
    }
    const auto status = engine_.status();
    result.status.assign(status.begin(), status.end());
    if (onFrame)
        onFrame(std::move(result));
}

void TrackingWorker::execute(const std::string& command)
{
    const QString text = QString::fromStdString(command);
    if (!injection_enabled_) {
        if (onCommand)
            onCommand(text, false);
        return;
    }
    if (!runner_) {
        if (!simulator_)
            simulator_ = std::make_unique<ids::Simulator>();
        runner_ = std::make_unique<ids::ScriptRunner>(*simulator_);
    }
    auto status = runner_->execute(command);
    if (!status && onError)
        onError(tr("%1: %2").arg(text, QString::fromStdString(status.error().message)));
    if (onCommand)
        onCommand(text, status.has_value());
}

void TrackingWorker::releaseAll()
{
    if (runner_)
        runner_->releaseAll();
}
