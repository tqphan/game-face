// The tracking worker on rendered head motion, with input simulation off:
// face tracking, bindings on head signals, the head mouse's computed
// movement and the headmouse.* commands. Input simulation stays off, so the
// input simulator is never created and nothing is injected or recorded;
// tst_head_mouse_e2e covers the input that comes out.

#include "worker_harness.h"

#include <QTest>

using namespace game_face;
using harness::motion;

class TestTrackingWorker : public QObject {
    Q_OBJECT

    harness::Renderer renderer_;

    // A started worker (input simulation off) with these head mouse settings.
    std::unique_ptr<TrackingWorker> makeWorker(harness::Run& run, const HeadMouseSettings& settings,
                                               const Profile& profile = {})
    {
        auto worker = std::make_unique<TrackingWorker>();
        harness::attach(*worker, run);
        const QString error = worker->start(harness::trackerConfig());
        if (!error.isEmpty())
            qWarning("%s", qPrintable(error));
        worker->setHeadMouseSettings(settings);
        worker->setProfile(profile, false);
        return worker;
    }

    static HeadMouseSettings enabled(HeadMouseMode mode = HeadMouseMode::Relative)
    {
        HeadMouseSettings s;
        s.enabled = true;
        s.pointer.mode = mode;
        return s;
    }

private slots:
    void initTestCase()
    {
        if (!harness::assetsAvailable())
            QSKIP("test assets weren't downloaded (cmake/FetchTestAssets.cmake)");
        QVERIFY(renderer_.load());
    }

