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
    s.auto_start_with_os = autoStartWithOs;
    s.run_on_secure_desktop = runOnSecureDesktop;
    return s;
}
