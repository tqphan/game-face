#include "game_face/core/signals.h"

#include "game_face/core/face_tracker.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace game_face {

namespace {

constexpr double kDegrees = 180.0 / std::numbers::pi;

// Row r, column c of a column-major 4x4 matrix.
double at(std::span<const float, 16> m, int r, int c)
{
    return m[static_cast<std::size_t>(c * 4 + r)];
}

using Mat3 = std::array<std::array<double, 3>, 3>;

Mat3 multiply(const Mat3& a, const Mat3& b)
{
    Mat3 out{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < 3; ++k)
                out[r][c] += a[r][k] * b[k][c];
    return out;
}

} // namespace

HeadPose headPoseFromMatrix(std::span<const float, 16> m)
{
    // atan2 of matching entries, so a uniform scale in the matrix cancels out.
    HeadPose pose;
    pose.yaw = -std::atan2(at(m, 0, 2), at(m, 2, 2)) * kDegrees;
    pose.pitch = std::atan2(at(m, 1, 2), std::hypot(at(m, 0, 2), at(m, 2, 2))) * kDegrees;
    pose.roll = std::atan2(at(m, 1, 0), at(m, 1, 1)) * kDegrees;
    pose.x = -at(m, 0, 3);
    pose.y = at(m, 1, 3);
    pose.z = at(m, 2, 3);
    return pose;
}

std::array<float, 16> matrixFromHeadPose(const HeadPose& pose)
{
    const double a = -pose.yaw / kDegrees;
    const double b = -pose.pitch / kDegrees;
    const double c = pose.roll / kDegrees;
    const Mat3 ry{{{std::cos(a), 0, std::sin(a)}, {0, 1, 0}, {-std::sin(a), 0, std::cos(a)}}};
    const Mat3 rx{{{1, 0, 0}, {0, std::cos(b), -std::sin(b)}, {0, std::sin(b), std::cos(b)}}};
    const Mat3 rz{{{std::cos(c), -std::sin(c), 0}, {std::sin(c), std::cos(c), 0}, {0, 0, 1}}};
    const Mat3 r = multiply(multiply(ry, rx), rz);

    std::array<float, 16> m{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            m[static_cast<std::size_t>(col * 4 + row)] = static_cast<float>(r[row][col]);
    m[12] = static_cast<float>(-pose.x);
    m[13] = static_cast<float>(pose.y);
    m[14] = static_cast<float>(pose.z);
    m[15] = 1;
    return m;
}

std::optional<std::size_t> signalIndex(std::string_view name)
{
    for (std::size_t i = 0; i < kSignalNames.size(); ++i) {
        if (kSignalNames[i] == name)
            return i;
    }
    return std::nullopt;
}

SignalValues toSignals(const BlendshapeScores& scores)
{
    SignalValues values{};
    std::copy(scores.begin(), scores.end(), values.begin());
    return values;
}

SignalValues toSignals(const FaceFrame& frame)
{
    SignalValues values = toSignals(toScores(frame));
    if (frame.pose) {
        values[signalIndex(HeadSignal::Yaw)] = frame.pose->yaw;
        values[signalIndex(HeadSignal::Pitch)] = frame.pose->pitch;
        values[signalIndex(HeadSignal::Roll)] = frame.pose->roll;
        values[signalIndex(HeadSignal::X)] = frame.pose->x;
        values[signalIndex(HeadSignal::Y)] = frame.pose->y;
        values[signalIndex(HeadSignal::Z)] = frame.pose->z;
    }
    if (frame.landmarks.size() > kNoseTipLandmark) {
        const Landmark& nose = frame.landmarks[kNoseTipLandmark];
        values[signalIndex(HeadSignal::NoseX)] = 1.0 - nose.x;
        values[signalIndex(HeadSignal::NoseY)] = 1.0 - nose.y;
    }
    return values;
}

} // namespace game_face
