#include <game_face/core/head_mouse.h>

#include <QTest>

#include <cmath>
#include <functional>

using namespace game_face;

namespace {

struct Head {
    double yaw = 0, pitch = 0, roll = 0, x = 0, y = 0, z = 0;
    double jawOpen = 0;
};

SignalValues signalsOf(const Head& h)
{
    SignalValues v{};
    v[signalIndex(HeadSignal::Yaw)] = h.yaw;
    v[signalIndex(HeadSignal::Pitch)] = h.pitch;
    v[signalIndex(HeadSignal::Roll)] = h.roll;
    v[signalIndex(HeadSignal::X)] = h.x;
    v[signalIndex(HeadSignal::Y)] = h.y;
    v[signalIndex(HeadSignal::Z)] = h.z;
    v[*signalIndex("jawOpen")] = h.jawOpen;
    return v;
}

// Enabled, centered at 0, no smoothing: easy numbers.
HeadMouseSettings plain(HeadMouseMode mode = HeadMouseMode::Relative)
{
    HeadMouseSettings s;
    s.enabled = true;
    s.pointer.mode = mode;
    s.pointer.smoothing = 0;
    s.pointer.deadzone = 0.05;
    s.scroll.deadzone = 0.05;
    s.pointer.x.neutral = 0;
    s.pointer.y.neutral = 0;
    return s;
}

struct Totals {
    long dx = 0, dy = 0, scroll_x = 0, scroll_y = 0;
    int frames = 0;
    int moving_frames = 0;
    HeadMouseOutput last;
};

// Feeds head(t) at `fps` from t0_s for `seconds`.
Totals run(HeadMouse& hm, const std::function<Head(double t_s)>& head, double fps, double seconds,
           double t0_s = 0)
{
    Totals t;
    const int n = static_cast<int>(std::lround(seconds * fps));
    for (int i = 0; i <= n; ++i) {
        const double ts = t0_s + i / fps;
        t.last = hm.process(signalsOf(head(ts)), ts * 1000.0);
        t.dx += t.last.dx;
        t.dy += t.last.dy;
        t.scroll_x += t.last.scroll_x;
        t.scroll_y += t.last.scroll_y;
        ++t.frames;
        if (t.last.moving)
            ++t.moving_frames;
    }
    return t;
}

// Yaw from 0 to `to` degrees over `over` seconds, then held.
std::function<Head(double)> turn(double to, double over, double start = 0)
{
    return [=](double t) {
        const double f = std::clamp((t - start) / over, 0.0, 1.0);
        return Head{.yaw = to * f};
    };
}

} // namespace

class TestHeadMouse : public QObject {
    Q_OBJECT

private slots:
    // ------------------------------------------------------------ commands

