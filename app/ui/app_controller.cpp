#include "app_controller.h"

#include <game_face/core/settings.h>

#include <QCameraDevice>

#include <algorithm>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QVideoSink>

namespace {

constexpr int kAutoSaveDelayMs = 1000;
constexpr int kMaxTrackingWidth = 640;

#if defined(Q_OS_WIN)
constexpr auto kMediaPipeLibrary = "libmediapipe.dll";
#elif defined(Q_OS_MACOS)
constexpr auto kMediaPipeLibrary = "libmediapipe.dylib";
#else
constexpr auto kMediaPipeLibrary = "libmediapipe.so";
#endif

std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , settings_(new SettingsObject(this))
    , profileModel_(new ProfileListModel(&profileSet_, this))
    , bindingModel_(new BindingListModel(&profileSet_, this))
    , blendshapeModel_(new BlendshapeModel(this))
    , worker_(new TrackingWorker)
{
    worker_->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

    // Worker callbacks run on the worker thread; hop to this thread.
    worker_->onFrame = [this](FrameResult result) {
        QMetaObject::invokeMethod(this, [this, r = std::move(result)]() mutable { onFrameResult(std::move(r)); },
                                  Qt::QueuedConnection);
    };
    worker_->onCommand = [this](QString command, bool injected) {
        QMetaObject::invokeMethod(this, [this, command, injected] {
            lastCommand_ = injected ? command : tr("%1 (not sent: input simulation is off)").arg(command);
            emit lastCommandChanged();
        }, Qt::QueuedConnection);
    };
    worker_->onError = [this](QString message) {
        QMetaObject::invokeMethod(this, [this, message] { setStatus(message); }, Qt::QueuedConnection);
    };
    workerThread_.setObjectName("tracking");
    workerThread_.start();

    connect(settings_, &SettingsObject::changed, this, &AppController::onSettingsChanged);
    connect(bindingModel_, &BindingListModel::edited, this, [this] {
        sendProfileToWorker(true);
        if (settings_->autoSaveProfiles)
            saveProfilesTimer_.start();
    });
    connect(&mediaDevices_, &QMediaDevices::videoInputsChanged, this, &AppController::onCamerasChanged);

    saveSettingsTimer_.setSingleShot(true);
    saveSettingsTimer_.setInterval(kAutoSaveDelayMs);
    connect(&saveSettingsTimer_, &QTimer::timeout, this, &AppController::writeSettings);
    saveProfilesTimer_.setSingleShot(true);
    saveProfilesTimer_.setInterval(kAutoSaveDelayMs);
    connect(&saveProfilesTimer_, &QTimer::timeout, this, &AppController::saveProfiles);
}

AppController::~AppController()
{
    shutdown();
    workerThread_.quit();
    workerThread_.wait();
}

std::filesystem::path AppController::configFile(const char* name) const
{
    return toPath(configFolder() + "/" + QLatin1String(name));
}

QString AppController::configFolder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

void AppController::initialize()
{
    // Missing files are normal on first run; anything else is reported.
    const auto settingsPath = configFile("settings.json");
    if (std::filesystem::exists(settingsPath)) {
        auto loaded = game_face::loadSettings(settingsPath);
        if (loaded)
            settings_->load(*loaded);
        else
            setStatus(QString::fromStdString(loaded.error().message));
    } else {
        settings_->load(game_face::Settings{});
    }

    const auto profilesPath = configFile("profiles.json");
    if (std::filesystem::exists(profilesPath)) {
        auto loaded = game_face::loadProfiles(profilesPath);
        if (loaded)
            profileSet_ = std::move(*loaded);
        else
            setStatus(QString::fromStdString(loaded.error().message));
    }
    profileStructureChanged();
    emit profileIndexChanged();

    initialized_ = true;
    sendInjectionState();
    if (settings_->autoStartTracking)
        startTracking();
}

QStringList AppController::blendshapeNames() const
{
    QStringList names;
    for (auto name : game_face::kBlendshapeNames)
        names << QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
    return names;
}

void AppController::setProfileIndex(int index)
{
    if (index == profileSet_.selection || index < 0 || index >= static_cast<int>(profileSet_.profiles.size()))
        return;
    profileSet_.selection = index;
    bindingModel_->reload();
    sendProfileToWorker(false);
    if (settings_->autoSaveProfiles)
        saveProfilesTimer_.start();
    emit profileIndexChanged();
}

QVariantList AppController::cameras() const
{
    QVariantList list;
    for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
        list << QVariantMap{{"id", QString::fromUtf8(device.id())}, {"name", device.description()}};
    }
    return list;
}

void AppController::setVideoOutput(QObject* output)
{
    if (videoOutput_ == output)
        return;
    videoOutput_ = output;
    session_.setVideoOutput(output);
    disconnect(frameConnection_);
    if (QVideoSink* sink = session_.videoSink())
        frameConnection_ = connect(sink, &QVideoSink::videoFrameChanged, this, &AppController::onVideoFrame);
    emit videoOutputChanged();
}

