#include "game_face/core/binding_engine.h"

#include <algorithm>
#include <cctype>

namespace game_face {

namespace {

bool isBlank(const std::string& s)
{
    return std::ranges::all_of(s, [](unsigned char c) { return std::isspace(c); });
}

} // namespace

BindingEngine::BindingEngine(CommandSink sink)
    : sink_(std::move(sink))
{
}

void BindingEngine::setProfile(const Profile& profile)
{
    bindings_.clear();
    status_.clear();

    auto compile = [](const Trigger& trigger, CompiledTrigger& out, std::string& error) {
        out.command = trigger.command;
        out.debounce_ms = trigger.debounce_ms;
        if (isBlank(trigger.expression)) {
            error = "expression is empty";
            return;
        }
        auto expression = Expression::compile(trigger.expression, kBlendshapeNames);
        if (expression)
            out.expression = std::move(*expression);
        else
            error = expression.error().message;
    };

    for (const Binding& binding : profile.bindings) {
        CompiledBinding compiled;
        compiled.enabled = binding.enabled;
        compiled.simplified = binding.simplified;
        compiled.blendshape = blendshapeIndex(binding.simple.blendshape);
        compiled.threshold = binding.simple.threshold;
        compiled.start_command = binding.simple.start_command;
        compiled.stop_command = binding.simple.stop_command;

        BindingStatus status;
        compile(binding.start, compiled.start, status.start_error);
        compile(binding.stop, compiled.stop, status.stop_error);

        bindings_.push_back(std::move(compiled));
        status_.push_back(std::move(status));
    }
    reset();
}

void BindingEngine::resetBinding(CompiledBinding& binding)
{
    binding.armed = true;
    binding.start.activated = false;
    binding.start.since_ms.reset();
    // The stop trigger starts "fired", so a stop condition that is already
    // true on the first frame doesn't send a stray release.
    binding.stop.activated = true;
    binding.stop.since_ms.reset();
}

void BindingEngine::reset()
{
    for (auto& binding : bindings_)
        resetBinding(binding);
    for (auto& status : status_) {
        status.start_active = false;
        status.stop_active = true;
        status.simple_active = false;
    }
}

void BindingEngine::process(const BlendshapeScores& scores, double time_ms)
{
    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        CompiledBinding& binding = bindings_[i];
        if (!binding.enabled)
            continue;
        if (binding.simplified) {
            processSimple(binding, scores);
        } else {
            processTrigger(binding.start, scores, time_ms);
            processTrigger(binding.stop, scores, time_ms);
        }
        status_[i].start_active = binding.start.activated;
        status_[i].stop_active = binding.stop.activated;
        status_[i].simple_active = !binding.armed;
    }
}

// Hysteresis on one blendshape. A value equal to the threshold changes nothing.
void BindingEngine::processSimple(CompiledBinding& binding, const BlendshapeScores& scores)
{
    if (!binding.blendshape)
        return;
    const double value = scores[*binding.blendshape];
    if (binding.armed) {
        if (binding.threshold < value) {
            fire(binding.start_command);
            binding.armed = false;
        }
    } else if (binding.threshold > value) {
        fire(binding.stop_command);
        binding.armed = true;
    }
}

// Fires once when the expression turns true, after it has stayed true for
// longer than debounce_ms; re-arms as soon as it turns false.
void BindingEngine::processTrigger(CompiledTrigger& trigger, const BlendshapeScores& scores, double time_ms)
{
    const bool valid = trigger.expression && trigger.expression->test(scores);
    if (!valid) {
        trigger.activated = false;
        trigger.since_ms.reset();
        return;
    }
    if (trigger.activated)
        return;

    if (trigger.debounce_ms > 0) {
        if (!trigger.since_ms) {
            trigger.since_ms = time_ms;
            return;
        }
        if (time_ms - *trigger.since_ms <= trigger.debounce_ms)
            return;
    }
    fire(trigger.command);
    trigger.activated = true;
}

void BindingEngine::fire(const std::string& command)
{
    if (sink_ && !isBlank(command))
        sink_(command);
}

} // namespace game_face
