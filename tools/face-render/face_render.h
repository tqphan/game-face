#pragma once

#include <game_face/core/result.h>
#include <game_face/core/signals.h>

#include <QColor>
#include <QImage>
#include <QSize>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

// Renders a textured 3D head at a known pose, in software, so face tracking
// can be tested without a camera or a person (docs/head-mouse-plan.md, 8.2).
// No GPU: the same pixels on every machine, including CI.
namespace face_render {

using game_face::HeadPose;
using game_face::Result;

// A pinhole camera at the origin looking down -z, like MediaPipe's (its face
// geometry assumes a 63 degree vertical field of view).
struct Camera {
    QSize size{640, 480};
    double vertical_fov_deg = 63;
    int supersample = 2;  // renders this much larger, then scales down
};

struct Look {
    QColor background{118, 122, 128};
    double brightness = 1.0;  // scales the lighting
    // Light direction, toward the light, in camera space (x right, y up, z
    // toward the camera); normalised when used.
    double light_x = -0.4, light_y = 0.5, light_z = 1.0;
    double ambient = 0.45;
};

// The head is placed this far in front of the camera when pose.z is 0;
// pose.z moves it toward the camera from there.
inline constexpr double kDefaultDistanceCm = 50;

class HeadModel {
public:
    // A binary glTF (.glb) with POSITION, NORMAL and TEXCOORD_0, and its
    // base colour texture. Only what the Lee Perry-Smith scan uses is read.
    static Result<HeadModel> load(const std::filesystem::path& glb, const std::filesystem::path& texture);

    struct Vertex {
        float x, y, z;     // model space, cm, face toward +z, origin mid-head
        float nx, ny, nz;
        float u, v;
    };
    const std::vector<Vertex>& vertices() const { return vertices_; }
    const std::vector<std::uint32_t>& indices() const { return indices_; }
    const QImage& texture() const { return texture_; }

private:
    std::vector<Vertex> vertices_;
    std::vector<std::uint32_t> indices_;
    QImage texture_;
};

// The head at `pose` (user's point of view, as game_face::HeadPose; z is
// relative to kDefaultDistanceCm), or just the background when there's no
// pose. RGB888.
QImage render(const HeadModel& model, const std::optional<HeadPose>& pose, const Camera& camera = {},
              const Look& look = {});

// Camera-like flaws, applied in this order.
struct Degrade {
    double brightness = 1.0;    // < 1 is dim lighting
    double blur_px = 0;         // box blur radius
    double noise_sigma = 0;     // Gaussian noise, in 0..255 levels
    int jpeg_quality = 0;       // 1..100 recompresses; 0 doesn't
    std::uint32_t seed = 1;
};
QImage degrade(const QImage& image, const Degrade& flaws);

// A head motion script, e.g.
//   {"fps": 30, "start": {"yaw": 0},
//    "steps": [{"hold": 0.5}, {"to": {"yaw": 20}, "over": 1}, {"hide": 0.5}]}
// "to" moves to the given pose values (others keep theirs) with an ease-in-out
// over "over" seconds; "hold" keeps still; "hide" removes the face (the pose
// is kept for when it's back). Pose keys: yaw pitch roll x y z.
struct MotionFrame {
    double time_s = 0;
    std::optional<HeadPose> pose;  // nullopt: no face in this frame
};
struct Motion {
    double fps = 30;
    std::vector<MotionFrame> frames;
};
Result<Motion> motionFromJson(std::string_view json);

} // namespace face_render
