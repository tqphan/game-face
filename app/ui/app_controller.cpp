#include "app_controller.h"

#include <game_face/core/settings.h>
#include <game_face/core/signals.h>

#include <input_devices_simulator/recording_platform.h>

#include <QCameraDevice>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QVideoSink>

namespace {

constexpr int kAutoSaveDelayMs = 1000;
constexpr int kMaxTrackingWidth = 640;
// Long enough for a camera to start; a driver reset stalls it for longer.
constexpr int kCameraStallMs = 5000;
// Camera retries back off from the first delay to the last.
constexpr int kFirstRetryMs = 1000;
constexpr int kMaxRetryMs = 30000;

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

StartupOptions& startupOptions()
{
    static StartupOptions options;
    return options;
}

TrackingWorker::PlatformFactory inputPlatform()
{
    if (!startupOptions().recordingInput)
        return {};
    // Each event as a line on stdout, e.g. "input: mouse moveBy 5 -3".
    return [] {
        return std::make_unique<input_devices_simulator::RecordingPlatform>([](std::string_view line) {
            std::printf("input: %.*s\n", static_cast<int>(line.size()), line.data());
            std::fflush(stdout);
        });
    };
}

QString fromView(std::string_view s)
{
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

} // namespace

void AppController::setStartupOptions(const StartupOptions& options)
{
    startupOptions() = options;
}

AppController::AppController(QObject* parent)
    : QObject(parent)
    , settings_(new SettingsObject(this))
    , profileModel_(new ProfileListModel(&profileSet_, this))
    , bindingModel_(new BindingListModel(&profileSet_, this))
    , blendshapeModel_(new BlendshapeModel(this))
    , soundPlayer_(new SoundPlayer(this))
    , worker_(new TrackingWorker(inputPlatform()))
{
    worker_->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

    // Worker callbacks run on the worker thread; hop to this thread.
    worker_->onFrame = [this](FrameResult result) {
        QMetaObject::invokeMethod(this, [this, r = std::move(result)]() mutable { onFrameResult(std::move(r)); },
                                  Qt::QueuedConnection);
    };
    worker_->onEdge = [this](game_face::BindingEngine::Edge edge) {
        QMetaObject::invokeMethod(this, [this, edge] { soundPlayer_->play(edge); }, Qt::QueuedConnection);
    };
    worker_->onError = [this](QString message) {
        QMetaObject::invokeMethod(this, [this, message] { setStatus(message); }, Qt::QueuedConnection);
    };
    worker_->onHeadMouseSettings = [this](game_face::HeadMouseSettings settings) {
        QMetaObject::invokeMethod(this, [this, settings] { onWorkerHeadMouseSettings(settings); },
                                  Qt::QueuedConnection);
    };
    workerThread_.setObjectName("tracking");
    workerThread_.start();

    connect(settings_, &SettingsObject::changed, this, &AppController::onSettingsChanged);
    connect(bindingModel_, &BindingListModel::edited, this, [this] {
        sendProfileToWorker(true);
        profilesEdited();
    });
    connect(&mediaDevices_, &QMediaDevices::videoInputsChanged, this, &AppController::onCamerasChanged);

    saveSettingsTimer_.setSingleShot(true);
    saveSettingsTimer_.setInterval(kAutoSaveDelayMs);
    connect(&saveSettingsTimer_, &QTimer::timeout, this, &AppController::writeSettings);
    saveProfilesTimer_.setSingleShot(true);
    saveProfilesTimer_.setInterval(kAutoSaveDelayMs);
    connect(&saveProfilesTimer_, &QTimer::timeout, this, &AppController::saveProfiles);

    stallTimer_.setSingleShot(true);
    stallTimer_.setInterval(kCameraStallMs);
    connect(&stallTimer_, &QTimer::timeout, this,
            [this] { cameraFailed(tr("The camera stopped sending video.")); });
    retryTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this, &AppController::retryCamera);

