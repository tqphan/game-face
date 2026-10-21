#include "game_face/core/head_mouse.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>
#include <utility>

namespace game_face {

namespace {

// Longest gap between frames that still counts as motion; after a longer one
// (a stall, a paused camera) joystick mode doesn't make up for lost time.
constexpr double kMaxFrameGapS = 0.25;
// Relative mode acceleration: the gain factor at a speed of 0 and its slope
// per normalised unit per second, so 1 unit/s moves at the base rate.
constexpr double kAccelerationBase = 0.4;
constexpr double kAccelerationSlope = 0.6;
constexpr double kAccelerationMax = 2.5;
// Joystick mode: deflections beyond the range keep speeding up, to this much.
constexpr double kJoystickMaxDeflection = 2.0;
constexpr double kJoystickCurve = 1.5;
// A side of an axis that moved less than this share of its current range
// during calibration wasn't moved on purpose; its range is kept, so holding
// still doesn't make an axis hypersensitive. Small enough that someone with
// little head movement can still calibrate a small range.
constexpr double kCalibrationMinShare = 0.05;
constexpr std::size_t kCalibrationMinSamples = 10;
constexpr double kCalibrationPercentile = 0.95;
constexpr double kMinRange = 1e-6;

double lowPassAlpha(double cutoff_hz, double dt_s)
{
    const double tau = 1.0 / (2.0 * std::numbers::pi * cutoff_hz);
    return 1.0 / (1.0 + tau / dt_s);
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

double percentile(std::vector<double> values, double share)
{
    std::ranges::sort(values);
    const auto index = static_cast<std::size_t>(share * static_cast<double>(values.size() - 1) + 0.5);
    return values[std::min(index, values.size() - 1)];
}

} // namespace

// ------------------------------------------------------------- commands

std::optional<Result<HeadMouseCommand>> parseHeadMouseCommand(std::string_view text)
{
    constexpr std::string_view kPrefix = "headmouse";
    if (text.find(kPrefix) == std::string_view::npos)
        return std::nullopt;

    const std::string_view s = trim(text);
    const auto mixed = [&] {
        return failure("headmouse commands can't be combined with other commands; "
                       "give each its own binding");
    };
    if (!s.starts_with(kPrefix))
        return mixed();
    std::string_view rest = trim(s.substr(kPrefix.size()));
    if (!rest.starts_with('.'))
        return failure("expected headmouse.<command>(), e.g. headmouse.toggle()");
    rest = trim(rest.substr(1));
    std::size_t name_end = 0;
    while (name_end < rest.size() && std::isalpha(static_cast<unsigned char>(rest[name_end])))
        ++name_end;
    const std::string_view name = rest.substr(0, name_end);
    rest = trim(rest.substr(name_end));
    if (!rest.starts_with('('))
        return failure("expected headmouse." + std::string(name) + "()");
    rest = trim(rest.substr(1));
    if (!rest.starts_with(')'))
        return failure("headmouse." + std::string(name) + "() takes no arguments");
    if (!trim(rest.substr(1)).empty())
        return mixed();

    static constexpr std::pair<std::string_view, HeadMouseCommand> kNames[] = {
        {"toggle", HeadMouseCommand::Toggle},   {"enable", HeadMouseCommand::Enable},
        {"disable", HeadMouseCommand::Disable}, {"hold", HeadMouseCommand::Hold},
        {"release", HeadMouseCommand::Release}, {"recenter", HeadMouseCommand::Recenter},
        {"calibrate", HeadMouseCommand::Calibrate},
    };
    for (const auto& [known, command] : kNames) {
        if (name == known)
            return Result<HeadMouseCommand>(command);
    }
    return failure("unknown command headmouse." + std::string(name)
                   + "(); use toggle, enable, disable, hold, release, recenter or calibrate");
}

// ------------------------------------------------------------- OneEuroFilter

OneEuroFilter::OneEuroFilter(double min_cutoff_hz, double beta, double derivative_cutoff_hz)
    : min_cutoff_(min_cutoff_hz), beta_(beta), d_cutoff_(derivative_cutoff_hz)
{
}

void OneEuroFilter::setParameters(double min_cutoff_hz, double beta)
{
    min_cutoff_ = min_cutoff_hz;
    beta_ = beta;
}

double OneEuroFilter::filter(double value, double dt_s)
{
    if (!value_ || min_cutoff_ <= 0) {
        // A cutoff of 0 or less turns the filter off.
        value_ = value;
        derivative_ = 0;
        return value;
    }
    if (dt_s <= 0)
        return *value_;
    const double raw_derivative = (value - *value_) / dt_s;
    derivative_ += lowPassAlpha(d_cutoff_, dt_s) * (raw_derivative - derivative_);
    const double cutoff = min_cutoff_ + beta_ * std::abs(derivative_);
    value_ = *value_ + lowPassAlpha(cutoff, dt_s) * (value - *value_);
    return *value_;
}

void OneEuroFilter::reset()
{
    value_.reset();
    derivative_ = 0;
}

// ------------------------------------------------------------- HeadMouse

HeadMouse::HeadMouse()
{
    setSettings(settings_);
}

HeadMouseAxis& HeadMouse::axisSettings(int axis)
{
    HeadMouseGroup& group = groupSettings(axis);
    return axis % 2 == 0 ? group.x : group.y;
}

HeadMouseGroup& HeadMouse::groupSettings(int axis)
{
    return axis < ScrollX ? settings_.pointer : settings_.scroll;
}

void HeadMouse::setSettings(const HeadMouseSettings& settings)
{
    const HeadMouseSettings previous = settings_;
    settings_ = settings;
    for (int i = 0; i < kAxisCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        AxisState& state = axes_[index];
        const HeadMouseAxis& axis = axisSettings(i);
        const HeadMouseGroup& old_group = i < ScrollX ? previous.pointer : previous.scroll;
        const HeadMouseAxis& old_axis = i % 2 == 0 ? old_group.x : old_group.y;
        const bool changed = axis.source != old_axis.source || !state.expression;
        if (!changed && errors_[index].empty())
            continue;
        state.expression.reset();
        state.filter.reset();
        state.previous.reset();
        state.remainder = 0;
        errors_[index].clear();
        if (trim(axis.source).empty())
            continue;
        auto compiled = Expression::compile(axis.source, kSignalNames);
        if (compiled)
            state.expression = std::move(*compiled);
        else
            errors_[index] = compiled.error().message;
    }
    if (previous.pointer.mode != settings_.pointer.mode || previous.scroll.mode != settings_.scroll.mode)
        reset();
    applySmoothing();
}

void HeadMouse::applySmoothing()
{
    for (int i = 0; i < kAxisCount; ++i) {
        const double s = std::clamp(groupSettings(i).smoothing, 0.0, 1.0);
        // 0 passes values through; 1 settles at 0.3 Hz when still.
        const double min_cutoff = s <= 0 ? 0.0 : 10.0 * std::pow(0.03, s);
        axes_[static_cast<std::size_t>(i)].filter.setParameters(min_cutoff, 1.5);
    }
}

void HeadMouse::reset()
{
    for (AxisState& state : axes_) {
        state.filter.reset();
        state.previous.reset();
        state.remainder = 0;
    }
    last_time_ms_.reset();
}

void HeadMouse::setEnabled(bool enabled)
{
    if (enabled == settings_.enabled)
        return;
    settings_.enabled = enabled;
    settings_changed_ = true;
}

void HeadMouse::setHeld(bool held)
{
    held_ = held;
}

void HeadMouse::freezeUntil(double time_ms)
{
    frozen_until_ms_ = std::max(frozen_until_ms_, time_ms);
}

void HeadMouse::startCalibration(double duration_ms)
{
    calibration_ = Calibration{duration_ms, std::nullopt, {}};
}

bool HeadMouse::takeSettingsChanged()
{
    return std::exchange(settings_changed_, false);
}

double HeadMouse::normalise(int axis, double raw)
{
    const HeadMouseAxis& a = axisSettings(axis);
    const double offset = raw - a.neutral.value_or(raw);
    const double range = std::max(offset < 0 ? a.range_negative : a.range_positive, kMinRange);
    const double n = offset / range;
    return a.invert ? -n : n;
}

void HeadMouse::finishCalibration()
{
    for (int i = 0; i < kAxisCount; ++i) {
        auto& samples = calibration_->samples[static_cast<std::size_t>(i)];
        HeadMouseAxis& axis = axisSettings(i);
        std::vector<double> below;
        std::vector<double> above;
        for (double offset : samples)
            (offset < 0 ? below : above).push_back(std::abs(offset));
        auto measure = [](const std::vector<double>& offsets, double& range) {
            if (offsets.size() < kCalibrationMinSamples)
                return;
            const double measured = percentile(offsets, kCalibrationPercentile);
            if (measured >= range * kCalibrationMinShare)
                range = measured;
        };
        measure(below, axis.range_negative);
        measure(above, axis.range_positive);
    }
    calibration_.reset();
    settings_changed_ = true;
    // The ranges changed, so the normalised values did too: start the motion
    // over so the pointer doesn't jump.
    for (AxisState& state : axes_) {
        state.filter.reset();
        state.previous.reset();
        state.remainder = 0;
    }
}

int HeadMouse::take(int axis, double amount)
{
    double& remainder = axes_[static_cast<std::size_t>(axis)].remainder;
    remainder += amount;
    const double whole = std::trunc(remainder);
    remainder -= whole;
    return static_cast<int>(whole);
}

HeadMouseOutput HeadMouse::process(const SignalValues& values, double time_ms)
{
    HeadMouseOutput out;
    double dt_s = 0;
    if (last_time_ms_)
        dt_s = std::clamp((time_ms - *last_time_ms_) / 1000.0, 0.0, kMaxFrameGapS);
    last_time_ms_ = time_ms;

    for (int i = 0; i < kAxisCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        AxisState& state = axes_[index];
        if (!state.expression)
            continue;
        const double raw = state.expression->evaluate(values);
        if (!std::isfinite(raw))
            continue;
        HeadMouseAxis& axis = axisSettings(i);
        if (!axis.neutral || recenter_pending_) {
            axis.neutral = raw;
            settings_changed_ = true;
            // A new centre: start the motion over so nothing jumps.
            state.filter.reset();
            state.previous.reset();
            state.remainder = 0;
        }
        raw_[index] = raw;
        filtered_[index] = state.filter.filter(normalise(i, raw), dt_s);
        out.axes[index] = {true, raw, filtered_[index]};
    }
    recenter_pending_ = false;

    const bool calibrating = calibration_.has_value();
    if (calibrating) {
        if (!calibration_->start_ms)
            calibration_->start_ms = time_ms;
        for (int i = 0; i < kAxisCount; ++i) {
            const auto index = static_cast<std::size_t>(i);
            if (out.axes[index].active)
                calibration_->samples[index].push_back(raw_[index] - axisSettings(i).neutral.value_or(0));
        }
        if (time_ms - *calibration_->start_ms >= calibration_->duration_ms)
            finishCalibration();
    }

    const bool allowed = settings_.enabled && !held_ && time_ms >= frozen_until_ms_ && !calibrating;
    processGroup(PointerX, dt_s, rates_.pointer_relative_px, rates_.pointer_joystick_px_s, true, allowed, out);
    processGroup(ScrollX, dt_s, rates_.scroll_relative_notches, rates_.scroll_joystick_notches_s, false, allowed,
                 out);
    return out;
}

void HeadMouse::processGroup(int x_axis, double dt_s, double rate_relative, double rate_joystick,
                             bool allow_absolute, bool moving_allowed, HeadMouseOutput& out)
{
    const int y_axis = x_axis + 1;
    const HeadMouseGroup& group = groupSettings(x_axis);
    AxisState& sx = axes_[static_cast<std::size_t>(x_axis)];
    AxisState& sy = axes_[static_cast<std::size_t>(y_axis)];
    const bool x_on = out.axes[static_cast<std::size_t>(x_axis)].active;
    const bool y_on = out.axes[static_cast<std::size_t>(y_axis)].active;
    if (!x_on && !y_on)
        return;
    const double nx = x_on ? filtered_[static_cast<std::size_t>(x_axis)] : 0;
    const double ny = y_on ? filtered_[static_cast<std::size_t>(y_axis)] : 0;
    const double deadzone = std::clamp(group.deadzone, 0.0, 0.9);
    const double gain_x = std::clamp(group.x.gain, 0.0, 10.0);
    const double gain_y = std::clamp(group.y.gain, 0.0, 10.0);

    double amount_x = 0;
    double amount_y = 0;
    HeadMouseMode mode = group.mode;
    if (mode == HeadMouseMode::Absolute && !allow_absolute)
        mode = HeadMouseMode::Joystick;

    // Relative mode follows a point that the head drags along behind it
    // (backlash): the point only moves once the head is more than the
    // deadzone away from it. Tracking noise, which wobbles around a still
    // head, never gets that far; a real movement loses only the deadzone's
    // width when it starts or turns back. `previous` holds the point.
    std::optional<double> point_x = nx, point_y = ny;
    if (mode == HeadMouseMode::Relative) {
        if (sx.previous || !x_on)
            point_x = x_on ? *sx.previous : 0;
        if (sy.previous || !y_on)
            point_y = y_on ? *sy.previous : 0;
        const double ox = nx - *point_x;
        const double oy = ny - *point_y;
        const double distance = std::hypot(ox, oy);
        double mx = 0, my = 0;
        if (distance > deadzone) {
            const double past = (distance - deadzone) / distance;
            mx = ox * past;
            my = oy * past;
        }
        point_x = *point_x + mx;
        point_y = *point_y + my;
        // The movement already spans the time between frames; dt only gives
        // the speed for acceleration.
        if (dt_s > 0 && (mx != 0 || my != 0)) {
            const double speed = std::hypot(mx, my) / dt_s;
            const double acceleration = std::min(kAccelerationBase + kAccelerationSlope * speed, kAccelerationMax);
            amount_x = mx * acceleration * rate_relative * gain_x;
            amount_y = my * acceleration * rate_relative * gain_y;
        }
    } else {
        const double magnitude = std::hypot(nx, ny);
        if (magnitude > deadzone) {
            const double past = std::min((magnitude - deadzone) / (1.0 - deadzone), kJoystickMaxDeflection);
            const double scale = std::pow(past, kJoystickCurve) / magnitude;
            if (mode == HeadMouseMode::Absolute) {
                if (moving_allowed)
                    out.absolute = std::array<double, 2>{std::clamp(nx * scale * gain_x, -1.0, 1.0),
                                                         std::clamp(ny * scale * gain_y, -1.0, 1.0)};
            } else {
                amount_x = nx * scale * rate_joystick * gain_x * dt_s;
                amount_y = ny * scale * rate_joystick * gain_y * dt_s;
            }
        } else if (mode == HeadMouseMode::Absolute && moving_allowed) {
            out.absolute = std::array<double, 2>{0.0, 0.0};
        }
    }
    if (x_on)
        sx.previous = point_x;
    if (y_on)
        sy.previous = point_y;

    if (!moving_allowed)
        return;
    const int ix = take(x_axis, amount_x);
    const int iy = take(y_axis, amount_y);
    if (x_axis == PointerX) {
        out.dx = ix;
        out.dy = iy;
    } else {
        out.scroll_x = ix;
        out.scroll_y = iy;
    }
    if (ix != 0 || iy != 0 || out.absolute)
        out.moving = true;
}

} // namespace game_face
