#pragma once

#include <game_face/core/binding_engine.h>
#include <game_face/core/blendshapes.h>
#include <game_face/core/face_tracker.h>
#include <game_face/core/head_mouse.h>
#include <game_face/core/signals.h>

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QString>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace input_devices_simulator {
class Mouse;
class Platform;
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
    game_face::SignalValues signal_values{};
    std::vector<game_face::Landmark> landmarks;
    std::vector<game_face::BindingStatus> status;
    // The head mouse's readout for this frame (filled while disabled too).
    game_face::HeadMouseOutput head_mouse;
    bool head_mouse_calibrating = false;
};

// Face tracking, bindings and input injection, on a thread of its own.
// Every method must run on that thread (AppController calls them through
// QMetaObject::invokeMethod). Results go out through the callbacks, which are
// also called on that thread.
class TrackingWorker : public QObject {
    Q_OBJECT

public:
    // Makes the platform input goes to; by default, the real one. Tests and
    // --recording-input pass a RecordingPlatform, which injects nothing.
    using PlatformFactory = std::function<std::unique_ptr<input_devices_simulator::Platform>()>;

    explicit TrackingWorker(PlatformFactory platform = {});
    ~TrackingWorker() override;

    std::function<void(FrameResult)> onFrame;
    std::function<void(QString message)> onError;
    // A binding's start or stop fired (for its sound); not while settling.
    std::function<void(game_face::BindingEngine::Edge edge)> onEdge;
    // The head mouse changed its own settings (a headmouse.* command, a
    // captured neutral, a finished calibration); the owner saves them.
    std::function<void(game_face::HeadMouseSettings)> onHeadMouseSettings;

    // Returns an error message, or an empty string on success.
    QString start(const TrackerConfig& config);
    void stop();
    void setProfile(const game_face::Profile& profile, bool keep_state);
    void setInjectionEnabled(bool enabled);
    // When on, stop() and pause() first run the bindings as if every
    // blendshape dropped to 0 (BindingEngine::settle).
    void setSettleOnStop(bool enabled);
    void setHeadMouseSettings(const game_face::HeadMouseSettings& settings);
    // Where absolute mode places the pointer, in the simulator's desktop pixels.
    void setScreenGeometry(const QRect& geometry);
    void recenterHeadMouse();
    void calibrateHeadMouse();
    // Releases held input and re-arms the bindings; for when frames stop.
    void pause();
    // `image` is RGB888. Frames are converted on the GUI thread: calling
    // QVideoFrame::toImage() here would create a thread-local Direct3D device
    // that deadlocks when this thread exits (Windows loader lock).
    void process(const QImage& image);
    // The same with the frame's time given, for tests and recorded frames;
    // times must not go backwards.
    void process(const QImage& image, double time_ms);

private:
    void execute(const std::string& command);
    void runHeadMouseCommand(game_face::HeadMouseCommand command);
    void applyHeadMouse(const game_face::HeadMouseOutput& output);
    void reportHeadMouseSettings();
    input_devices_simulator::Simulator& simulator();
    void releaseAll();
    void settle();

    PlatformFactory platform_factory_;
    std::unique_ptr<game_face::FaceTracker> tracker_;
    game_face::BindingEngine engine_;
    game_face::HeadMouse head_mouse_;
    std::unique_ptr<input_devices_simulator::Simulator> simulator_;
    std::unique_ptr<input_devices_simulator::ScriptRunner> runner_;
    // The head mouse's own device, separate from the runner's.
    std::unique_ptr<input_devices_simulator::Mouse> mouse_;
    QRect screen_;
    bool injection_enabled_ = false;
    bool settle_on_stop_ = false;
    bool settling_ = false;
    // The bindings have seen a face since tracking (re)started, so there is
    // something to settle.
    bool saw_face_ = false;
    bool had_face_ = false;  // the previous frame had a face
    double frame_time_ms_ = 0;
    QElapsedTimer clock_;
};
