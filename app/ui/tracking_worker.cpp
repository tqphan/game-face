#include "tracking_worker.h"

#include <input_devices_simulator/script.h>
#include <input_devices_simulator/simulator.h>

#include <cmath>

namespace ids = input_devices_simulator;

TrackingWorker::TrackingWorker(PlatformFactory platform)
    : platform_factory_(std::move(platform))
    , engine_([this](const std::string& command) { execute(command); })
{
    engine_.setEdgeSink([this](game_face::BindingEngine::Edge edge) {
        if (!settling_ && onEdge)
            onEdge(edge);
    });
}

TrackingWorker::~TrackingWorker()
{
    releaseAll();
}

ids::Simulator& TrackingWorker::simulator()
{
    if (!simulator_) {
        simulator_ = platform_factory_ ? std::make_unique<ids::Simulator>(platform_factory_())
                                       : std::make_unique<ids::Simulator>();
    }
    return *simulator_;
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
    head_mouse_.reset();
    had_face_ = false;
    clock_.start();
    return {};
}

void TrackingWorker::stop()
{
    settle();
    releaseAll();
    engine_.reset();
    head_mouse_.reset();
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
        ids::Simulator& sim = simulator();
        if (sim.permission() == ids::Permission::Denied
            && sim.requestPermission() != ids::Permission::Granted && onError) {
            onError(tr("game-face needs permission to simulate input (macOS: Accessibility; "
                       "Linux: write access to /dev/uinput)."));
        }
    }
}

void TrackingWorker::setSettleOnStop(bool enabled)
{
    settle_on_stop_ = enabled;
}

void TrackingWorker::setHeadMouseSettings(const game_face::HeadMouseSettings& settings)
{
    head_mouse_.setSettings(settings);
}

void TrackingWorker::setScreenGeometry(const QRect& geometry)
{
    screen_ = geometry;
}

void TrackingWorker::recenterHeadMouse()
{
    head_mouse_.recenter();
}

void TrackingWorker::calibrateHeadMouse()
{
    head_mouse_.startCalibration();
}

void TrackingWorker::pause()
{
    settle();
    releaseAll();
    engine_.reset();
    head_mouse_.reset();
    had_face_ = false;
}

void TrackingWorker::process(const QImage& image)
{
    process(image, static_cast<double>(clock_.isValid() ? clock_.elapsed() : 0));
}

void TrackingWorker::process(const QImage& image, double time_ms)
{
    FrameResult result;
    frame_time_ms_ = time_ms;
    if (tracker_ && !image.isNull()) {
        const game_face::RgbImageView view{image.constBits(), image.width(), image.height(),
                                           static_cast<int>(image.bytesPerLine())};
        auto face = tracker_->detect(view, static_cast<std::int64_t>(time_ms));
        if (!face) {
            if (onError)
                onError(QString::fromStdString(face.error().message));
        } else if (face->face_found) {
            result.face_found = true;
            result.signal_values = game_face::toSignals(*face);
            std::copy_n(result.signal_values.begin(), result.scores.size(), result.scores.begin());
            result.landmarks = std::move(face->landmarks);
            // As in the original app, bindings only see frames with a face,
            // so keys pressed before the face was lost stay pressed.
            engine_.process(result.signal_values, time_ms);
            saw_face_ = true;
            // After the bindings, so a headmouse.* command they send applies
            // to this frame.
            result.head_mouse = head_mouse_.process(result.signal_values, time_ms);
            applyHeadMouse(result.head_mouse);
        }
    }
    if (!result.face_found && had_face_) {
        // Lost the face: forget the motion, so the pointer doesn't jump when
        // it's back.
        head_mouse_.reset();
    }
    had_face_ = result.face_found;
    result.head_mouse_calibrating = head_mouse_.calibrating();
    reportHeadMouseSettings();

    const auto status = engine_.status();
    result.status.assign(status.begin(), status.end());
    if (onFrame)
        onFrame(std::move(result));
}

