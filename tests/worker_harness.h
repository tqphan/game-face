#pragma once

// Drives a TrackingWorker with rendered head motion instead of a camera
// (docs/head-mouse-plan.md, 8.3). Everything runs on the calling thread.

#include <face_render.h>
#include <tracking_worker.h>

#include <QCoreApplication>
#include <QString>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace harness {

#if defined(Q_OS_WIN)
inline constexpr auto kLibrary = "libmediapipe.dll";
#elif defined(Q_OS_MACOS)
inline constexpr auto kLibrary = "libmediapipe.dylib";
#else
inline constexpr auto kLibrary = "libmediapipe.so";
#endif

inline std::filesystem::path asset(const char* name)
{
    return std::filesystem::path(GAME_FACE_TEST_ASSETS_DIR) / name;
}

inline bool assetsAvailable()
{
    return std::filesystem::exists(asset("LeePerrySmith.glb")) && std::filesystem::exists(asset("Map-COL.jpg"));
}

// Renders frames for poses, remembering each one: scripts revisit poses.
class Renderer {
public:
    bool load()
    {
        auto head = face_render::HeadModel::load(asset("LeePerrySmith.glb"), asset("Map-COL.jpg"));
        if (!head)
            return false;
        head_ = std::move(*head);
        camera_.size = QSize(400, 300);
        return true;
    }

    QImage frame(const std::optional<face_render::HeadPose>& pose)
    {
        if (!pose)
            return face_render::render(*head_, pose, camera_);
        const auto key = std::make_tuple(pose->yaw, pose->pitch, pose->roll, pose->x, pose->y, pose->z);
        auto it = cache_.find(key);
        if (it == cache_.end())
            it = cache_.emplace(key, face_render::render(*head_, pose, camera_)).first;
        return it->second;
    }

private:
    std::optional<face_render::HeadModel> head_;
    face_render::Camera camera_;
    std::map<std::tuple<double, double, double, double, double, double>, QImage> cache_;
};

// Collects what the worker reports.
struct Run {
    std::vector<FrameResult> frames;
    std::vector<QString> errors;
    std::vector<game_face::HeadMouseSettings> settings_reports;
    int start_edges = 0;
    int stop_edges = 0;

    long dx() const
    {
        long sum = 0;
        for (const auto& f : frames)
            sum += f.head_mouse.dx;
        return sum;
    }
    long dy() const
    {
        long sum = 0;
        for (const auto& f : frames)
            sum += f.head_mouse.dy;
        return sum;
    }
};

inline TrackerConfig trackerConfig()
{
    const QString dir = QCoreApplication::applicationDirPath();
    TrackerConfig config;
    config.library_path = std::filesystem::path((dir + "/" + kLibrary).toStdU16String());
    config.model_path = std::filesystem::path((dir + "/face_landmarker.task").toStdU16String());
    return config;
}

// Hooks a worker's callbacks up to `run`.
inline void attach(TrackingWorker& worker, Run& run)
{
    worker.onFrame = [&run](FrameResult r) { run.frames.push_back(std::move(r)); };
    worker.onError = [&run](QString message) { run.errors.push_back(message); };
    worker.onHeadMouseSettings = [&run](game_face::HeadMouseSettings s) { run.settings_reports.push_back(s); };
    worker.onEdge = [&run](game_face::BindingEngine::Edge e) {
        (e == game_face::BindingEngine::Edge::Start ? run.start_edges : run.stop_edges)++;
    };
}

// Plays a motion script into the worker, with the script's timestamps
// offset by t0_ms. Returns the time after the last frame.
inline double play(TrackingWorker& worker, Renderer& renderer, const face_render::Motion& motion, double t0_ms = 0)
{
    double t = t0_ms;
    for (const auto& frame : motion.frames) {
        t = t0_ms + frame.time_s * 1000.0;
        worker.process(renderer.frame(frame.pose), t);
    }
    return t + 1000.0 / motion.fps;
}

inline face_render::Motion motion(const char* json)
{
    auto parsed = face_render::motionFromJson(json);
    if (!parsed)
        qFatal("bad motion script: %s", parsed.error().message.c_str());
    return *parsed;
}

// A profile with one advanced binding.
inline game_face::Profile bindingProfile(const std::string& start_expression, const std::string& start_command,
                                         const std::string& stop_expression = {},
                                         const std::string& stop_command = {})
{
    game_face::Binding binding;
    binding.simplified = false;
    binding.start = {start_expression, start_command, 0};
    binding.stop = {stop_expression.empty() ? "headRoll < -1000" : stop_expression, stop_command, 0};
    game_face::Profile profile;
    profile.bindings.push_back(binding);
    return profile;
}

} // namespace harness
