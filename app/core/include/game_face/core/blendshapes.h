#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace game_face {

struct FaceFrame;

// MediaPipe face blendshape names, in model output order.
inline constexpr std::array<std::string_view, 52> kBlendshapeNames = {
    "_neutral",
    "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight",
    "cheekPuff", "cheekSquintLeft", "cheekSquintRight",
    "eyeBlinkLeft", "eyeBlinkRight",
    "eyeLookDownLeft", "eyeLookDownRight", "eyeLookInLeft", "eyeLookInRight",
    "eyeLookOutLeft", "eyeLookOutRight", "eyeLookUpLeft", "eyeLookUpRight",
    "eyeSquintLeft", "eyeSquintRight", "eyeWideLeft", "eyeWideRight",
    "jawForward", "jawLeft", "jawOpen", "jawRight",
    "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight",
    "mouthFunnel", "mouthLeft", "mouthLowerDownLeft", "mouthLowerDownRight",
    "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight",
    "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper",
    "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight",
    "mouthUpperUpLeft", "mouthUpperUpRight",
    "noseSneerLeft", "noseSneerRight",
};

// Scores 0..100 (MediaPipe score * 100, rounded), indexed like kBlendshapeNames.
// This is the scale bindings and expressions use, same as the original app.
using BlendshapeScores = std::array<double, kBlendshapeNames.size()>;

std::optional<std::size_t> blendshapeIndex(std::string_view name);

// Copies a tracker frame's blendshapes into scores. Names the table doesn't
// know are ignored; missing ones stay 0.
BlendshapeScores toScores(const FaceFrame& frame);

} // namespace game_face
