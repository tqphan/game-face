#pragma once

#include "expression.h"
#include "head_mouse_settings.h"
#include "result.h"
#include "signals.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace game_face {

// Commands bindings send to control the head mouse instead of injecting input.
enum class HeadMouseCommand { Toggle, Enable, Disable, Hold, Release, Recenter, Calibrate };

// nullopt: not a head mouse command (it goes to the input simulator).
// An error: it mentions headmouse but isn't exactly one headmouse.<name>()
// call, e.g. it's mixed with other commands, which isn't allowed.
std::optional<Result<HeadMouseCommand>> parseHeadMouseCommand(std::string_view text);

// The One Euro filter (Casiez, Roussel and Vogel, CHI 2012): a low-pass
// filter whose cutoff rises with speed, so holding still is smooth and fast
// movement doesn't lag. A minimum cutoff of 0 or less passes values through.
class OneEuroFilter {
public:
    OneEuroFilter(double min_cutoff_hz = 1.0, double beta = 0.0, double derivative_cutoff_hz = 1.0);
    void setParameters(double min_cutoff_hz, double beta);
    double filter(double value, double dt_s);
    void reset();

private:
    double min_cutoff_;
    double beta_;
    double d_cutoff_;
    std::optional<double> value_;
    double derivative_ = 0;
};

// Base rates that the sensitivity (gain) multiplies. A normalised value of 1
// is a full deflection of the source.
struct HeadMouseRates {
    double pointer_relative_px = 900;      // pixels per normalised unit moved
    double pointer_joystick_px_s = 1400;   // pixels per second at full deflection
    double scroll_relative_notches = 12;   // notches per normalised unit moved
    double scroll_joystick_notches_s = 12; // notches per second at full deflection
};

// What one frame produced.
struct HeadMouseOutput {
    // Relative movement in whole pixels, X right and Y up.
    int dx = 0;
    int dy = 0;
    // Whole wheel notches, X right and Y up.
    int scroll_x = 0;
    int scroll_y = 0;
    // Absolute mode: where the pointer should be, -1..1 on each axis from the
    // screen's center (X right, Y up). Not set in other modes.
    std::optional<std::array<double, 2>> absolute;

    // Live readout, indexed pointer X, pointer Y, scroll X, scroll Y.
    struct Axis {
        bool active = false;      // has a source that compiled
        double raw = 0;           // the source's value
        double normalised = 0;    // filtered offset from neutral / range
    };
    std::array<Axis, 4> axes;
    bool moving = false;  // anything was sent (or would be, if enabled)
};

// Maps signals to pointer and scroll movement (docs/head-mouse-plan.md).
// Not thread-safe; owned by the tracking worker. Injects nothing.
class HeadMouse {
public:
    static constexpr int kAxisCount = 4;
    enum Axis { PointerX, PointerY, ScrollX, ScrollY };

    HeadMouse();

    // Compiles the sources. A source that doesn't compile turns its axis off;
    // sourceError says why. Keeps filter state when only tuning changes.
    void setSettings(const HeadMouseSettings& settings);
    const HeadMouseSettings& settings() const { return settings_; }
    const std::string& sourceError(int axis) const { return errors_[static_cast<std::size_t>(axis)]; }
    void setRates(const HeadMouseRates& rates) { rates_ = rates; }

    // One frame with a face. time_ms must not go backwards. The readout is
    // filled even while the head mouse is disabled.
    HeadMouseOutput process(const SignalValues& values, double time_ms);

    // The face was lost or tracking paused: forget the motion so the pointer
    // doesn't jump when it's back. The neutral is kept.
    void reset();

    // Captures every axis's current source value as its neutral on the next
    // processed frame (this one, if called from a binding during it).
    void recenter() { recenter_pending_ = true; }

    void setEnabled(bool enabled);
    // The clutch: no movement while held; resuming doesn't jump.
    void setHeld(bool held);
    bool held() const { return held_; }
    // No movement until time_ms (click stabilisation).
    void freezeUntil(double time_ms);

    // Measures each active axis's range for duration_ms from the next frame:
    // the user moves through their comfortable range, then the furthest
    // deviation each way (ignoring the most extreme 5% of samples) becomes
    // range_negative/range_positive. Nothing moves meanwhile.
    void startCalibration(double duration_ms = 5000);
    bool calibrating() const { return calibration_.has_value(); }

    // True once after the mapper changed its own settings (a neutral was
    // captured, calibration finished), so the owner can save them.
    bool takeSettingsChanged();

private:
    struct AxisState {
        std::optional<Expression> expression;
        OneEuroFilter filter;
        // Relative mode: the backlash point the head drags (see processGroup).
        std::optional<double> previous;
        double remainder = 0;            // sub-pixel / sub-notch carry
    };

    struct Calibration {
        double duration_ms;
        std::optional<double> start_ms;
        std::array<std::vector<double>, kAxisCount> samples;
    };

    HeadMouseAxis& axisSettings(int axis);
    HeadMouseGroup& groupSettings(int axis);
    void applySmoothing();
    void finishCalibration();
    double normalise(int axis, double raw);
    void processGroup(int x_axis, double dt_s, double rate_relative, double rate_joystick,
                      bool allow_absolute, bool moving_allowed, HeadMouseOutput& out);
    int take(int axis, double amount);

    HeadMouseSettings settings_;
    HeadMouseRates rates_;
    std::array<AxisState, kAxisCount> axes_;
    std::array<std::string, kAxisCount> errors_;
    std::array<double, kAxisCount> raw_{};
    std::array<double, kAxisCount> filtered_{};
    std::optional<double> last_time_ms_;
    double frozen_until_ms_ = -1;
    bool held_ = false;
    bool recenter_pending_ = false;
    bool settings_changed_ = false;
    std::optional<Calibration> calibration_;
};

} // namespace game_face
