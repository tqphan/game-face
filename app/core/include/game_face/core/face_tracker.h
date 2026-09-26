#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace game_face {

struct TrackerError {
    std::string message;
};

template <class T>
using TrackerResult = std::expected<T, TrackerError>;

struct FaceTrackerOptions {
    enum class Mode { Image, Video };

    std::filesystem::path library_path;  // libmediapipe.{dll,so,dylib}
    std::filesystem::path model_path;    // face_landmarker.task
    Mode mode = Mode::Video;
    float min_face_detection_confidence = 0.5f;
    float min_face_presence_confidence = 0.5f;
    float min_tracking_confidence = 0.5f;
};

// Tightly packed or strided 8-bit RGB pixels. Not owned.
struct RgbImageView {
    const std::uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;  // bytes per row, >= width * 3
};

struct Blendshape {
    std::string name;  // MediaPipe category name, e.g. "jawOpen"
    float score;       // 0..1
};

struct Landmark {
    float x;  // normalised to image width
    float y;  // normalised to image height
    float z;
};

// Results for the first detected face.
struct FaceFrame {
    bool face_found = false;
    std::vector<Blendshape> blendshapes;
    std::vector<Landmark> landmarks;  // 478 when a face is found
};

// Wraps a MediaPipe face landmarker from libmediapipe, loaded at runtime.
// Not thread-safe: create and use each instance on one thread.
class FaceTracker {
public:
    static TrackerResult<std::unique_ptr<FaceTracker>> create(const FaceTrackerOptions& options);
    ~FaceTracker();

    FaceTracker(const FaceTracker&) = delete;
    FaceTracker& operator=(const FaceTracker&) = delete;

    // In Video mode timestamps must increase; a timestamp that doesn't is
    // bumped to one past the previous one. Ignored in Image mode.
    TrackerResult<FaceFrame> detect(const RgbImageView& image, std::int64_t timestamp_ms);

private:
    struct Impl;
    explicit FaceTracker(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace game_face
