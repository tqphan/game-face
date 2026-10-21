#pragma once

#include "frame_player.h"
#include "list_models.h"
#include "settings_object.h"
#include "sound_player.h"
#include "tracking_worker.h"

#include <game_face/core/face_tracker.h>
#include <game_face/core/profiles.h>

#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QVariantList>

#include <memory>
#include <vector>

// Command-line options main() sets before the QML engine creates App.
struct StartupOptions {
    // Frames to play in place of the camera: an image folder or a video file.
    QString frames;
    double framesFps = 0;      // 0: the folder's fps.txt, or 30
    bool quitWhenFramesEnd = false;
    // Settings and profiles folder; empty for the usual AppConfigLocation.
    QString configFolder;
    // Print input as text instead of injecting it (RecordingPlatform).
    bool recordingInput = false;
    // Start tracking at startup even if the settings don't say to.
    bool startTracking = false;
};

// The `App` singleton QML talks to. Owns settings, profiles, the camera and
// the tracking thread. Lives on the GUI thread. The QML engine creates and
// owns it (it is destroyed with the engine); main() fetches it with
// QQmlEngine::singletonInstance to call initialize().
class AppController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(SettingsObject* settings READ settings CONSTANT)
    Q_PROPERTY(ProfileListModel* profiles READ profiles CONSTANT)
    Q_PROPERTY(BindingListModel* bindings READ bindings CONSTANT)
    Q_PROPERTY(BlendshapeModel* blendshapes READ blendshapes CONSTANT)
    Q_PROPERTY(QStringList blendshapeNames READ blendshapeNames CONSTANT)
    Q_PROPERTY(int profileIndex READ profileIndex WRITE setProfileIndex NOTIFY profileIndexChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY trackingChanged)
    Q_PROPERTY(bool faceFound READ faceFound NOTIFY faceFoundChanged)
    // Profiles changed since they were last loaded or saved.
    Q_PROPERTY(bool profilesDirty READ profilesDirty NOTIFY profilesDirtyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QObject* videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
    Q_PROPERTY(QString configFolder READ configFolder CONSTANT)
    // [{id, name}] of the start/stop sounds Settings offers.
    Q_PROPERTY(QVariantList sounds READ sounds CONSTANT)
    // Every name an expression or head mouse source can use.
    Q_PROPERTY(QStringList signalNames READ signalNames CONSTANT)
    Q_PROPERTY(QStringList headSignalNames READ headSignalNames CONSTANT)
    // [{id, name, x, y}] of the head mouse presets.
    Q_PROPERTY(QVariantList headMousePresets READ headMousePresets CONSTANT)
    // Live head mouse readout: [{active, raw, normalised}] for pointer X,
    // pointer Y, scroll X and scroll Y.
    Q_PROPERTY(QVariantList headMouseAxes READ headMouseAxes NOTIFY headMouseChanged)
    Q_PROPERTY(bool headMouseCalibrating READ headMouseCalibrating NOTIFY headMouseChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    // Call before the QML engine creates App.
    static void setStartupOptions(const StartupOptions& options);
    ~AppController() override;

    // Loads settings and profiles; starts tracking if configured to.
    void initialize();

    SettingsObject* settings() const { return settings_; }
    ProfileListModel* profiles() const { return profileModel_; }
    BindingListModel* bindings() const { return bindingModel_; }
    BlendshapeModel* blendshapes() const { return blendshapeModel_; }
    QStringList blendshapeNames() const;
    int profileIndex() const { return profileSet_.selection; }
    void setProfileIndex(int index);
    bool tracking() const { return tracking_; }
    bool faceFound() const { return faceFound_; }
    bool profilesDirty() const { return profilesDirty_; }
    QString statusMessage() const { return statusMessage_; }
    QVariantList cameras() const;
    QObject* videoOutput() const { return videoOutput_; }
    void setVideoOutput(QObject* output);
    QString configFolder() const;
    QVariantList sounds() const { return SoundPlayer::sounds(); }
    QStringList signalNames() const;
    QStringList headSignalNames() const;
    QVariantList headMousePresets() const;
    QVariantList headMouseAxes() const { return headMouseAxes_; }
    bool headMouseCalibrating() const { return headMouseCalibrating_; }
    // The pointer's normalised offset from neutral (x right, y up) and the
    // deadzone, for the overlay's puck; showPuck is false when there's nothing to show.
    QPointF headMousePuck() const { return headMousePuck_; }
    double headMouseDeadzone() const;
    bool showHeadMousePuck() const;

    const std::vector<game_face::Landmark>& landmarks() const { return landmarks_; }

    Q_INVOKABLE void toggleTracking();
    Q_INVOKABLE void startTracking();
    Q_INVOKABLE void stopTracking();
    Q_INVOKABLE bool saveSettings();
    Q_INVOKABLE bool saveProfiles();
    Q_INVOKABLE void createProfile(const QString& name);
    Q_INVOKABLE void removeProfile();
    Q_INVOKABLE void addBinding();
    Q_INVOKABLE void removeBinding(int row);
    Q_INVOKABLE void previewSound(const QString& name) { soundPlayer_->preview(name); }
    // Captures the current head position as the head mouse's center (on the
    // next frame with a face).
    Q_INVOKABLE void recenterHeadMouse();
    // Measures the head mouse ranges over the next few seconds.
    Q_INVOKABLE void calibrateHeadMouse();
    // Stops tracking and auto-saves. Call before quitting.
    Q_INVOKABLE void shutdown();

signals:
    void profileIndexChanged();
    void trackingChanged();
    void faceFoundChanged();
    void profilesDirtyChanged();
    void statusMessageChanged();
    void camerasChanged();
    void videoOutputChanged();
    void landmarksChanged();
    void headMouseChanged();

private:
    bool writeSettings();
    void onVideoFrame(const QVideoFrame& frame);
    void onFrameResult(FrameResult result);
    void onSettingsChanged();
    void onCamerasChanged();
    void openCamera(const QCameraDevice& device);
    void closeCamera();
    // Closes the camera and schedules another try; tracking stays on.
    void cameraFailed(const QString& reason);
    void retryCamera();
    void clearLiveState();
    void profileStructureChanged();
    // Marks the profiles unsaved and starts the auto-save timer if it's on.
    void profilesEdited();
    void setProfilesDirty(bool dirty);
    void sendProfileToWorker(bool keep_state);
    void sendWorkerSettings();
    // The worker changed the head mouse settings: show and save them.
    void onWorkerHeadMouseSettings(const game_face::HeadMouseSettings& settings);
    // Saves the head mouse part of the settings file, leaving other unsaved
    // changes unsaved.
    void saveHeadMouseSettings();
    void startFrames();
    void applySoundSettings();
    void setStatus(const QString& message);
    void setFaceFound(bool found);
    QCameraDevice chooseCamera() const;
    std::filesystem::path configFile(const char* name) const;

    template <class F>
    void onWorker(F&& f)
    {
        QMetaObject::invokeMethod(worker_, std::forward<F>(f), Qt::QueuedConnection);
    }

    SettingsObject* settings_;
    game_face::ProfileSet profileSet_;
    ProfileListModel* profileModel_;
    BindingListModel* bindingModel_;
    BlendshapeModel* blendshapeModel_;
    SoundPlayer* soundPlayer_;

    QMediaDevices mediaDevices_;
    QMediaCaptureSession session_;
    std::unique_ptr<QCamera> camera_;
    // While tracking, the camera to use. Recovery reopens only this one, never
    // another camera, which could be an infrared sensor.
    QByteArray cameraId_;
    // Restarted by every frame; fires when the camera stops sending them.
    QTimer stallTimer_;
    QTimer retryTimer_;
    int retryDelayMs_ = 0;
    // The camera failed and has not sent a frame since.
    bool recovering_ = false;
    QPointer<QObject> videoOutput_;
    QMetaObject::Connection frameConnection_;

    QThread workerThread_;
    TrackingWorker* worker_;
    bool workerBusy_ = false;

    bool initialized_ = false;
    bool tracking_ = false;
    bool faceFound_ = false;
    bool profilesDirty_ = false;
    QString statusMessage_;
    std::vector<game_face::Landmark> landmarks_;
    QTimer saveSettingsTimer_;
    QTimer saveProfilesTimer_;

    FramePlayer framePlayer_;
    QVariantList headMouseAxes_;
    bool headMouseCalibrating_ = false;
    QPointF headMousePuck_;
};
