#pragma once

#include <game_face/core/binding_engine.h>
#include <game_face/core/blendshapes.h>
#include <game_face/core/face_tracker.h>

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace input_devices_simulator {
class Simulator;
class ScriptRunner;
}

struct TrackerConfig {
    std::filesystem::path library_path;
    std::filesystem::path model_path;
    float detection_confidence = 0.5f;
    float presence_confidence = 0.5f;
    float tracking_confidence = 0.5f;
};

struct FrameResult {
    bool face_found = false;
    game_face::BlendshapeScores scores{};
    std::vector<game_face::Landmark> landmarks;
    std::vector<game_face::BindingStatus> status;
};

// Face tracking, bindings and input injection, on a thread of its own.
// Every method must run on that thread (AppController calls them through
// QMetaObject::invokeMethod). Results go out through the callbacks, which are
// also called on that thread.
class TrackingWorker : public QObject {
    Q_OBJECT

public:
    TrackingWorker();
    ~TrackingWorker() override;

    std::function<void(FrameResult)> onFrame;
    std::function<void(QString command, bool injected)> onCommand;
    std::function<void(QString message)> onError;

    // Returns an error message, or an empty string on success.
    QString start(const TrackerConfig& config);
    void stop();
    void setProfile(const game_face::Profile& profile, bool keep_state);
    void setInjectionEnabled(bool enabled);
    // `image` is RGB888. Frames are converted on the GUI thread: calling
    // QVideoFrame::toImage() here would create a thread-local Direct3D device
    // that deadlocks when this thread exits (Windows loader lock).
    void process(const QImage& image);

private:
    void execute(const std::string& command);
    void releaseAll();

    std::unique_ptr<game_face::FaceTracker> tracker_;
    game_face::BindingEngine engine_;
    std::unique_ptr<input_devices_simulator::Simulator> simulator_;
    std::unique_ptr<input_devices_simulator::ScriptRunner> runner_;
    bool injection_enabled_ = false;
    QElapsedTimer clock_;
};
