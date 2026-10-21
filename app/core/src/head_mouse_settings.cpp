#include "game_face/core/head_mouse_settings.h"

namespace game_face {

std::string_view headMouseModeName(HeadMouseMode mode)
{
    switch (mode) {
    case HeadMouseMode::Relative: return "relative";
    case HeadMouseMode::Joystick: return "joystick";
    case HeadMouseMode::Absolute: return "absolute";
    }
    return "relative";
}

std::optional<HeadMouseMode> headMouseModeFromString(std::string_view text)
{
    for (auto mode : {HeadMouseMode::Relative, HeadMouseMode::Joystick, HeadMouseMode::Absolute}) {
        if (headMouseModeName(mode) == text)
            return mode;
    }
    return std::nullopt;
}

HeadMouseGroup HeadMouseSettings::defaultPointer()
{
    HeadMouseGroup group;
    group.mode = HeadMouseMode::Relative;
    applyHeadMousePreset(group, kHeadMousePresets[0]);
    return group;
}

HeadMouseGroup HeadMouseSettings::defaultScroll()
{
    // Off until the user gives an axis a source; holding a tilt to keep
    // scrolling is what joystick mode does.
    HeadMouseGroup group;
    group.mode = HeadMouseMode::Joystick;
    group.x.range_negative = group.x.range_positive = 15;
    group.y.range_negative = group.y.range_positive = 15;
    return group;
}

const HeadMousePreset* findHeadMousePreset(std::string_view id)
{
    for (const auto& preset : kHeadMousePresets) {
        if (preset.id == id)
            return &preset;
    }
    return nullptr;
}

void applyHeadMousePreset(HeadMouseGroup& group, const HeadMousePreset& preset)
{
    group.x.source = std::string(preset.x_source);
    group.y.source = std::string(preset.y_source);
    group.x.range_negative = group.x.range_positive = preset.x_range;
    group.y.range_negative = group.y.range_positive = preset.y_range;
    group.x.neutral.reset();
    group.y.neutral.reset();
}

std::string_view headMousePresetId(const HeadMouseGroup& group)
{
    for (const auto& preset : kHeadMousePresets) {
        if (group.x.source == preset.x_source && group.y.source == preset.y_source)
            return preset.id;
    }
    return {};
}

} // namespace game_face