    void commands_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("command");
        QTest::newRow("toggle") << "headmouse.toggle()" << int(HeadMouseCommand::Toggle);
        QTest::newRow("enable") << "headmouse.enable()" << int(HeadMouseCommand::Enable);
        QTest::newRow("disable") << "headmouse.disable()" << int(HeadMouseCommand::Disable);
        QTest::newRow("hold") << "headmouse.hold()" << int(HeadMouseCommand::Hold);
        QTest::newRow("release") << "headmouse.release()" << int(HeadMouseCommand::Release);
        QTest::newRow("recenter") << "headmouse.recenter()" << int(HeadMouseCommand::Recenter);
        QTest::newRow("calibrate") << "headmouse.calibrate()" << int(HeadMouseCommand::Calibrate);
        QTest::newRow("spaces") << "  headmouse . toggle ( )  " << int(HeadMouseCommand::Toggle);
    }

    void commands()
    {
        QFETCH(QString, text);
        QFETCH(int, command);
        auto parsed = parseHeadMouseCommand(text.toStdString());
        QVERIFY(parsed.has_value());
        QVERIFY2(parsed->has_value(), parsed->has_value() ? "" : parsed->error().message.c_str());
        QCOMPARE(int(**parsed), command);
    }

    void otherCommandsAreNotHeadMouse()
    {
        QVERIFY(!parseHeadMouseCommand("mouse.click(BTN_LEFT)"));
        QVERIFY(!parseHeadMouseCommand("keyboard.press(KEY_A)"));
        QVERIFY(!parseHeadMouseCommand(""));
    }

    void badHeadMouseCommands_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("error");
        QTest::newRow("mixed after") << "headmouse.toggle(); mouse.click(BTN_LEFT)" << "combined";
        QTest::newRow("mixed before") << "mouse.click(BTN_LEFT); headmouse.toggle()" << "combined";
        QTest::newRow("argument") << "headmouse.toggle(1)" << "no arguments";
        QTest::newRow("unknown") << "headmouse.fly()" << "unknown";
        QTest::newRow("no call") << "headmouse.toggle" << "expected";
        QTest::newRow("no name") << "headmouse" << "expected";
    }

    void badHeadMouseCommands()
    {
        QFETCH(QString, text);
        QFETCH(QString, error);
        auto parsed = parseHeadMouseCommand(text.toStdString());
        QVERIFY(parsed.has_value());
        QVERIFY(!parsed->has_value());
        QVERIFY2(QString::fromStdString(parsed->error().message).contains(error),
                 parsed->error().message.c_str());
    }

    // ------------------------------------------------------------ presets

    void presets()
    {
        HeadMouseGroup group = HeadMouseSettings::defaultPointer();
        QCOMPARE(headMousePresetId(group), std::string_view("headTurn"));
        group.x.neutral = 3;
        applyHeadMousePreset(group, *findHeadMousePreset("headPosition"));
        QCOMPARE(group.x.source, std::string("headX"));
        QCOMPARE(group.y.source, std::string("headY"));
        QCOMPARE(group.x.range_negative, 5.0);
        QCOMPARE(group.x.range_positive, 5.0);
        QVERIFY(!group.x.neutral);  // a new source has a new center
        QCOMPARE(headMousePresetId(group), std::string_view("headPosition"));
        group.x.source = "headYaw + 2 * headX";
        QCOMPARE(headMousePresetId(group), std::string_view());
        QVERIFY(!findHeadMousePreset("nope"));
    }

    // ------------------------------------------------------------ neutral

    void firstFrameCapturesNeutral()
    {
        HeadMouse hm;
        HeadMouseSettings s = plain();
        s.pointer.x.neutral.reset();
        s.pointer.y.neutral.reset();
        hm.setSettings(s);
        auto out = hm.process(signalsOf({.yaw = 7, .pitch = -3}), 0);
        QVERIFY(hm.takeSettingsChanged());
        QVERIFY(!hm.takeSettingsChanged());  // once
        QCOMPARE(hm.settings().pointer.x.neutral, std::optional<double>(7));
        QCOMPARE(hm.settings().pointer.y.neutral, std::optional<double>(-3));
        QCOMPARE(out.axes[HeadMouse::PointerX].normalised, 0.0);
        QCOMPARE(out.dx, 0);
    }

    void savedNeutralIsKept()
    {
        HeadMouse hm;
        HeadMouseSettings s = plain();
        s.pointer.x.neutral = 5;
        hm.setSettings(s);
        auto out = hm.process(signalsOf({.yaw = 15}), 0);
        QVERIFY(!hm.takeSettingsChanged());
        QCOMPARE(out.axes[HeadMouse::PointerX].normalised, 0.5);  // (15 - 5) / 20
    }

    void recenter()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        hm.process(signalsOf({.yaw = 10}), 0);
        hm.recenter();
        auto out = hm.process(signalsOf({.yaw = 10}), 33);
        QCOMPARE(hm.settings().pointer.x.neutral, std::optional<double>(10));
        QVERIFY(hm.takeSettingsChanged());
        QCOMPARE(out.axes[HeadMouse::PointerX].normalised, 0.0);
        QCOMPARE(out.dx, 0);  // no jump
    }

    // ------------------------------------------------------------ relative

    void relativeFollowsTheHead()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        auto t = run(hm, turn(10, 0.5), 30, 1);
        QVERIFY2(t.dx > 100, qPrintable(QString::number(t.dx)));  // the user's right is screen right
        QCOMPARE(t.dy, 0L);

        HeadMouse up;
        up.setSettings(plain());
        auto u = run(up, [](double s) { return Head{.pitch = std::min(s, 0.5) * 16}; }, 30, 1);
        QVERIFY2(u.dy > 100, qPrintable(QString::number(u.dy)));  // up is positive
        QCOMPARE(u.dx, 0L);
    }

    void relativeStopsWhenTheHeadStops()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        run(hm, turn(10, 0.5), 30, 1);
        auto still = run(hm, turn(10, 0.5), 30, 1, 1.0);
        QCOMPARE(still.dx, 0L);
    }

    void relativeDeadzoneIgnoresJitter()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        // Noise up to 0.9 degrees, under the deadzone's 1 degree (0.05 of 20).
        auto t = run(hm, [](double s) { return Head{.yaw = (int(s * 30) % 3 - 1) * 0.45}; }, 30, 2);
        QCOMPARE(t.dx, 0L);
        QCOMPARE(t.moving_frames, 0);
    }

    void invertAndGain()
    {
        HeadMouse a;
        a.setSettings(plain());
        const auto base = run(a, turn(10, 0.5), 30, 1);

        HeadMouseSettings s = plain();
        s.pointer.x.invert = true;
        HeadMouse b;
        b.setSettings(s);
        const auto inverted = run(b, turn(10, 0.5), 30, 1);
        QCOMPARE(inverted.dx, -base.dx);

        s = plain();
        s.pointer.x.gain = 2;
        HeadMouse c;
        c.setSettings(s);
        const auto doubled = run(c, turn(10, 0.5), 30, 1);
        QVERIFY2(std::abs(doubled.dx - 2 * base.dx) <= 1, qPrintable(QString("%1 %2").arg(doubled.dx).arg(base.dx)));
    }

    void accelerationFavoursSlowPrecision()
    {
        // The same 10 degrees, slowly and quickly: slow moves the pointer less.
        HeadMouse slow;
        slow.setSettings(plain());
        const auto s = run(slow, turn(10, 2), 30, 3);
        HeadMouse fast;
        fast.setSettings(plain());
        const auto f = run(fast, turn(10, 0.2), 30, 3);
        QVERIFY2(s.dx < f.dx, qPrintable(QString("%1 %2").arg(s.dx).arg(f.dx)));
    }

    // ------------------------------------------------------------ joystick

    void joystickSpeedFollowsOffset()
    {
        HeadMouse hm;
        hm.setSettings(plain(HeadMouseMode::Joystick));
        // Held at 10 degrees: n = 0.5, past the deadzone (0.5 - 0.05) / 0.95.
        auto t = run(hm, [](double) { return Head{.yaw = 10}; }, 30, 1);
        const double expected = std::pow(0.45 / 0.95, 1.5) * 1400.0;
        QVERIFY2(std::abs(t.dx - expected) <= 2, qPrintable(QString("%1 vs %2").arg(t.dx).arg(expected)));
    }

    void joystickDeadzone()
    {
        HeadMouse hm;
        hm.setSettings(plain(HeadMouseMode::Joystick));
        auto t = run(hm, [](double) { return Head{.yaw = 0.9, .pitch = -0.4}; }, 30, 1);  // n < 0.05
        QCOMPARE(t.dx, 0L);
        QCOMPARE(t.dy, 0L);
    }

    void radialDeadzone()
    {
        // Each axis alone is inside the deadzone, together they are outside.
        HeadMouseSettings s = plain(HeadMouseMode::Joystick);
        s.pointer.deadzone = 0.1;
        s.pointer.y.range_negative = s.pointer.y.range_positive = 20;
        HeadMouse hm;
        hm.setSettings(s);
        auto t = run(hm, [](double) { return Head{.yaw = 1.8, .pitch = 1.8}; }, 30, 1);  // 0.09 each, 0.127 total
        QVERIFY(t.dx > 0);
        QVERIFY(t.dy > 0);
    }

    // ------------------------------------------------------------ frame rate

    void frameRateIndependence_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<double>("smoothing");
        QTest::newRow("relative") << int(HeadMouseMode::Relative) << 0.0;
        QTest::newRow("relative smoothed") << int(HeadMouseMode::Relative) << 0.5;
        QTest::newRow("joystick") << int(HeadMouseMode::Joystick) << 0.0;
        QTest::newRow("joystick smoothed") << int(HeadMouseMode::Joystick) << 0.5;
    }

    void frameRateIndependence()
    {
        QFETCH(int, mode);
        QFETCH(double, smoothing);
        auto total = [&](double fps) {
            HeadMouseSettings s = plain(HeadMouseMode(mode));
            s.pointer.smoothing = smoothing;
            HeadMouse hm;
            hm.setSettings(s);
            return run(hm, turn(15, 1), fps, 3).dx;
        };
        const double at15 = total(15);
        const double at30 = total(30);
        const double at60 = total(60);
        const double tolerance = std::max(3.0, 0.05 * at60);
        QVERIFY2(std::abs(at15 - at60) <= tolerance && std::abs(at30 - at60) <= tolerance,
                 qPrintable(QString("15 fps %1, 30 fps %2, 60 fps %3").arg(at15).arg(at30).arg(at60)));
    }

    void droppedFrames()
    {
        // Same motion, with every third frame of 30 fps dropped.
        auto total = [](bool drop) {
            HeadMouse hm;
            hm.setSettings(plain(HeadMouseMode::Joystick));
            long dx = 0;
            for (int i = 0; i <= 90; ++i) {
                if (drop && i % 3 == 1)
                    continue;
                const double t = i / 30.0;
                dx += hm.process(signalsOf(turn(15, 1)(t)), t * 1000).dx;
            }
            return dx;
        };
        const long all = total(false);
        const long dropped = total(true);
        QVERIFY2(std::abs(all - dropped) <= std::max(3L, all / 20), qPrintable(QString("%1 %2").arg(all).arg(dropped)));
    }

    // ------------------------------------------------------------ accumulator

    void accumulatorCarriesFractions()
    {
        // Joystick just past the deadzone: well under a pixel per frame.
        HeadMouse hm;
        hm.setSettings(plain(HeadMouseMode::Joystick));
        auto t = run(hm, [](double) { return Head{.yaw = 1.2}; }, 60, 5);  // n = 0.06
        const double expected = std::pow(0.01 / 0.95, 1.5) * 1400.0 * 5;
        QVERIFY2(t.dx > 0, "slow movement stalled");
        QVERIFY2(std::abs(t.dx - expected) <= 1.0, qPrintable(QString("%1 vs %2").arg(t.dx).arg(expected)));
        int largest = 0;
        // Never more than one pixel in a frame at this speed.
        HeadMouse again;
        again.setSettings(plain(HeadMouseMode::Joystick));
        for (int i = 0; i < 300; ++i)
            largest = std::max(largest, std::abs(again.process(signalsOf({.yaw = 1.2}), i * 1000.0 / 60).dx));
        QCOMPARE(largest, 1);
    }

    void relativeManySmallSteps()
    {
        // 0.004 degrees per frame is far less than a pixel per frame; it must
        // still add up, the same as one big move at the same speed would.
        HeadMouseSettings s = plain();
        s.pointer.deadzone = 0;
        HeadMouse hm;
        hm.setSettings(s);
        auto t = run(hm, [](double sec) { return Head{.yaw = sec * 0.24}; }, 60, 10);  // 2.4 degrees total
        // speed 0.012 units/s: acceleration 0.4072; 2.4 / 20 * 900 * 0.4072 = 43.98
        QVERIFY2(std::abs(t.dx - 44) <= 1, qPrintable(QString::number(t.dx)));
    }

    // ------------------------------------------------------------ face lost

    void resetAvoidsJumps()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        run(hm, turn(10, 0.5), 30, 1);
        hm.reset();  // face lost
        // The face comes back turned much further: no jump.
        auto out = hm.process(signalsOf({.yaw = 25}), 3000);
        QCOMPARE(out.dx, 0);
        QCOMPARE(hm.settings().pointer.x.neutral, std::optional<double>(0));  // kept
        auto next = hm.process(signalsOf({.yaw = 25}), 3033);
        QCOMPARE(next.dx, 0);
        // And it follows from there.
        auto moved = hm.process(signalsOf({.yaw = 28}), 3066);
        QVERIFY(moved.dx > 0);
    }

    // ------------------------------------------------------------ sources

    void anySourceDrivesAnAxis_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<double>("range");
        QTest::addColumn<int>("which");  // what changes: 0 yaw, 1 x, 2 roll, 3 jaw
        QTest::newRow("headYaw") << "headYaw" << 20.0 << 0;
        QTest::newRow("headX") << "headX" << 5.0 << 1;
        QTest::newRow("headRoll") << "headRoll" << 15.0 << 2;
        QTest::newRow("blendshape") << "jawOpen" << 50.0 << 3;
        QTest::newRow("compound") << "headYaw + 2 * headX" << 20.0 << 1;
    }

    void anySourceDrivesAnAxis()
    {
        QFETCH(QString, source);
        QFETCH(double, range);
        QFETCH(int, which);
        HeadMouseSettings s = plain(HeadMouseMode::Joystick);
        s.pointer.x.source = source.toStdString();
        s.pointer.x.range_negative = s.pointer.x.range_positive = range;
        HeadMouse hm;
        hm.setSettings(s);
        QVERIFY(hm.sourceError(HeadMouse::PointerX).empty());
        auto t = run(hm, [&](double) {
            Head h;
            if (which == 0) h.yaw = range / 2;
            if (which == 1) h.x = range / 2;
            if (which == 2) h.roll = range / 2;
            if (which == 3) h.jawOpen = range / 2;
            return h;
        }, 30, 1);
        QVERIFY2(t.dx > 100, qPrintable(QString::number(t.dx)));
        QCOMPARE(t.dy, 0L);
    }

    void badSourceTurnsOnlyItsAxisOff()
    {
        HeadMouseSettings s = plain(HeadMouseMode::Joystick);
        s.pointer.x.source = "headYaw +";
        HeadMouse hm;
        hm.setSettings(s);
        QVERIFY(!hm.sourceError(HeadMouse::PointerX).empty());
        QVERIFY(hm.sourceError(HeadMouse::PointerY).empty());
        auto t = run(hm, [](double) { return Head{.yaw = 10, .pitch = 10}; }, 30, 1);
        QCOMPARE(t.dx, 0L);
        QVERIFY(t.dy > 0);
        QVERIFY(!t.last.axes[HeadMouse::PointerX].active);

        s.pointer.x.source = "headYaw";  // fixed
        hm.setSettings(s);
        QVERIFY(hm.sourceError(HeadMouse::PointerX).empty());

        s.pointer.x.source = "unknownName";
        hm.setSettings(s);
        QVERIFY(!hm.sourceError(HeadMouse::PointerX).empty());
    }

    void asymmetricRange()
    {
        HeadMouseSettings s = plain();
        s.pointer.x.range_negative = 10;
        s.pointer.x.range_positive = 20;
        HeadMouse hm;
        hm.setSettings(s);
        QCOMPARE(hm.process(signalsOf({.yaw = -5}), 0).axes[0].normalised, -0.5);
        QCOMPARE(hm.process(signalsOf({.yaw = 10}), 33).axes[0].normalised, 0.5);
    }

    // ------------------------------------------------------------ clutch, freeze, enabled

    void clutch()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        hm.process(signalsOf({}), 0);
        hm.setHeld(true);
        auto held = run(hm, turn(10, 0.5), 30, 1, 0.033);
        QCOMPARE(held.dx, 0L);
        hm.setHeld(false);
        auto out = hm.process(signalsOf({.yaw = 10}), 1100);
        QCOMPARE(out.dx, 0);  // repositioned, no jump
        QVERIFY(hm.process(signalsOf({.yaw = 12}), 1133).dx > 0);
    }

    void clickFreeze()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        hm.process(signalsOf({}), 0);
        hm.freezeUntil(150);
        int moved_while_frozen = 0;
        for (int i = 1; i <= 4; ++i)  // 33..133 ms
            moved_while_frozen += std::abs(hm.process(signalsOf({.yaw = i * 1.0}), i * 33.3).dx);
        QCOMPARE(moved_while_frozen, 0);
        auto after = hm.process(signalsOf({.yaw = 5.0}), 166);
        QVERIFY(after.dx > 0);
        QVERIFY2(after.dx < 60, qPrintable(QString::number(after.dx)));  // one frame's worth, not the frozen motion
    }

    void disabledStillReads()
    {
        HeadMouseSettings s = plain(HeadMouseMode::Joystick);
        s.enabled = false;
        HeadMouse hm;
        hm.setSettings(s);
        auto t = run(hm, [](double) { return Head{.yaw = 10}; }, 30, 1);
        QCOMPARE(t.dx, 0L);
        QCOMPARE(t.last.axes[HeadMouse::PointerX].normalised, 0.5);
        QVERIFY(t.last.axes[HeadMouse::PointerX].active);
        hm.setEnabled(true);
        QVERIFY(hm.takeSettingsChanged());
        QVERIFY(hm.process(signalsOf({.yaw = 10}), 2000).dx > 0);
    }

    // ------------------------------------------------------------ calibration

    void calibration()
    {
        HeadMouse hm;
        hm.setSettings(plain());
        hm.startCalibration(2000);
        QVERIFY(hm.calibrating());
        // Swings between -8 and +12 degrees, with a couple of wild samples.
        long moved = 0;
        for (int i = 0; i <= 61; ++i) {
            double yaw = i % 2 ? 12.0 * ((i % 4) == 1 ? 1.0 : 0.6) : -8.0 * ((i % 4) == 0 ? 1.0 : 0.6);
            if (i == 10)
                yaw = 40;
            if (i == 20)
                yaw = -30;
            moved += std::abs(hm.process(signalsOf({.yaw = yaw}), i * 33.3).dx);
        }
        QCOMPARE(moved, 0L);  // nothing moves while calibrating
        QVERIFY(!hm.calibrating());
        QVERIFY(hm.takeSettingsChanged());
        const auto& x = hm.settings().pointer.x;
        QVERIFY2(std::abs(x.range_positive - 12) < 0.01, qPrintable(QString::number(x.range_positive)));
        QVERIFY2(std::abs(x.range_negative - 8) < 0.01, qPrintable(QString::number(x.range_negative)));
        // No movement on pitch: its range is left alone.
        QCOMPARE(hm.settings().pointer.y.range_positive, 15.0);
    }

    // ------------------------------------------------------------ scroll, absolute

    void scroll()
    {
        HeadMouseSettings s = plain();
        s.scroll.y.source = "headRoll";
        s.scroll.y.neutral = 0;
        s.scroll.smoothing = 0;
        HeadMouse hm;
        hm.setSettings(s);
        auto t = run(hm, [](double) { return Head{.roll = 15}; }, 30, 1);  // full deflection
        // ((1 - 0.05) / 0.95)^1.5 * 12 notches/s for 1 s.
        QVERIFY2(std::abs(t.scroll_y - 12) <= 1, qPrintable(QString::number(t.scroll_y)));
        QCOMPARE(t.scroll_x, 0L);
        QCOMPARE(t.dx, 0L);
        QCOMPARE(t.dy, 0L);
    }

    void absolute()
    {
        HeadMouse hm;
        hm.setSettings(plain(HeadMouseMode::Absolute));
        auto out = hm.process(signalsOf({.yaw = 20, .pitch = -15}), 0);
        QVERIFY(out.absolute);
        QCOMPARE((*out.absolute)[0], 1.0);
        QCOMPARE((*out.absolute)[1], -1.0);
        out = hm.process(signalsOf({.yaw = 0.5}), 33);  // inside the deadzone: center
        QCOMPARE((*out.absolute)[0], 0.0);
        QCOMPARE(out.dx, 0);
    }
};

QTEST_APPLESS_MAIN(TestHeadMouse)
#include "tst_head_mouse.moc"
