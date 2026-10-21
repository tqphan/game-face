#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace game_face {

enum class HeadMouseMode {
    Relative,  // movement follows changes in the source, like a trackpad
    Joystick,  // speed follows the source's offset from neutral
    Absolute,  // the offset from neutral picks a screen position (pointer only)
};

std::string_view headMouseModeName(HeadMouseMode mode);
std::optional<HeadMouseMode> headMouseModeFromString(std::string_view text);

// One output axis (pointer X/Y, scroll X/Y). JSON keys under
// head.mouse.<group>.<x|y>.
struct HeadMouseAxis {
    // An expression over the signal names (kSignalNames), e.g. "headYaw" or
    // "headYaw + 2 * headX". Empty turns the axis off.
    std::string source;
    // The source's value when centered. Unset until Recenter or the first frame
    // with a face captures it.
    std::optional<double> neutral;
    // How far the source moves from neutral, below and above it, for a full
    // deflection (normalised value -1 or 1). In the source's units, > 0.
    double range_negative = 20;
    double range_positive = 20;
    double gain = 1;  // sensitivity multiplier, 0.05..10
    bool invert = false;
};

// Pointer or scroll. Pointer X is right, pointer Y is up, scroll Y is up
// (away from the user) and scroll X is right; the worker converts pointer Y to
// screen coordinates.
struct HeadMouseGroup {
    HeadMouseMode mode = HeadMouseMode::Relative;
    // Normalised units, 0..0.9. Joystick and absolute: offsets from neutral
    // smaller than this count as centered. Relative: how far the head must
    // move before the pointer follows, which keeps tracking noise out.
    // The defaults are tuned with tools/head-mouse-bench.
    double deadzone = 0.06;
    double smoothing = 0.6;  // 0 (none) .. 1 (most)
    HeadMouseAxis x;
    HeadMouseAxis y;
};

struct HeadMouseSettings {
    bool enabled = false;
    // After a binding presses or clicks a mouse button, the pointer holds
    // still this long, so the expression that clicks doesn't move it. 0 is off.
    double click_freeze_ms = 150;
    HeadMouseGroup pointer = defaultPointer();
    HeadMouseGroup scroll = defaultScroll();

    static HeadMouseGroup defaultPointer();
    static HeadMouseGroup defaultScroll();
};

// Ready-made sources and ranges for a group's two axes.
struct HeadMousePreset {
    std::string_view id;  // stable, used by the UI
    std::string_view x_source;
    std::string_view y_source;
    double x_range;
    double y_range;
};

inline constexpr std::array<HeadMousePreset, 4> kHeadMousePresets = {{
    {"headTurn", "headYaw", "headPitch", 20, 15},     // degrees
    {"headPosition", "headX", "headY", 5, 4},         // cm
    {"nosePoint", "noseX", "noseY", 0.12, 0.12},      // fraction of the frame
    {"tiltAndNod", "headRoll", "headPitch", 15, 15},  // degrees
}};

const HeadMousePreset* findHeadMousePreset(std::string_view id);
// Sets the group's sources and symmetric ranges from the preset and clears
// their neutral (a new source has a different center).
void applyHeadMousePreset(HeadMouseGroup& group, const HeadMousePreset& preset);
// The preset whose sources the group uses, or empty for custom sources.
std::string_view headMousePresetId(const HeadMouseGroup& group);

} // namespace game_face
