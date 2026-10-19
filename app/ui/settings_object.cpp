#include "settings_object.h"

void SettingsObject::load(const game_face::Settings& s)
{
    theme = QString::fromStdString(s.theme);
    language = QString::fromStdString(s.language);
    cameraId = QString::fromStdString(s.camera_id);
    webcamOpacity = s.webcam_opacity;
    landmarksOpacity = s.landmarks_opacity;
    detectionConfidence = s.detection_confidence;
    presenceConfidence = s.presence_confidence;
    trackingConfidence = s.tracking_confidence;
    autoStartTracking = s.auto_start_tracking;
    allowInputSimulation = s.allow_input_simulation;
    autoSaveSettings = s.auto_save_settings;
    autoSaveProfiles = s.auto_save_profiles;
    lockUi = s.lock_ui;
    confirmOnClose = s.confirm_on_close;
    settleOnStop = s.settle_on_stop;
    startSound = QString::fromStdString(s.start_sound);
    stopSound = QString::fromStdString(s.stop_sound);
    soundVolume = s.sound_volume;
    meshTessellationColor = QString::fromStdString(s.mesh_tessellation.color);
    meshTessellationVisible = s.mesh_tessellation.visible;
    meshFaceOvalColor = QString::fromStdString(s.mesh_face_oval.color);
    meshFaceOvalVisible = s.mesh_face_oval.visible;
    meshLipsColor = QString::fromStdString(s.mesh_lips.color);
    meshLipsVisible = s.mesh_lips.visible;
    meshEyesColor = QString::fromStdString(s.mesh_eyes.color);
    meshEyesVisible = s.mesh_eyes.visible;
    meshEyebrowsColor = QString::fromStdString(s.mesh_eyebrows.color);
    meshEyebrowsVisible = s.mesh_eyebrows.visible;
    meshIrisesColor = QString::fromStdString(s.mesh_irises.color);
    meshIrisesVisible = s.mesh_irises.visible;
    meshContourWidth = s.mesh_contour_width;
    meshTessellationWidth = s.mesh_tessellation_width;
    autoStartWithOs = s.auto_start_with_os;
    runOnSecureDesktop = s.run_on_secure_desktop;
    emit changed();
}

game_face::Settings SettingsObject::toSettings() const
{
    game_face::Settings s;
    s.theme = theme.toStdString();
    s.language = language.toStdString();
    s.camera_id = cameraId.toStdString();
    s.webcam_opacity = webcamOpacity;
    s.landmarks_opacity = landmarksOpacity;
    s.detection_confidence = detectionConfidence;
    s.presence_confidence = presenceConfidence;
    s.tracking_confidence = trackingConfidence;
    s.auto_start_tracking = autoStartTracking;
    s.allow_input_simulation = allowInputSimulation;
    s.auto_save_settings = autoSaveSettings;
    s.auto_save_profiles = autoSaveProfiles;
    s.lock_ui = lockUi;
    s.confirm_on_close = confirmOnClose;
    s.settle_on_stop = settleOnStop;
    s.start_sound = startSound.toStdString();
    s.stop_sound = stopSound.toStdString();
    s.sound_volume = soundVolume;
    s.mesh_tessellation = {meshTessellationColor.toStdString(), meshTessellationVisible};
    s.mesh_face_oval = {meshFaceOvalColor.toStdString(), meshFaceOvalVisible};
    s.mesh_lips = {meshLipsColor.toStdString(), meshLipsVisible};
    s.mesh_eyes = {meshEyesColor.toStdString(), meshEyesVisible};
    s.mesh_eyebrows = {meshEyebrowsColor.toStdString(), meshEyebrowsVisible};
    s.mesh_irises = {meshIrisesColor.toStdString(), meshIrisesVisible};
    s.mesh_contour_width = meshContourWidth;
    s.mesh_tessellation_width = meshTessellationWidth;
    s.auto_start_with_os = autoStartWithOs;
    s.run_on_secure_desktop = runOnSecureDesktop;
    return s;
}

void SettingsObject::resetMeshStyle()
{
    const game_face::Settings defaults;
    meshTessellationColor = QString::fromStdString(defaults.mesh_tessellation.color);
    meshTessellationVisible = defaults.mesh_tessellation.visible;
    meshFaceOvalColor = QString::fromStdString(defaults.mesh_face_oval.color);
    meshFaceOvalVisible = defaults.mesh_face_oval.visible;
    meshLipsColor = QString::fromStdString(defaults.mesh_lips.color);
    meshLipsVisible = defaults.mesh_lips.visible;
    meshEyesColor = QString::fromStdString(defaults.mesh_eyes.color);
    meshEyesVisible = defaults.mesh_eyes.visible;
    meshEyebrowsColor = QString::fromStdString(defaults.mesh_eyebrows.color);
    meshEyebrowsVisible = defaults.mesh_eyebrows.visible;
    meshIrisesColor = QString::fromStdString(defaults.mesh_irises.color);
    meshIrisesVisible = defaults.mesh_irises.visible;
    meshContourWidth = defaults.mesh_contour_width;
    meshTessellationWidth = defaults.mesh_tessellation_width;
    emit changed();
}