    // Recorded frames go through the preview's sink like camera frames, so
    // they're shown and tracked the same way.
    connect(&framePlayer_, &FramePlayer::frameReady, this, [this](const QVideoFrame& frame) {
        if (QVideoSink* sink = session_.videoSink())
            sink->setVideoFrame(frame);
        else
            onVideoFrame(frame);
    });
    connect(&framePlayer_, &FramePlayer::finished, this, [this] {
        if (!startupOptions().quitWhenFramesEnd)
            return;
        // Give the worker time to finish the last frame. Shut down first, as
        // closing the window does: stopping tracking touches the video output,
        // which is gone by the time quitting destroys this object.
        QTimer::singleShot(1000, this, [this] {
            shutdown();
            QCoreApplication::quit();
        });
    });
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
    if (!startupOptions().configFolder.isEmpty())
        return startupOptions().configFolder;
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
    sendWorkerSettings();
    applySoundSettings();
    if (settings_->autoStartTracking || startupOptions().startTracking)
        startTracking();
}

QStringList AppController::signalNames() const
{
    QStringList names;
    for (auto name : game_face::kSignalNames)
        names << fromView(name);
    return names;
}

QStringList AppController::headSignalNames() const
{
    QStringList names;
    for (auto name : game_face::kHeadSignalNames)
        names << fromView(name);
    return names;
}

QVariantList AppController::headMousePresets() const
{
    QVariantList list;
    for (const auto& preset : game_face::kHeadMousePresets) {
        const QString id = fromView(preset.id);
        QString name;
        if (id == QLatin1String("headTurn"))
            name = tr("Head turn");
        else if (id == QLatin1String("headPosition"))
            name = tr("Head position");
        else if (id == QLatin1String("nosePoint"))
            name = tr("Nose point");
        else if (id == QLatin1String("tiltAndNod"))
            name = tr("Tilt and nod");
        list << QVariantMap{{"id", id}, {"name", name}, {"x", fromView(preset.x_source)},
                            {"y", fromView(preset.y_source)}};
    }
    return list;
}

double AppController::headMouseDeadzone() const
{
    return settings_->headMouse()->pointer()->deadzone;
}

bool AppController::showHeadMousePuck() const
{
    if (!settings_->headMouse()->enabled || !faceFound_ || headMouseAxes_.size() < 2)
        return false;
    return headMouseAxes_[0].toMap().value("active").toBool() || headMouseAxes_[1].toMap().value("active").toBool();
}

void AppController::recenterHeadMouse()
{
    onWorker([w = worker_] { w->recenterHeadMouse(); });
}

void AppController::calibrateHeadMouse()
{
    onWorker([w = worker_] { w->calibrateHeadMouse(); });
}

void AppController::onWorkerHeadMouseSettings(const game_face::HeadMouseSettings& settings)
{
    // Takes only what the worker changes itself (on/off, centers, ranges), so
    // an edit the worker hasn't received yet isn't undone. Loading sends the
    // result back to the worker.
    game_face::HeadMouseSettings merged = settings_->headMouse()->toSettings();
    merged.enabled = settings.enabled;
    auto take = [](game_face::HeadMouseAxis& to, const game_face::HeadMouseAxis& from) {
        if (to.source != from.source)
            return;  // the source changed since; its old center and range don't apply
        to.neutral = from.neutral;
        to.range_negative = from.range_negative;
        to.range_positive = from.range_positive;
    };
    take(merged.pointer.x, settings.pointer.x);
    take(merged.pointer.y, settings.pointer.y);
    take(merged.scroll.x, settings.scroll.x);
    take(merged.scroll.y, settings.scroll.y);
    settings_->headMouse()->load(merged);
    saveHeadMouseSettings();
}

