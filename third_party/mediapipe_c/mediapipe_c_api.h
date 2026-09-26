/*
 * Declarations for the MediaPipe Tasks C API exported by libmediapipe,
 * limited to what game-face uses (face landmarker + images).
 *
 * The pip wheel ships the library but no headers. These declarations were
 * transcribed from the ctypes bindings in the same wheel (mediapipe 1.0.1:
 * the .py files under mediapipe/tasks/python/{core,vision,components/containers}), so they
 * match that binary exactly. Re-check them whenever MEDIAPIPE_VERSION in
 * cmake/FetchMediaPipe.cmake changes.
 *
 * Functions are declared as pointer types because the library is loaded at
 * runtime. Status-returning functions return 0 (kMpOk) on success; on failure,
 * *error_msg receives a string that must be released with MpErrorFree.
 */
#ifndef GAME_FACE_MEDIAPIPE_C_API_H
#define GAME_FACE_MEDIAPIPE_C_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { kMpOk = 0 };

/* core/base_options_c.py */
enum { kMpDelegateCpu = 0, kMpDelegateGpu = 1 };
enum { kMpHostSystemUnknown = 0, kMpHostSystemLinux = 1, kMpHostSystemMac = 2, kMpHostSystemWindows = 3 };

typedef struct MpBaseOptions {
    const char* model_asset_buffer;
    unsigned int model_asset_buffer_count;
    const char* model_asset_path;
    int file_descriptor;          /* -1 when unused */
    int delegate;
    int host_environment;
    int host_system;
    const char* host_version;
    const char* ca_bundle_path;
    const char* app_id;
    const char* app_version;
} MpBaseOptions;

/* vision/core/vision_task_running_mode.py */
enum { kMpRunningModeImage = 1, kMpRunningModeVideo = 2, kMpRunningModeLiveStream = 3 };

/* vision/core/image.py */
enum { kMpImageFormatSrgb = 1, kMpImageFormatSrgba = 2, kMpImageFormatGray8 = 3 };

/* components/containers/landmark_c.py */
typedef struct MpNormalizedLandmark {
    float x;
    float y;
    float z;
    bool has_visibility;
    float visibility;
    bool has_presence;
    float presence;
    const char* name;
} MpNormalizedLandmark;

typedef struct MpNormalizedLandmarks {
    MpNormalizedLandmark* landmarks;
    uint32_t landmarks_count;
} MpNormalizedLandmarks;

/* components/containers/category_c.py */
typedef struct MpCategory {
    int index;
    float score;
    const char* category_name;
    const char* display_name;
} MpCategory;

typedef struct MpCategories {
    MpCategory* categories;
    uint32_t categories_count;
} MpCategories;

/* components/containers/matrix_c.py */
typedef struct MpMatrix {
    uint32_t rows;
    uint32_t cols;
    float* data;
} MpMatrix;

/* vision/face_landmarker.py */
typedef struct MpFaceLandmarkerResult {
    MpNormalizedLandmarks* face_landmarks;
    uint32_t face_landmarks_count;
    MpCategories* face_blendshapes;
    uint32_t face_blendshapes_count;
    MpMatrix* facial_transformation_matrixes;
    uint32_t facial_transformation_matrixes_count;
} MpFaceLandmarkerResult;

typedef void (*MpFaceLandmarkerResultCallback)(int32_t status,
                                               const MpFaceLandmarkerResult* result,
                                               void* image, int64_t timestamp_ms);

typedef struct MpFaceLandmarkerOptions {
    MpBaseOptions base_options;
    int running_mode;
    int num_faces;
    float min_face_detection_confidence;
    float min_face_presence_confidence;
    float min_tracking_confidence;
    bool output_face_blendshapes;
    bool output_facial_transformation_matrixes;
    MpFaceLandmarkerResultCallback result_callback;  /* live-stream mode only */
} MpFaceLandmarkerOptions;

typedef void* MpFaceLandmarkerPtr;
typedef void* MpImagePtr;

typedef int (*MpFaceLandmarkerCreateFn)(const MpFaceLandmarkerOptions* options,
                                        MpFaceLandmarkerPtr* landmarker, char** error_msg);
/* image_processing_options may be NULL. */
typedef int (*MpFaceLandmarkerDetectImageFn)(MpFaceLandmarkerPtr landmarker, MpImagePtr image,
                                             const void* image_processing_options,
                                             MpFaceLandmarkerResult* result, char** error_msg);
typedef int (*MpFaceLandmarkerDetectForVideoFn)(MpFaceLandmarkerPtr landmarker, MpImagePtr image,
                                                const void* image_processing_options,
                                                int64_t timestamp_ms,
                                                MpFaceLandmarkerResult* result, char** error_msg);
typedef void (*MpFaceLandmarkerCloseResultFn)(MpFaceLandmarkerResult* result);
typedef int (*MpFaceLandmarkerCloseFn)(MpFaceLandmarkerPtr landmarker, char** error_msg);

/* Copies the pixels; rows must be tightly packed (width * channels bytes).
 * data_size is the number of bytes in data. */
typedef int (*MpImageCreateFromUint8DataFn)(int format, int width, int height,
                                            const uint8_t* data, int data_size,
                                            MpImagePtr* image, char** error_msg);
typedef void (*MpImageFreeFn)(MpImagePtr image);

typedef void (*MpErrorFreeFn)(char* error_msg);

#ifdef __cplusplus
}
#endif

#endif /* GAME_FACE_MEDIAPIPE_C_API_H */