QCameraDevice AppController::chooseCamera() const
{
    const QByteArray wanted = settings_->cameraId.toUtf8();
    for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
        if (device.id() == wanted)
            return device;
    }
    return QMediaDevices::defaultVideoInput();
}

void AppController::toggleTracking()
{
    if (tracking_)
        stopTracking();
    else
        startTracking();
}

void AppController::startTracking()
{
    if (tracking_)
        return;
    const QCameraDevice device = chooseCamera();
    if (device.isNull()) {
        setStatus(tr("No camera found."));
        return;
    }

    const QString dir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_MACOS)
    // Contents/MacOS holds only the executable; see game_face_deploy_mediapipe.
    const QString libraryDir = dir + "/../Frameworks";
    const QString modelDir = dir + "/../Resources";
#else
    const QString libraryDir = dir;
    const QString modelDir = dir;
#endif
    TrackerConfig config;
    config.library_path = toPath(libraryDir + "/" + kMediaPipeLibrary);
    config.model_path = toPath(modelDir + "/face_landmarker.task");
    config.detection_confidence = static_cast<float>(settings_->detectionConfidence);
    config.presence_confidence = static_cast<float>(settings_->presenceConfidence);
    config.tracking_confidence = static_cast<float>(settings_->trackingConfidence);

    // Loading the model takes tens of milliseconds; wait for it so a failure
    // is reported before the camera turns on.
    QString error;
    QMetaObject::invokeMethod(worker_, [w = worker_, config] { return w->start(config); },
                              Qt::BlockingQueuedConnection, &error);
    if (!error.isEmpty()) {
        setStatus(tr("Face tracking could not start: %1").arg(error));
        return;
    }
    sendProfileToWorker(false);

    camera_ = std::make_unique<QCamera>(device);
    connect(camera_.get(), &QCamera::errorOccurred, this, [this](QCamera::Error, const QString& message) {
        setStatus(tr("Camera error: %1").arg(message));
        stopTracking();
    });
    session_.setCamera(camera_.get());
    camera_->start();

    tracking_ = true;
    setStatus({});
    emit trackingChanged();
}

void AppController::stopTracking()
{
    if (!tracking_)
        return;
    tracking_ = false;
    if (camera_)
        camera_->stop();
    session_.setCamera(nullptr);
    camera_.reset();
    // The video output keeps showing its last frame; replace it with nothing.
    if (QVideoSink* sink = session_.videoSink())
        sink->setVideoFrame(QVideoFrame());
    onWorker([w = worker_] { w->stop(); });

    landmarks_.clear();
    setFaceFound(false);
    bindingModel_->clearLiveState();
    blendshapeModel_->setScores({});
    emit landmarksChanged();
    emit trackingChanged();
}

void AppController::onVideoFrame(const QVideoFrame& frame)
{
    // One frame in flight: frames arriving while the worker is busy are dropped.
    if (!tracking_ || workerBusy_ || !frame.isValid())
        return;

    // Converted here, not on the worker; see TrackingWorker::process.
    // MediaPipe scales frames down to its model input anyway, so a smaller
    // image keeps the conversion cheap.
    QImage image = frame.toImage();
    if (image.isNull())
        return;
    if (image.width() > kMaxTrackingWidth)
        image = image.scaledToWidth(kMaxTrackingWidth, Qt::FastTransformation);
    image = image.convertToFormat(QImage::Format_RGB888);

    workerBusy_ = true;
    onWorker([w = worker_, image = std::move(image)] { w->process(image); });
}

void AppController::onFrameResult(FrameResult result)
{
    workerBusy_ = false;
    if (!tracking_)
        return;
    setFaceFound(result.face_found);
    if (result.face_found) {
        blendshapeModel_->setScores(result.scores);
        landmarks_ = std::move(result.landmarks);
    } else {
        landmarks_.clear();
    }
    bindingModel_->setLiveState(result.status, result.scores);
    emit landmarksChanged();
}

void AppController::onSettingsChanged()
{
    if (!initialized_)
        return;
    sendInjectionState();
    if (settings_->autoSaveSettings)
        saveSettingsTimer_.start();
}

void AppController::onCamerasChanged()
{
    emit camerasChanged();
    if (!tracking_ || !camera_)
        return;
    const QByteArray active = camera_->cameraDevice().id();
    for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
        if (device.id() == active)
            return;
    }
    // Stop rather than silently switching to another camera, which could be
    // an infrared sensor.
    stopTracking();
    setStatus(tr("The camera was disconnected; tracking stopped."));
}