void TrackingWorker::applyHeadMouse(const game_face::HeadMouseOutput& output)
{
    if (!injection_enabled_ || !output.moving)
        return;
    if (!mouse_) {
        auto created = simulator().createMouse();
        if (!created) {
            if (onError)
                onError(tr("Head mouse: %1").arg(QString::fromStdString(created.error().message)));
            return;
        }
        mouse_ = std::move(*created);
    }
    ids::Status status;
    if (output.absolute) {
        if (screen_.isValid()) {
            // -1..1 from the centre, up positive; screen y grows downward.
            const double half_w = (screen_.width() - 1) / 2.0;
            const double half_h = (screen_.height() - 1) / 2.0;
            const int x = static_cast<int>(std::lround(screen_.x() + half_w + (*output.absolute)[0] * half_w));
            const int y = static_cast<int>(std::lround(screen_.y() + half_h - (*output.absolute)[1] * half_h));
            status = mouse_->moveTo(x, y);
        }
    } else if (output.dx != 0 || output.dy != 0) {
        status = mouse_->moveBy(output.dx, -output.dy);
    }
    if (status && (output.scroll_x != 0 || output.scroll_y != 0))
        status = mouse_->scroll(output.scroll_x, output.scroll_y);
    if (!status && onError)
        onError(tr("Head mouse: %1").arg(QString::fromStdString(status.error().message)));
}

void TrackingWorker::reportHeadMouseSettings()
{
    if (head_mouse_.takeSettingsChanged() && onHeadMouseSettings)
        onHeadMouseSettings(head_mouse_.settings());
}

void TrackingWorker::execute(const std::string& command)
{
    if (auto head_mouse = game_face::parseHeadMouseCommand(command)) {
        if (!*head_mouse) {
            if (onError)
                onError(tr("%1: %2").arg(QString::fromStdString(command),
                                         QString::fromStdString(head_mouse->error().message)));
            return;
        }
        runHeadMouseCommand(**head_mouse);
        return;
    }

    if (!injection_enabled_)
        return;
    if (!runner_)
        runner_ = std::make_unique<ids::ScriptRunner>(simulator());
    auto parsed = ids::parseCommand(command);
    if (!parsed) {
        if (onError)
            onError(tr("%1: %2").arg(QString::fromStdString(command), QString::fromStdString(parsed.error().message)));
        return;
    }
    // Making the expression that clicks moves the head a little; hold the
    // pointer still for a moment so the click lands where it was aimed.
    if (const auto* button = std::get_if<ids::command::MouseButton>(&*parsed)) {
        const double freeze = head_mouse_.settings().click_freeze_ms;
        if (freeze > 0 && button->action != ids::command::MouseButton::Action::Release)
            head_mouse_.freezeUntil(frame_time_ms_ + freeze);
    }
    auto status = runner_->execute(*parsed);
    if (!status && onError)
        onError(tr("%1: %2").arg(QString::fromStdString(command), QString::fromStdString(status.error().message)));
}

void TrackingWorker::runHeadMouseCommand(game_face::HeadMouseCommand command)
{
    using C = game_face::HeadMouseCommand;
    // Recentering and calibrating move nothing, so they work even with input
    // simulation off; the rest follow the same rule as other commands.
    if (command == C::Recenter) {
        head_mouse_.recenter();
        return;
    }
    if (command == C::Calibrate) {
        head_mouse_.startCalibration();
        return;
    }
    if (!injection_enabled_)
        return;
    switch (command) {
    case C::Toggle: head_mouse_.setEnabled(!head_mouse_.settings().enabled); break;
    case C::Enable: head_mouse_.setEnabled(true); break;
    case C::Disable: head_mouse_.setEnabled(false); break;
    case C::Hold: head_mouse_.setHeld(true); break;
    case C::Release: head_mouse_.setHeld(false); break;
    default: break;
    }
}

void TrackingWorker::releaseAll()
{
    if (runner_)
        runner_->releaseAll();
    if (mouse_)
        mouse_->releaseAll();
}

// Sends each binding's stop command before releaseAll, which only releases
// held keys and buttons.
void TrackingWorker::settle()
{
    if (settle_on_stop_ && saw_face_) {
        // Settling is cleanup, not something the user did: no sounds.
        settling_ = true;
        engine_.settle(frame_time_ms_);
        settling_ = false;
    }
    saw_face_ = false;
}
