#pragma once

#include "blendshapes.h"
#include "expression.h"
#include "profiles.h"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace game_face {

// Per-binding state the UI shows.
struct BindingStatus {
    std::string start_error;  // empty when the start expression compiled
    std::string stop_error;
    bool start_active = false;  // start trigger has fired and not re-armed
    bool stop_active = true;
    bool simple_active = false; // simple binding is above its threshold
};

// Turns blendshape scores into commands, following the original app's rules
// frame by frame. Commands go to `sink`; the engine injects nothing itself.
// Not thread-safe.
class BindingEngine {
public:
    using CommandSink = std::function<void(const std::string& command)>;

    explicit BindingEngine(CommandSink sink);

    // Copies the profile, compiles its expressions and resets all state.
    void setProfile(const Profile& profile);
    // Like setProfile, but keeps each binding's state (by position), so
    // editing a profile while tracking doesn't re-send commands.
    void updateProfile(const Profile& profile);
    // Re-arms every binding, e.g. after tracking stops. Held keys are the
    // caller's to release.
    void reset();

    // time_ms must not go backwards; it only matters for debounce.
    void process(const BlendshapeScores& scores, double time_ms);

    std::span<const BindingStatus> status() const { return status_; }

private:
    struct CompiledTrigger {
        std::optional<Expression> expression;
        std::string command;
        double debounce_ms = 0;
        bool activated = false;
        std::optional<double> since_ms;
    };

    struct CompiledBinding {
        bool enabled = true;
        bool simplified = true;
        std::optional<std::size_t> blendshape;
        double threshold = 50;
        std::string start_command;
        std::string stop_command;
        bool armed = true;  // the original app's simple.activated
        CompiledTrigger start;
        CompiledTrigger stop;
    };

    void compileProfile(const Profile& profile);
    void updateStatus(std::size_t index);
    void processSimple(CompiledBinding& binding, const BlendshapeScores& scores);
    void processTrigger(CompiledTrigger& trigger, const BlendshapeScores& scores, double time_ms);
    void fire(const std::string& command);
    void resetBinding(CompiledBinding& binding);

    CommandSink sink_;
    std::vector<CompiledBinding> bindings_;
    std::vector<BindingStatus> status_;
};

} // namespace game_face