void AppController::saveHeadMouseSettings()
{
    const auto path = configFile("settings.json");
    game_face::Settings saved = settings_->toSettings();
    if (std::filesystem::exists(path)) {
        auto loaded = game_face::loadSettings(path);
        if (loaded)
            saved = std::move(*loaded);
    }
    saved.head_mouse = settings_->headMouse()->toSettings();
    auto written = game_face::saveSettings(saved, path);
    if (!written)
        setStatus(QString::fromStdString(written.error().message));
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
    profilesEdited();
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
    const bool useFrames = !startupOptions().frames.isEmpty();
    const QCameraDevice device = useFrames ? QCameraDevice() : chooseCamera();
    if (!useFrames && device.isNull()) {
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

    // Set first: opening the camera can report an error, which needs tracking on.
    tracking_ = true;
    retryDelayMs_ = kFirstRetryMs;
    setStatus({});
    if (useFrames) {
        startFrames();
    } else {
        cameraId_ = device.id();
        openCamera(device);
    }
    emit trackingChanged();
}

void AppController::startFrames()
{
    const QString error = framePlayer_.open(startupOptions().frames, startupOptions().framesFps);
    if (!error.isEmpty()) {
        setStatus(error);
        return;
    }
    framePlayer_.start();
}

void AppController::stopTracking()
{
    if (!tracking_)
        return;
    tracking_ = false;
    retryTimer_.stop();
    if (recovering_) {
        recovering_ = false;
        setStatus({});
    }
    closeCamera();
    framePlayer_.stop();
    onWorker([w = worker_] { w->stop(); });
    clearLiveState();
    emit trackingChanged();
}

void AppController::openCamera(const QCameraDevice& device)
{
    camera_ = std::make_unique<QCamera>(device);
    // Queued: cameraFailed deletes the camera, which must not happen while it
    // is emitting. The check skips errors from a camera already replaced.
    connect(camera_.get(), &QCamera::errorOccurred, this,
            [this, camera = camera_.get()](QCamera::Error, const QString& message) {
                if (camera_.get() == camera)
                    cameraFailed(tr("Camera error: %1").arg(message));
            }, Qt::QueuedConnection);
    session_.setCamera(camera_.get());
    camera_->start();
    stallTimer_.start();
}

void AppController::closeCamera()
{
    stallTimer_.stop();
    if (camera_)
        camera_->stop();
    session_.setCamera(nullptr);
    camera_.reset();
    // The video output keeps showing its last frame; replace it with nothing.
    if (QVideoSink* sink = session_.videoSink())
        sink->setVideoFrame(QVideoFrame());
}

void AppController::cameraFailed(const QString& reason)
{
    // Recorded frames have no camera to recover; they just end.
    if (!tracking_ || !startupOptions().frames.isEmpty())
        return;
    closeCamera();
    if (!recovering_) {
        // No frames means no stop triggers; don't leave keys held meanwhile.
        recovering_ = true;
        onWorker([w = worker_] { w->pause(); });
        clearLiveState();
    }
    setStatus(tr("%1 Trying again in %n second(s).", nullptr, retryDelayMs_ / 1000).arg(reason));
    retryTimer_.start(retryDelayMs_);
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryMs);
}

void AppController::retryCamera()
{
    if (!tracking_ || camera_)
        return;
    for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
        if (device.id() == cameraId_) {
            // Recovered once it sends a frame; see onVideoFrame.
            openCamera(device);
            return;
        }
    }
    cameraFailed(tr("The camera is not connected."));
}

void AppController::clearLiveState()
{
    landmarks_.clear();
    setFaceFound(false);
    bindingModel_->clearLiveState();
    blendshapeModel_->setScores({});
    headMouseAxes_.clear();
    headMouseCalibrating_ = false;
    headMousePuck_ = {};
    emit landmarksChanged();
    emit headMouseChanged();
}

