#pragma once

#include "blendshapes.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace game_face {

struct FaceFrame;

// Head rotation and position, from the user's point of view.
struct HeadPose {
    double yaw = 0;    // degrees; turning to the user's right is positive
    double pitch = 0;  // degrees; tilting up is positive
    double roll = 0;   // degrees; tilting toward the right shoulder is positive
    double x = 0;      // cm (approx.); moving to the user's right is positive
    double y = 0;      // cm (approx.); moving up is positive
    double z = 0;      // cm (approx.); moving toward the camera is positive
};

// Converts MediaPipe's facial transformation matrix (4x4, column-major, see
// mediapipe_c_api.h) to a pose. The matrix maps the canonical face model into
// camera space: x to the image's right, y up, z toward the camera. The camera
// faces the user, so the user's right is the camera's left; the signs here
// turn camera directions into the user's. Rotation is decomposed as
// Ry(-yaw) * Rx(-pitch) * Rz(roll). z is the face's camera-space z, which is
// minus its distance from the camera, so only changes in it are meaningful.
HeadPose headPoseFromMatrix(std::span<const float, 16> column_major);

// The inverse, for tests and the face renderer: the matrix that
// headPoseFromMatrix turns back into `pose`.
std::array<float, 16> matrixFromHeadPose(const HeadPose& pose);

// Signals the head mouse and advanced bindings can use besides the
// blendshapes. noseX/noseY are the nose tip's position in the image (0..1),
// mirrored so the user's right and up are positive.
inline constexpr std::array<std::string_view, 8> kHeadSignalNames = {
    "headYaw", "headPitch", "headRoll", "headX", "headY", "headZ", "noseX", "noseY",
};

// Every signal: the blendshapes (0..100) in kBlendshapeNames order, then the
// head signals. A SignalValues array is indexed the same way.
inline constexpr std::size_t kSignalCount = kBlendshapeNames.size() + kHeadSignalNames.size();
inline constexpr std::size_t kHeadSignalOffset = kBlendshapeNames.size();

inline constexpr std::array<std::string_view, kSignalCount> kSignalNames = [] {
    std::array<std::string_view, kSignalCount> names{};
    std::size_t i = 0;
    for (auto name : kBlendshapeNames)
        names[i++] = name;
    for (auto name : kHeadSignalNames)
        names[i++] = name;
    return names;
}();

using SignalValues = std::array<double, kSignalCount>;

enum class HeadSignal : std::size_t { Yaw, Pitch, Roll, X, Y, Z, NoseX, NoseY };

inline constexpr std::size_t signalIndex(HeadSignal signal)
{
    return kHeadSignalOffset + static_cast<std::size_t>(signal);
}

std::optional<std::size_t> signalIndex(std::string_view name);

// MediaPipe's nose tip landmark.
inline constexpr std::size_t kNoseTipLandmark = 1;

// The blendshape scores (as toScores) and the head signals of a frame. Head
// signals are 0 when the frame has no pose or landmarks.
SignalValues toSignals(const FaceFrame& frame);

// Copies the blendshape scores into signals, with the head signals 0.
SignalValues toSignals(const BlendshapeScores& scores);

} // namespace game_face
