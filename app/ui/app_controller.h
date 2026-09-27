#pragma once

#include "list_models.h"
#include "settings_object.h"
#include "tracking_worker.h"

#include <game_face/core/face_tracker.h>
#include <game_face/core/profiles.h>

#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <memory>
#include <vector>

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
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString lastCommand READ lastCommand NOTIFY lastCommandChanged)
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QObject* videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
    Q_PROPERTY(QString configFolder READ configFolder CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
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
    QString statusMessage() const { return statusMessage_; }
    QString lastCommand() const { return lastCommand_; }
    QVariantList cameras() const;
    QObject* videoOutput() const { return videoOutput_; }
    void setVideoOutput(QObject* output);
    QString configFolder() const;

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
    // Imports user.profiles.json / user.settings.json from the original app.
    // Returns a message for the user.
    Q_INVOKABLE QString importLegacy(const QUrl& folder);
    // Stops tracking and auto-saves. Call before quitting.
    Q_INVOKABLE void shutdown();

signals:
    void profileIndexChanged();
    void trackingChanged();
    void faceFoundChanged();
    void statusMessageChanged();
    void lastCommandChanged();
    void camerasChanged();
    void videoOutputChanged();
    void landmarksChanged();

private:
    bool writeSettings();
    void onVideoFrame(const QVideoFrame& frame);
    void onFrameResult(FrameResult result);
    void onSettingsChanged();
    void onCamerasChanged();
    void profileStructureChanged();
    void sendProfileToWorker(bool keep_state);
    void sendInjectionState();
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

    QMediaDevices mediaDevices_;
    QMediaCaptureSession session_;
    std::unique_ptr<QCamera> camera_;
    QPointer<QObject> videoOutput_;
    QMetaObject::Connection frameConnection_;

    QThread workerThread_;
    TrackingWorker* worker_;
    bool workerBusy_ = false;

    bool initialized_ = false;
    bool tracking_ = false;
    bool faceFound_ = false;
    QString statusMessage_;
    QString lastCommand_;
    std::vector<game_face::Landmark> landmarks_;
    QTimer saveSettingsTimer_;
    QTimer saveProfilesTimer_;
};