void AppController::sendProfileToWorker(bool keep_state)
{
    game_face::Profile profile;
    if (const game_face::Profile* selected = profileSet_.selected())
        profile = *selected;
    onWorker([w = worker_, profile = std::move(profile), keep_state] { w->setProfile(profile, keep_state); });
}

void AppController::sendInjectionState()
{
    const bool enabled = settings_->allowInputSimulation;
    onWorker([w = worker_, enabled] { w->setInjectionEnabled(enabled); });
}

void AppController::profileStructureChanged()
{
    profileModel_->reload();
    bindingModel_->reload();
    sendProfileToWorker(false);
    if (initialized_ && settings_->autoSaveProfiles)
        saveProfilesTimer_.start();
}

bool AppController::saveSettings()
{
    if (!writeSettings())
        return false;
    // Confidence thresholds only apply when the tracker is created.
    if (tracking_) {
        stopTracking();
        startTracking();
    }
    return true;
}

// Saves without restarting tracking (auto-save, import, quit).
bool AppController::writeSettings()
{
    saveSettingsTimer_.stop();
    auto saved = game_face::saveSettings(settings_->toSettings(), configFile("settings.json"));
    if (!saved) {
        setStatus(QString::fromStdString(saved.error().message));
        return false;
    }
    return true;
}

bool AppController::saveProfiles()
{
    saveProfilesTimer_.stop();
    auto saved = game_face::saveProfiles(profileSet_, configFile("profiles.json"));
    if (!saved) {
        setStatus(QString::fromStdString(saved.error().message));
        return false;
    }
    return true;
}

void AppController::createProfile(const QString& name)
{
    game_face::Profile profile;
    profile.name = name.trimmed().isEmpty() ? tr("New profile").toStdString() : name.trimmed().toStdString();
    profile.bindings.emplace_back();
    profileSet_.profiles.push_back(std::move(profile));
    profileSet_.selection = static_cast<int>(profileSet_.profiles.size()) - 1;
    profileStructureChanged();
    emit profileIndexChanged();
}

void AppController::removeProfile()
{
    if (profileSet_.profiles.empty())
        return;
    const auto index = static_cast<std::size_t>(profileSet_.selection);
    profileSet_.profiles.erase(profileSet_.profiles.begin() + static_cast<std::ptrdiff_t>(index));
    profileSet_.selection = std::max(0, profileSet_.selection - 1);
    profileStructureChanged();
    emit profileIndexChanged();
}

void AppController::addBinding()
{
    if (profileSet_.profiles.empty())
        return;
    profileSet_.profiles[static_cast<std::size_t>(profileSet_.selection)].bindings.emplace_back();
    profileStructureChanged();
}

void AppController::removeBinding(int row)
{
    if (profileSet_.profiles.empty())
        return;
    auto& bindings = profileSet_.profiles[static_cast<std::size_t>(profileSet_.selection)].bindings;
    if (row < 0 || row >= static_cast<int>(bindings.size()))
        return;
    bindings.erase(bindings.begin() + row);
    profileStructureChanged();
}

QString AppController::importLegacy(const QUrl& folder)
{
    const QString dir = folder.toLocalFile();
    const auto profilesPath = toPath(dir + "/user.profiles.json");
    const auto settingsPath = toPath(dir + "/user.settings.json");
    QStringList imported;

    if (std::filesystem::exists(profilesPath)) {
        auto loaded = game_face::loadProfiles(profilesPath);
        if (!loaded)
            return QString::fromStdString(loaded.error().message);
        profileSet_ = std::move(*loaded);
        profileStructureChanged();
        emit profileIndexChanged();
        saveProfiles();
        imported << tr("%n profile(s)", nullptr, static_cast<int>(profileSet_.profiles.size()));
    }
    if (std::filesystem::exists(settingsPath)) {
        auto loaded = game_face::loadSettings(settingsPath);
        if (!loaded)
            return QString::fromStdString(loaded.error().message);
        const QString camera = settings_->cameraId;  // the old file has no usable camera ID
        settings_->load(*loaded);
        settings_->cameraId = camera;
        writeSettings();
        imported << tr("settings");
    }

    if (imported.isEmpty())
        return tr("No user.profiles.json or user.settings.json in %1").arg(QDir::toNativeSeparators(dir));
    return tr("Imported %1.").arg(imported.join(tr(" and ")));
}

void AppController::shutdown()
{
    stopTracking();
    if (!initialized_)
        return;
    if (settings_->autoSaveSettings)
        writeSettings();
    if (settings_->autoSaveProfiles)
        saveProfiles();
    initialized_ = false;
}

void AppController::setStatus(const QString& message)
{
    if (message == statusMessage_)
        return;
    statusMessage_ = message;
    emit statusMessageChanged();
}

void AppController::setFaceFound(bool found)
{
    if (found == faceFound_)
        return;
    faceFound_ = found;
    emit faceFoundChanged();
}
