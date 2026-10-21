// head-mouse-bench: measures how the head mouse feels, with numbers
// (docs/head-mouse-plan.md, 8.5). Runs the HeadMouse mapper on generated
// head motion and reports, for each mode and smoothing setting:
//   jitter    pointer movement while the head holds still (px/s)
//   lag       how long the pointer keeps going after the head stops (ms)
//   fps       worst difference in total movement between 15, 30 and 60 fps
//             with 10% of frames dropped, against 60 fps without drops (%)
//   targets   a simulated user aiming at targets of different sizes and
//             distances: time per target (ms), Fitts throughput (bits/s),
//             first-move overshoot (px) and misses (%)
// Head noise defaults to what tst_face_signals measures on a still rendered
// head (yaw 0.15, pitch 0.27 degrees); --noise scales it, since real webcams
// are noisier.
//
//   head-mouse-bench [--csv results.csv] [--check] [--noise 2]
//
// --check fails (exit 1) when the default settings miss the limits below; it
// runs as a ctest. Nothing is injected: the mapper only computes movement.

#include <game_face/core/head_mouse.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>
#include <random>
#include <vector>

using namespace game_face;

namespace {

// --check limits for the default settings.
constexpr double kMaxJitterPxPerS = 2.0;
constexpr double kMaxLagMs = 250;
constexpr double kMaxFpsDifference = 5.0;  // %
constexpr double kMaxMissRate = 5.0;       // %

struct Head {
    double yaw = 0;
    double pitch = 0;
};

SignalValues signalsOf(const Head& h)
{
    SignalValues v{};
    v[signalIndex(HeadSignal::Yaw)] = h.yaw;
    v[signalIndex(HeadSignal::Pitch)] = h.pitch;
    return v;
}

struct Config {
    HeadMouseMode mode;
    double smoothing;
};

// --deadzone; negative keeps the default.
double g_deadzone = -1;

HeadMouseSettings settingsFor(const Config& c)
{
    HeadMouseSettings s;
    s.enabled = true;
    s.pointer.mode = c.mode;
    s.pointer.smoothing = c.smoothing;
    if (g_deadzone >= 0)
        s.pointer.deadzone = g_deadzone;
    s.pointer.x.neutral = 0;
    s.pointer.y.neutral = 0;
    return s;
}

class Noise {
public:
    Noise(double scale, unsigned seed) : rng_(seed), yaw_(0, 0.15 * scale), pitch_(0, 0.27 * scale) {}
    Head add(Head h)
    {
        h.yaw += yaw_(rng_);
        h.pitch += pitch_(rng_);
        return h;
    }

private:
    std::mt19937 rng_;
    std::normal_distribution<double> yaw_;
    std::normal_distribution<double> pitch_;
};

double minimumJerk(double f)
{
    f = std::clamp(f, 0.0, 1.0);
    return f * f * f * (10 - 15 * f + 6 * f * f);
}

// ------------------------------------------------------------ jitter

double jitter(const Config& c, double noise_scale)
{
    HeadMouse hm;
    hm.setSettings(settingsFor(c));
    Noise noise(noise_scale, 7);
    const double fps = 30;
    double moved = 0;
    const int frames = static_cast<int>(10 * fps);
    for (int i = 0; i <= frames; ++i) {
        const auto out = hm.process(signalsOf(noise.add({})), i * 1000.0 / fps);
        moved += std::hypot(out.dx, out.dy);
    }
    return moved / 10.0;
}

// ------------------------------------------------------------ lag

// Relative: the head turns 10 degrees in 0.3 s and stops; how long until
// the pointer has done 95% of its total movement? Joystick: the head turns
// to 10 degrees and holds; how long after it arrives until the pointer
// reaches 95% of its final speed?
double lag(const Config& c)
{
    HeadMouse hm;
    hm.setSettings(settingsFor(c));
    const double fps = 60;
    const double turn_end = 0.3;
    std::vector<double> t, x;
    double total = 0;
    for (int i = 0; i <= static_cast<int>(3 * fps); ++i) {
        const double ts = i / fps;
        const auto out = hm.process(signalsOf({10 * minimumJerk(ts / turn_end), 0}), ts * 1000);
        total += out.dx;
        t.push_back(ts);
        x.push_back(total);
    }
    if (c.mode == HeadMouseMode::Relative) {
        for (std::size_t i = 0; i < x.size(); ++i) {
            if (x[i] >= 0.95 * total)
                return std::max(0.0, (t[i] - turn_end) * 1000);
        }
        return 3000;
    }
    // Joystick: speed over the last half second is the final speed.
    const double final_speed = (x.back() - x[x.size() - 31]) / 0.5;
    for (std::size_t i = 6; i < x.size(); ++i) {
        const double speed = (x[i] - x[i - 6]) / 0.1;
        if (speed >= 0.95 * final_speed)
            return std::max(0.0, (t[i] - turn_end) * 1000);
    }
    return 3000;
}

// ------------------------------------------------------------ frame rate

double fpsDifference(const Config& c)
{
    auto total = [&](double fps, double drop, unsigned seed) {
        HeadMouse hm;
        hm.setSettings(settingsFor(c));
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> chance(0, 1);
        long dx = 0;
        for (int i = 0; i <= static_cast<int>(3 * fps); ++i) {
            if (i > 0 && chance(rng) < drop)
                continue;
            const double ts = i / fps;
            dx += hm.process(signalsOf({15 * minimumJerk(ts / 1.0), 0}), ts * 1000).dx;
        }
        return static_cast<double>(dx);
    };
    const double reference = total(60, 0, 1);
    double worst = 0;
    for (double fps : {15.0, 30.0, 60.0})
        worst = std::max(worst, std::abs(total(fps, 0.1, 3) - reference) / std::max(1.0, std::abs(reference)) * 100);
    return worst;
}

// ------------------------------------------------------------ simulated user

struct TargetStats {
    double time_ms = 0;      // mean time to acquire, of the hits
    double throughput = 0;   // bits/s, effective
    double overshoot = 0;    // mean px past the target edge on the first move
    double miss_rate = 0;    // % of trials not acquired in time
};

double moveLength(double degrees)
{
    return 0.2 + 0.08 * std::log2(1 + std::abs(degrees));
}

// How far a smooth head turn of `degrees` from `head` would move the pointer
// in relative mode, given the mapper as it is now: a practised user's sense
// of the mapping, acceleration and deadzone included.
double relativePixels(const HeadMouse& now, double head, double t_s, double degrees)
{
    HeadMouse hm = now;
    const double fps = 30;
    const double length = moveLength(degrees);
    long dx = 0;
    for (int i = 1; i <= static_cast<int>((length + 0.3) * fps); ++i) {
        const double t = i / fps;
        dx += hm.process(signalsOf({head + degrees * minimumJerk(t / length), 0}), (t_s + t) * 1000).dx;
    }
    return static_cast<double>(dx);
}

// The turn that moves the pointer `pixels`, by bisection.
double relativeDegrees(const HeadMouse& now, double head, double t_s, double pixels)
{
    const double sign = pixels < 0 ? -1 : 1;
    double lo = 0, hi = 60;
    for (int i = 0; i < 25; ++i) {
        const double mid = (lo + hi) / 2;
        (relativePixels(now, head, t_s, sign * mid) * sign < std::abs(pixels) ? lo : hi) = mid;
    }
    return sign * (lo + hi) / 2;
}

// A person moving the pointer to a target with their head, in x only.
// They see the pointer with a reaction delay and repeat until it has stayed
// on the target for 300 ms. Relative and absolute: they plan a smooth head
// movement for what's left, knowing the mapping from practice but
// misjudging each movement's size by up to 20%. Joystick: they steer
// continuously, turning further the further the target is, and their head
// follows with a short lag. Their head has a slight tremor on top of the
// tracking noise.
TargetStats aimAtTargets(const Config& c, double noise_scale)
{
    constexpr double kFps = 30;
    constexpr double kReaction = 0.15;  // s
    constexpr double kDwell = 0.3;      // s on target to count
    constexpr double kTimeout = 6.0;    // s
    const double distances[] = {150, 400, 800};
    const double widths[] = {16, 32, 64};
    const HeadMouseSettings settings = settingsFor(c);
    const double range = settings.pointer.x.range_positive;
    HeadMouseRates rates;

    std::mt19937 rng(11);
    std::uniform_real_distribution<double> misjudge(0.8, 1.2);
    Noise noise(noise_scale, 5);
    double total_time = 0, total_bits = 0, total_overshoot = 0;
    int hits = 0, trials = 0, misses = 0;

    for (double distance : distances) {
        for (double width : widths) {
            for (int repeat = 0; repeat < 4; ++repeat) {
                ++trials;
                HeadMouse hm;
                hm.setSettings(settings);
                double pointer = 0;
                double head = 0;                 // yaw, degrees
                double move_from = 0, move_to = 0, move_start = 0, move_length = 0;
                double next_plan = kReaction;   // when the user next decides
                double on_target_since = -1;
                bool first_move_done = false;
                double first_overshoot = 0;
                std::vector<double> seen;        // pointer positions, for the delay
                bool hit = false;
                double t = 0;
                for (int i = 0; t < kTimeout; ++i) {
                    t = i / kFps;
                    // Where the user thinks the pointer is: kReaction ago.
                    seen.push_back(pointer);
                    const auto delay_frames = static_cast<std::size_t>(kReaction * kFps);
                    const double perceived = seen.size() > delay_frames ? seen[seen.size() - 1 - delay_frames] : 0;
                    const double error = distance - perceived;

                    if (c.mode == HeadMouseMode::Joystick) {
                        // Speed toward the target, slowing down near it, and
                        // centered once on it.
                        double want = 0;
                        if (std::abs(error) > width / 4) {
                            const double speed = std::clamp(error * 3.0, -rates.pointer_joystick_px_s * 0.6,
                                                            rates.pointer_joystick_px_s * 0.6);
                            const double n_speed = std::abs(speed) / rates.pointer_joystick_px_s;
                            // Invert the response curve: past = n_speed^(1/1.5).
                            const double past = std::pow(n_speed, 1 / 1.5);
                            const double n = past * (1 - settings.pointer.deadzone) + settings.pointer.deadzone;
                            want = (speed >= 0 ? 1 : -1) * n * range;
                        }
                        // The head follows what they want with a 0.12 s lag.
                        head += (want - head) * (1 - std::exp(-1.0 / kFps / 0.12));
                    } else if (t >= next_plan) {
                        double target_head;
                        if (c.mode == HeadMouseMode::Absolute) {
                            // Where the pointer sits for a head angle, and back;
                            // aim for where it is plus what's left.
                            const double dz = settings.pointer.deadzone;
                            auto place = [&](double yaw) {
                                const double n = std::abs(yaw) / range;
                                const double past = n <= dz ? 0 : std::pow((n - dz) / (1 - dz), 1.5);
                                return (yaw < 0 ? -1 : 1) * std::min(past, 1.0) * 960.0;
                            };
                            auto angle = [&](double px) {
                                const double past = std::pow(std::min(std::abs(px) / 960.0, 1.0), 1 / 1.5);
                                return (px < 0 ? -1 : 1) * (past * (1 - dz) + dz) * range;
                            };
                            target_head = angle(place(head) + error * misjudge(rng));
                        } else {
                            // Relative: turn by what's left.
                            target_head = head + relativeDegrees(hm, head, t, error) * misjudge(rng);
                        }
                        move_from = head;
                        move_to = target_head;
                        move_start = t;
                        move_length = moveLength(move_to - move_from);
                        next_plan = t + move_length + kReaction;
                    }
                    if (c.mode != HeadMouseMode::Joystick)
                        head = move_from + (move_to - move_from) * minimumJerk((t - move_start) / std::max(move_length, 1e-3));
                    const double tremor = 0.1 * std::sin(2 * std::numbers::pi * 9 * t);
                    Head h = noise.add({head + tremor, 0});
                    const auto out = hm.process(signalsOf(h), t * 1000);
                    if (out.absolute)
                        pointer = (*out.absolute)[0] * 960.0;
                    else
                        pointer += out.dx;

                    if (!first_move_done && c.mode == HeadMouseMode::Joystick && pointer > distance - width / 2
                        && std::abs(head) < 1e-3 + range) {
                        // Joystick: the first pass over the target.
                        if (std::abs(error) <= width / 4 || pointer > distance + width / 2) {
                            first_move_done = true;
                            first_overshoot = std::max(0.0, pointer - (distance + width / 2));
                        }
                    }
                    if (!first_move_done && c.mode != HeadMouseMode::Joystick && t > move_start + move_length
                        && move_start > 0) {
                        first_move_done = true;
                        first_overshoot = std::max(0.0, pointer - (distance + width / 2));
                    }
                    if (std::abs(pointer - distance) <= width / 2) {
                        if (on_target_since < 0)
                            on_target_since = t;
                        if (t - on_target_since >= kDwell) {
                            hit = true;
                            break;
                        }
                    } else {
                        on_target_since = -1;
                    }
                }
                if (hit) {
                    ++hits;
                    const double mt = on_target_since;  // reached the target for good
                    total_time += mt;
                    total_bits += std::log2(distance / width + 1);
                    total_overshoot += first_overshoot;
                } else {
                    ++misses;
                }
            }
        }
    }
    TargetStats stats;
    if (hits > 0) {
        stats.time_ms = total_time / hits * 1000;
        stats.throughput = total_bits / total_time;
        stats.overshoot = total_overshoot / hits;
    }
    stats.miss_rate = 100.0 * misses / trials;
    return stats;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Measures the head mouse's jitter, lag, frame-rate independence and aiming.");
    parser.addHelpOption();
    parser.addOptions({
        {"csv", "Also write the results here.", "file"},
        {"check", "Exit 1 if the default settings miss the limits."},
        {"noise", "Scale the head noise (1 = a still rendered head).", "scale", "1"},
        {"deadzone", "Use this deadzone instead of the default.", "deadzone"},
        {"smoothing", "Comma-separated smoothing values to try (default 0,0.25,0.5,0.75,1).", "list"},
    });
    parser.process(app);
    const double noise_scale = parser.value("noise").toDouble();
    if (parser.isSet("deadzone"))
        g_deadzone = parser.value("deadzone").toDouble();

    std::vector<double> smoothings{0.0, 0.25, 0.5, 0.75, 1.0};
    if (parser.isSet("smoothing")) {
        smoothings.clear();
        for (const QString& v : parser.value("smoothing").split(','))
            smoothings.push_back(v.toDouble());
    }
    // The defaults are always measured, for --check.
    const HeadMouseSettings default_settings;
    if (std::ranges::find(smoothings, default_settings.pointer.smoothing) == smoothings.end())
        smoothings.push_back(default_settings.pointer.smoothing);
    std::vector<Config> configs;
    for (auto mode : {HeadMouseMode::Relative, HeadMouseMode::Joystick, HeadMouseMode::Absolute})
        for (double smoothing : smoothings)
            configs.push_back({mode, smoothing});

    QString csv = "mode,smoothing,jitter_px_s,lag_ms,fps_diff_pct,target_ms,throughput_bits_s,overshoot_px,miss_pct\n";
    std::printf("%-9s %9s %11s %8s %8s %10s %12s %10s %7s\n", "mode", "smoothing", "jitter px/s", "lag ms",
                "fps %", "target ms", "bits/s", "overshoot", "miss %");
    bool ok = true;
    const HeadMouseSettings defaults;
    for (const Config& c : configs) {
        const double j = jitter(c, noise_scale);
        const double l = lag(c);
        const double f = c.mode == HeadMouseMode::Absolute ? 0 : fpsDifference(c);
        const TargetStats aim = aimAtTargets(c, noise_scale);
        const auto mode = headMouseModeName(c.mode);
        std::printf("%-9.*s %9.2f %11.2f %8.0f %8.2f %10.0f %12.2f %10.1f %7.1f\n", static_cast<int>(mode.size()),
                    mode.data(), c.smoothing, j, l, f, aim.time_ms, aim.throughput, aim.overshoot, aim.miss_rate);
        csv += QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                   .arg(QString::fromUtf8(mode.data(), static_cast<qsizetype>(mode.size())))
                   .arg(c.smoothing).arg(j).arg(l).arg(f).arg(aim.time_ms).arg(aim.throughput)
                   .arg(aim.overshoot).arg(aim.miss_rate);

        const bool is_default = c.mode == defaults.pointer.mode && c.smoothing == defaults.pointer.smoothing;
        if (is_default) {
            auto check = [&](bool pass, const char* what, double value, double limit) {
                if (!pass) {
                    std::printf("  FAIL: default %s %.2f exceeds %.2f\n", what, value, limit);
                    ok = false;
                }
            };
            check(j <= kMaxJitterPxPerS, "jitter", j, kMaxJitterPxPerS);
            check(l <= kMaxLagMs, "lag", l, kMaxLagMs);
            check(f <= kMaxFpsDifference, "frame-rate difference", f, kMaxFpsDifference);
            check(aim.miss_rate <= kMaxMissRate, "miss rate", aim.miss_rate, kMaxMissRate);
        }
    }
    // Joystick's own frame-rate check: it scales with dt, so it must be tight too.
    if (fpsDifference({HeadMouseMode::Joystick, defaults.pointer.smoothing}) > kMaxFpsDifference) {
        std::printf("  FAIL: joystick frame-rate difference\n");
        ok = false;
    }

    if (parser.isSet("csv")) {
        QFile file(parser.value("csv"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::fprintf(stderr, "error: could not write %s\n", qPrintable(parser.value("csv")));
            return 1;
        }
        QTextStream(&file) << csv;
    }
    if (parser.isSet("check"))
        return ok ? 0 : 1;
    return 0;
}
