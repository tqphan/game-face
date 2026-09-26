#include "game_face/core/face_tracker.h"

#include "dynamic_library.h"

#include <mediapipe_c_api.h>

#include <cstring>

namespace game_face {

namespace {

int hostSystem()
{
#if defined(_WIN32)
    return kMpHostSystemWindows;
#elif defined(__APPLE__)
    return kMpHostSystemMac;
#elif defined(__linux__)
    return kMpHostSystemLinux;
#else
    return kMpHostSystemUnknown;
#endif
}

} // namespace

struct FaceTracker::Impl {
    detail::DynamicLibrary library;

    MpFaceLandmarkerCreateFn create = nullptr;
    MpFaceLandmarkerDetectImageFn detectImage = nullptr;
    MpFaceLandmarkerDetectForVideoFn detectForVideo = nullptr;
    MpFaceLandmarkerCloseResultFn closeResult = nullptr;
    MpFaceLandmarkerCloseFn close = nullptr;
    MpImageCreateFromUint8DataFn createImage = nullptr;
    MpImageFreeFn freeImage = nullptr;
    MpErrorFreeFn freeError = nullptr;

    MpFaceLandmarkerPtr landmarker = nullptr;
    FaceTrackerOptions::Mode mode = FaceTrackerOptions::Mode::Video;
    std::int64_t last_timestamp = -1;
    std::vector<std::uint8_t> packed;  // reused buffer for strided images

    ~Impl()
    {
        if (landmarker && close) {
            char* error = nullptr;
            close(landmarker, &error);
            freeErrorMessage(error);
        }
    }

    // Takes ownership of a C API error string.
    std::string takeError(char* error, const char* context) const
    {
        std::string message = context;
        if (error) {
            message += ": ";
            message += error;
            freeErrorMessage(error);
        }
        return message;
    }

    void freeErrorMessage(char* error) const
    {
        if (error && freeError)
            freeError(error);
    }

    std::string load(const std::filesystem::path& path)
    {
        if (auto error = library.open(path); !error.empty())
            return "could not load " + path.string() + ": " + error;

        const bool ok = library.resolve(create, "MpFaceLandmarkerCreate")
            && library.resolve(detectImage, "MpFaceLandmarkerDetectImage")
            && library.resolve(detectForVideo, "MpFaceLandmarkerDetectForVideo")
            && library.resolve(closeResult, "MpFaceLandmarkerCloseResult")
            && library.resolve(close, "MpFaceLandmarkerClose")
            && library.resolve(createImage, "MpImageCreateFromUint8Data")
            && library.resolve(freeImage, "MpImageFree")
            && library.resolve(freeError, "MpErrorFree");
        if (!ok)
            return path.string() + " is missing MediaPipe C API functions (version mismatch?)";
        return {};
    }
};

FaceTracker::FaceTracker(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

FaceTracker::~FaceTracker() = default;

TrackerResult<std::unique_ptr<FaceTracker>> FaceTracker::create(const FaceTrackerOptions& options)
{
    auto impl = std::make_unique<Impl>();
    if (auto error = impl->load(options.library_path); !error.empty())
        return std::unexpected(TrackerError{error});
    if (!std::filesystem::exists(options.model_path))
        return std::unexpected(TrackerError{"model not found: " + options.model_path.string()});

    const std::string model_path = options.model_path.string();

    MpFaceLandmarkerOptions mp{};
    mp.base_options.model_asset_path = model_path.c_str();
    mp.base_options.file_descriptor = -1;
    mp.base_options.delegate = kMpDelegateCpu;
    mp.base_options.host_system = hostSystem();
    mp.running_mode = options.mode == FaceTrackerOptions::Mode::Image ? kMpRunningModeImage
                                                                      : kMpRunningModeVideo;
    mp.num_faces = 1;
    mp.min_face_detection_confidence = options.min_face_detection_confidence;
    mp.min_face_presence_confidence = options.min_face_presence_confidence;
    mp.min_tracking_confidence = options.min_tracking_confidence;
    mp.output_face_blendshapes = true;
    mp.output_facial_transformation_matrixes = false;

    char* error = nullptr;
    if (impl->create(&mp, &impl->landmarker, &error) != kMpOk || !impl->landmarker)
        return std::unexpected(TrackerError{impl->takeError(error, "could not create face landmarker")});

    impl->mode = options.mode;
    return std::unique_ptr<FaceTracker>(new FaceTracker(std::move(impl)));
}

TrackerResult<FaceFrame> FaceTracker::detect(const RgbImageView& image, std::int64_t timestamp_ms)
{
    Impl& d = *impl_;
    if (!image.data || image.width <= 0 || image.height <= 0 || image.stride < image.width * 3)
        return std::unexpected(TrackerError{"invalid image"});

    // The C API needs tightly packed rows.
    const int row_bytes = image.width * 3;
    const std::uint8_t* pixels = image.data;
    if (image.stride != row_bytes) {
        d.packed.resize(static_cast<std::size_t>(row_bytes) * image.height);
        for (int y = 0; y < image.height; ++y)
            std::memcpy(d.packed.data() + static_cast<std::size_t>(y) * row_bytes,
                        image.data + static_cast<std::size_t>(y) * image.stride, row_bytes);
        pixels = d.packed.data();
    }

    MpImagePtr mp_image = nullptr;
    char* error = nullptr;
    if (d.createImage(kMpImageFormatSrgb, image.width, image.height, pixels,
                      row_bytes * image.height, &mp_image, &error) != kMpOk) {
        return std::unexpected(TrackerError{d.takeError(error, "could not create image")});
    }

    MpFaceLandmarkerResult result{};
    int status;
    if (d.mode == FaceTrackerOptions::Mode::Image) {
        status = d.detectImage(d.landmarker, mp_image, nullptr, &result, &error);
    } else {
        if (timestamp_ms <= d.last_timestamp)
            timestamp_ms = d.last_timestamp + 1;
        d.last_timestamp = timestamp_ms;
        status = d.detectForVideo(d.landmarker, mp_image, nullptr, timestamp_ms, &result, &error);
    }
    d.freeImage(mp_image);
    if (status != kMpOk)
        return std::unexpected(TrackerError{d.takeError(error, "face detection failed")});

    FaceFrame frame;
    if (result.face_landmarks_count > 0) {
        frame.face_found = true;
        const MpNormalizedLandmarks& face = result.face_landmarks[0];
        frame.landmarks.reserve(face.landmarks_count);
        for (std::uint32_t i = 0; i < face.landmarks_count; ++i)
            frame.landmarks.push_back({face.landmarks[i].x, face.landmarks[i].y, face.landmarks[i].z});
    }
    if (result.face_blendshapes_count > 0) {
        const MpCategories& shapes = result.face_blendshapes[0];
        frame.blendshapes.reserve(shapes.categories_count);
        for (std::uint32_t i = 0; i < shapes.categories_count; ++i) {
            const MpCategory& c = shapes.categories[i];
            frame.blendshapes.push_back({c.category_name ? c.category_name : "", c.score});
        }
    }
    d.closeResult(&result);
    return frame;
}

} // namespace game_face