    void headMouseFollowsATurn()
    {
        harness::Run run;
        auto worker = makeWorker(run, enabled());
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.4}, {"to": {"yaw": 20}, "over": 1}, {"hold": 0.4}]})"));
        QVERIFY(run.errors.empty());
        QVERIFY(std::all_of(run.frames.begin(), run.frames.end(), [](const FrameResult& f) { return f.face_found; }));
        const long dx = run.dx(), dy = run.dy();
        QVERIFY2(dx > 150, qPrintable(QString("dx %1").arg(dx)));
        QVERIFY2(std::abs(dy) < dx / 4, qPrintable(QString("dx %1 dy %2").arg(dx).arg(dy)));
        // The first frame captured the centre, so the end is a full deflection.
        const auto& last = run.frames.back().head_mouse.axes[HeadMouse::PointerX];
        QVERIFY2(std::abs(last.normalised - 1.0) < 0.25, qPrintable(QString::number(last.normalised)));
        QVERIFY(!run.settings_reports.empty());  // the captured neutral, to save
        QVERIFY(run.settings_reports.front().pointer.x.neutral.has_value());
    }

    void upIsPositive()
    {
        harness::Run run;
        auto worker = makeWorker(run, enabled());
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"pitch": 12}, "over": 0.8}, {"hold": 0.3}]})"));
        QVERIFY2(run.dy() > 80, qPrintable(QString("dy %1").arg(run.dy())));
        QVERIFY2(std::abs(run.dx()) < run.dy() / 3, qPrintable(QString("dx %1").arg(run.dx())));
    }

    void joystickKeepsMovingWhileTurned()
    {
        harness::Run run;
        auto worker = makeWorker(run, enabled(HeadMouseMode::Joystick));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 15}, "over": 0.4}, {"hold": 1.5}]})"));
        // Over the last second of the hold, it still moves every frame.
        int moving = 0;
        for (std::size_t i = run.frames.size() - 15; i < run.frames.size(); ++i)
            moving += run.frames[i].head_mouse.dx > 0;
        QVERIFY2(moving >= 14, qPrintable(QString("%1 of 15").arg(moving)));
    }

    void otherSources_data()
    {
        QTest::addColumn<QString>("preset");
        QTest::addColumn<QString>("script");
        QTest::newRow("head position") << "headPosition"
                                       << R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"x": 4}, "over": 0.8}, {"hold": 0.3}]})";
        QTest::newRow("nose point") << "nosePoint"
                                    << R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"x": 4}, "over": 0.8}, {"hold": 0.3}]})";
        QTest::newRow("tilt and nod") << "tiltAndNod"
                                      << R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 15}, "over": 0.8}, {"hold": 0.3}]})";
    }

    // Moving the head sideways, or tilting it, drives the pointer when the
    // preset says so.
    void otherSources()
    {
        QFETCH(QString, preset);
        QFETCH(QString, script);
        HeadMouseSettings s = enabled();
        applyHeadMousePreset(s.pointer, *findHeadMousePreset(preset.toStdString()));
        harness::Run run;
        auto worker = makeWorker(run, s);
        harness::play(*worker, renderer_, motion(script.toUtf8().constData()));
        QVERIFY2(run.dx() > 100, qPrintable(QString("dx %1 dy %2").arg(run.dx()).arg(run.dy())));
        QVERIFY2(std::abs(run.dy()) < run.dx() / 2, qPrintable(QString("dx %1 dy %2").arg(run.dx()).arg(run.dy())));
    }

    void faceLostDoesNotJump()
    {
        harness::Run run;
        auto worker = makeWorker(run, enabled());
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 10}, "over": 0.5},
                                 {"hide": 0.5}, {"to": {"yaw": 25}, "over": 0.0667}, {"hold": 0.5}]})"));
        // The first frame with the face back, 15 degrees further, moves nothing.
        std::size_t back = 0;
        for (std::size_t i = 1; i < run.frames.size(); ++i) {
            if (run.frames[i].face_found && !run.frames[i - 1].face_found) {
                back = i;
                break;
            }
        }
        QVERIFY(back > 0);
        QCOMPARE(run.frames[back].head_mouse.dx, 0);
        // Holding still afterwards moves nothing either.
        long after = 0;
        for (std::size_t i = back; i < run.frames.size(); ++i)
            after += std::abs(run.frames[i].head_mouse.dx);
        QVERIFY2(after < 5, qPrintable(QString::number(after)));
    }

    void headSignalsInBindings()
    {
        harness::Run run;
        auto worker = makeWorker(run, HeadMouseSettings{}, harness::bindingProfile("headYaw > 12", ""));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 20}, "over": 0.6}, {"hold": 0.3}]})"));
        QCOMPARE(run.start_edges, 1);
        QVERIFY(run.frames.back().status.front().start_active);
        QVERIFY(run.frames.back().status.front().start_error.empty());
    }

    // Recentering moves nothing, so a binding can do it with input simulation off.
    void recenterWorksWithInjectionOff()
    {
        harness::Run run;
        auto worker = makeWorker(run, enabled(), harness::bindingProfile("headRoll > 12", "headmouse.recenter()"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 10}, "over": 0.4},
                                 {"to": {"roll": 18}, "over": 0.4}, {"hold": 0.3}]})"));
        QVERIFY(run.errors.empty());
        QVERIFY(run.settings_reports.size() >= 2);  // first neutral, then the recenter
        const double first = *run.settings_reports.front().pointer.x.neutral;
        const double recentred = *run.settings_reports.back().pointer.x.neutral;
        QVERIFY2(recentred - first > 6, qPrintable(QString("%1 -> %2").arg(first).arg(recentred)));
    }

    // Toggling would move the pointer, so it needs input simulation.
    void toggleNeedsInjection()
    {
        HeadMouseSettings s;  // disabled
        harness::Run run;
        auto worker = makeWorker(run, s, harness::bindingProfile("headRoll > 12", "headmouse.toggle()"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 18}, "over": 0.4}, {"hold": 0.2}]})"));
        QCOMPARE(run.start_edges, 1);
        for (const auto& report : run.settings_reports)
            QVERIFY(!report.enabled);
    }

    void mixedCommandIsAnError()
    {
        harness::Run run;
        auto worker = makeWorker(run, HeadMouseSettings{},
                                 harness::bindingProfile("headRoll > 12", "headmouse.toggle(); mouse.click(BTN_LEFT)"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 18}, "over": 0.4}, {"hold": 0.2}]})"));
        QCOMPARE(run.errors.size(), std::size_t(1));
        QVERIFY2(run.errors.front().contains("combined"), qPrintable(run.errors.front()));
    }

    // headmouse.calibrate() from a binding, then a sweep: the ranges become
    // what the head actually did.
    void calibrateFromBinding()
    {
        harness::Run run;
        auto worker = makeWorker(run, enabled(), harness::bindingProfile("headRoll > 12", "headmouse.calibrate()"));
        // Tilt to start calibrating, come back, then sweep yaw -12..+16 for over 5 s.
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.2}, {"to": {"roll": 18}, "over": 0.3},
                                 {"to": {"roll": 0}, "over": 0.3},
                                 {"to": {"yaw": 16}, "over": 0.7}, {"to": {"yaw": -12}, "over": 1.2},
                                 {"to": {"yaw": 16}, "over": 1.2}, {"to": {"yaw": -12}, "over": 1.2},
                                 {"to": {"yaw": 0}, "over": 0.8}, {"hold": 0.5}]})"));
        QVERIFY(!run.frames.back().head_mouse_calibrating);
        QVERIFY(!run.settings_reports.empty());
        const auto& x = run.settings_reports.back().pointer.x;
        QVERIFY2(std::abs(x.range_positive - 16) < 4 && std::abs(x.range_negative - 12) < 4,
                 qPrintable(QString("ranges -%1 +%2").arg(x.range_negative).arg(x.range_positive)));
        // Nothing moved while calibrating.
        bool calibrating_seen = false;
        for (const auto& f : run.frames) {
            if (f.head_mouse_calibrating) {
                calibrating_seen = true;
                QCOMPARE(f.head_mouse.dx, 0);
            }
        }
        QVERIFY(calibrating_seen);
    }
};

QTEST_GUILESS_MAIN(TestTrackingWorker)
#include "tst_tracking_worker.moc"