void AppController::onVideoFrame(const QVideoFrame& frame)
{
    if (!tracking_ || !frame.isValid())
        return;
    stallTimer_.start();
    if (recovering_) {
        recovering_ = false;
        retryDelayMs_ = kFirstRetryMs;
        setStatus({});
    }

    // One frame in flight: frames arriving while the worker is busy are dropped.
    if (workerBusy_)
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
    // A frame from before the camera failed is stale.
    if (!tracking_ || recovering_)
        return;
    setFaceFound(result.face_found);
    if (result.face_found) {
        blendshapeModel_->setScores(result.scores);
        landmarks_ = std::move(result.landmarks);
    } else {
        landmarks_.clear();
    }
    bindingModel_->setLiveState(result.status, result.scores);

    headMouseAxes_.clear();
    for (const auto& axis : result.head_mouse.axes)
        headMouseAxes_ << QVariantMap{{"active", axis.active}, {"raw", axis.raw}, {"normalised", axis.normalised}};
    headMouseCalibrating_ = result.head_mouse_calibrating;
    headMousePuck_ = QPointF(result.head_mouse.axes[0].normalised, result.head_mouse.axes[1].normalised);
    emit landmarksChanged();
    emit headMouseChanged();
}

void AppController::onSettingsChanged()
{
    if (!initialized_)
        return;
    sendWorkerSettings();
    applySoundSettings();
    if (settings_->autoSaveSettings)
        saveSettingsTimer_.start();
}

void AppController::onCamerasChanged()
{
    emit camerasChanged();
    if (!tracking_)
        return;
    const auto devices = QMediaDevices::videoInputs();
    const bool present = std::any_of(devices.begin(), devices.end(),
                                     [this](const QCameraDevice& d) { return d.id() == cameraId_; });
    if (!present && camera_) {
        // A driver reset can remove the camera for a moment; wait for it.
        cameraFailed(tr("The camera was disconnected."));
    } else if (present && !camera_) {
        // It's back: don't wait out the backoff.
        retryTimer_.stop();
        retryCamera();
    }
}

void AppController::sendProfileToWorker(bool keep_state)
{
    game_face::Profile profile;
    if (const game_face::Profile* selected = profileSet_.selected())
        profile = *selected;
    onWorker([w = worker_, profile = std::move(profile), keep_state] { w->setProfile(profile, keep_state); });
}

void AppController::sendWorkerSettings()
{
    const bool enabled = settings_->allowInputSimulation;
    const bool settle = settings_->settleOnStop;
    const game_face::HeadMouseSettings headMouse = settings_->headMouse()->toSettings();
    // Absolute mode's screen, in physical pixels like the simulator's.
    QRect screen;
    if (const QScreen* primary = QGuiApplication::primaryScreen()) {
        const QRect g = primary->geometry();
        const double ratio = primary->devicePixelRatio();
        screen = QRect(qRound(g.x() * ratio), qRound(g.y() * ratio), qRound(g.width() * ratio),
                       qRound(g.height() * ratio));
    }
    onWorker([w = worker_, enabled, settle, headMouse, screen] {
        w->setInjectionEnabled(enabled);
        w->setSettleOnStop(settle);
        w->setHeadMouseSettings(headMouse);
        w->setScreenGeometry(screen);
    });
}

void AppController::applySoundSettings()
{
    soundPlayer_->configure(settings_->startSound, settings_->stopSound, settings_->soundVolume);
}

void AppController::profileStructureChanged()
{
    profileModel_->reload();
    bindingModel_->reload();
    sendProfileToWorker(false);
    // Loading the profiles at startup isn't an edit.
    if (initialized_)
        profilesEdited();
}

void AppController::profilesEdited()
{
    setProfilesDirty(true);
    if (settings_->autoSaveProfiles)
        saveProfilesTimer_.start();
}

void AppController::setProfilesDirty(bool dirty)
{
    if (dirty == profilesDirty_)
        return;
    profilesDirty_ = dirty;
    emit profilesDirtyChanged();
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

// Saves without restarting tracking (auto-save, quit).
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
    setProfilesDirty(false);
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
