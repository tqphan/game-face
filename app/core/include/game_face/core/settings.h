#pragma once

#include "head_mouse_settings.h"
#include "result.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace game_face {

// One group of face mesh lines (new). JSON keys: mesh.<group>.color, a
// "#rrggbb" or "#aarrggbb" string, and mesh.<group>.visible.
struct MeshStyle {
    std::string color;
    bool visible = true;
};

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
    // settle.on.stop (new): when tracking stops or the camera is interrupted,
    // run the bindings once as if every blendshape dropped to 0.
    bool settle_on_stop = false;
    // sound.start / sound.stop (new): played whenever any binding's start or
    // stop fires; a name from assets/sounds without the extension, or empty
    // for none.
    std::string start_sound;
    std::string stop_sound;
    double sound_volume = 0.5;                // sound.volume (new), 0..1
    // Face mesh drawn over the camera preview. Defaults are the original
    // app's colours, using its right-side colour for both sides.
    MeshStyle mesh_tessellation{"#46ffffff"};  // faint white
    MeshStyle mesh_face_oval{"#12ff32"};
    MeshStyle mesh_lips{"#0b006f"};
    MeshStyle mesh_eyes{"#ff06bd"};
    MeshStyle mesh_eyebrows{"#baff98"};
    MeshStyle mesh_irises{"#ff3030"};
    double mesh_contour_width = 1;            // mesh.contour.width, px, 1..6
    double mesh_tessellation_width = 1;       // mesh.tessellation.width, px, 0.5..3
    double text_scale = 1.25;                 // text.scale (new), 1..2: every font in the app
    // Head mouse (new), under head.mouse.*: head.mouse.enabled,
    // head.mouse.click.freeze, head.mouse.<pointer|scroll>.{mode,deadzone,smoothing}
    // and head.mouse.<group>.<x|y>.{source,neutral,range.negative,range.positive,gain,invert}.
    HeadMouseSettings head_mouse;
};

// Missing or malformed values keep their defaults; unknown keys are ignored.
Result<Settings> settingsFromJson(std::string_view json);
std::string settingsToJson(const Settings& settings);

Result<Settings> loadSettings(const std::filesystem::path& path);
Result<void> saveSettings(const Settings& settings, const std::filesystem::path& path);

} // namespace game_face
