#pragma once

#include "result.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace game_face {

// Application settings. JSON keys are the original app's dotted names
// (shown next to each field) so its user.settings.json can be imported.
struct Settings {
    std::string theme = "dark";               // theme: "dark" | "light"
    std::string language = "en";              // language
    // camera.id: Qt camera ID. The original app's webcam.deviceId is a browser
    // ID that can't be mapped to a Qt camera, so it isn't imported.
    std::string camera_id;
    double webcam_opacity = 1.0;              // webcam.opacity, 0..1
    double landmarks_opacity = 1.0;           // landmarks.opacity, 0..1
    double detection_confidence = 0.5;        // detection.confidence, 0..1
    double presence_confidence = 0.5;         // presence.confidence, 0..1
    double tracking_confidence = 0.5;         // tracking.confidence, 0..1
    bool auto_start_tracking = false;         // auto.start.prediction
    bool auto_start_with_os = false;          // auto.start.with.windows
    bool run_on_secure_desktop = false;       // run.on.secured.desktop
    // allow.input.simulation: the original app showed this but ignored it;
    // game-face injects input only when it is on.
    bool allow_input_simulation = false;
    bool auto_save_settings = false;          // auto.save.settings
    bool auto_save_profiles = false;          // auto.save.profiles
    bool lock_ui = false;                     // lock.ui
    bool confirm_on_close = true;             // confirm.on.close (new in game-face 2)
};

// Missing or malformed values keep their defaults; unknown keys are ignored.
Result<Settings> settingsFromJson(std::string_view json);
std::string settingsToJson(const Settings& settings);

Result<Settings> loadSettings(const std::filesystem::path& path);
Result<void> saveSettings(const Settings& settings, const std::filesystem::path& path);

} // namespace game_face
