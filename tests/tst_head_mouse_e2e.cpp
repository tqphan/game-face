// End to end: rendered head motion -> face tracking -> bindings and head
// mouse -> input, with input simulation on and the input recorded as text by
// input_devices_simulator's RecordingPlatform, which injects nothing
// (docs/head-mouse-plan.md, 8.3). Label: headmouse-e2e.

#include "worker_harness.h"

#include <input_devices_simulator/recording_platform.h>

#include <QTest>

#include <sstream>

using namespace game_face;
using harness::motion;
namespace ids = input_devices_simulator;

namespace {

struct Recorded {
    std::vector<std::string> lines;

    long sum(const char* kind, int field) const
    {
        long total = 0;
        for (const auto& line : lines) {
            std::istringstream in(line);
            std::string device, what;
            long a = 0, b = 0;
            in >> device >> what;
            if (device != "mouse" || what != kind)
                continue;
            in >> a >> b;
            total += field == 0 ? a : b;
        }
        return total;
    }
    int count(const std::string& prefix) const
    {
        return static_cast<int>(std::count_if(lines.begin(), lines.end(),
                                              [&](const std::string& l) { return l.starts_with(prefix); }));
    }
    std::optional<std::pair<long, long>> lastMoveTo() const
    {
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            std::istringstream in(*it);
            std::string device, what;
            long x = 0, y = 0;
            in >> device >> what >> x >> y;
            if (device == "mouse" && what == "moveTo")
                return std::pair{x, y};
        }
        return std::nullopt;
    }
};

} // namespace

class TestHeadMouseE2E : public QObject {
    Q_OBJECT

    harness::Renderer renderer_;

    // A started worker with input simulation on, recording into `recorded`.
    std::unique_ptr<TrackingWorker> makeWorker(harness::Run& run, Recorded& recorded, const HeadMouseSettings& settings,
                                               const Profile& profile = {})
    {
        auto worker = std::make_unique<TrackingWorker>([&recorded] {
            return std::make_unique<ids::RecordingPlatform>(
                [&recorded](std::string_view line) { recorded.lines.emplace_back(line); });
        });
        harness::attach(*worker, run);
        const QString error = worker->start(harness::trackerConfig());
        if (!error.isEmpty())
            qWarning("%s", qPrintable(error));
        worker->setHeadMouseSettings(settings);
        worker->setProfile(profile, false);
        worker->setScreenGeometry(QRect(0, 0, 1920, 1080));
        worker->setInjectionEnabled(true);
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

    // Turning right moves the pointer right; looking up moves it up, which
    // is negative y on screen.
    void pointerFollowsTheHead()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, enabled());
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 20}, "over": 0.8},
                                 {"to": {"pitch": 12}, "over": 0.8}, {"hold": 0.3}]})"));
        QVERIFY(run.errors.empty());
        QVERIFY2(recorded.sum("moveBy", 0) > 150, qPrintable(QString::number(recorded.sum("moveBy", 0))));
        QVERIFY2(recorded.sum("moveBy", 1) < -80, qPrintable(QString::number(recorded.sum("moveBy", 1))));
        // What was sent is what the mapper computed.
        QCOMPARE(recorded.sum("moveBy", 0), run.dx());
        QCOMPARE(recorded.sum("moveBy", 1), -run.dy());
    }

    void nothingWhenInjectionIsOff()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, enabled());
        worker->setInjectionEnabled(false);
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 20}, "over": 0.8}]})"));
        QCOMPARE(recorded.count("mouse moveBy"), 0);
    }

    void scrollFromRoll()
    {
        HeadMouseSettings s = enabled();
        s.pointer.x.source.clear();
        s.pointer.y.source.clear();
        s.scroll.y.source = "headRoll";
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, s);
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 15}, "over": 0.4}, {"hold": 1.5}]})"));
        const long notches = recorded.sum("scroll", 1);
        QVERIFY2(notches >= 8, qPrintable(QString::number(notches)));  // about 12 a second at full tilt
        QCOMPARE(recorded.count("mouse moveBy"), 0);
    }

    // A binding turns the head mouse on, and the new state is reported for saving.
    void toggleFromABinding()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, HeadMouseSettings{},
                                 harness::bindingProfile("headRoll > 12", "headmouse.toggle()"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 15}, "over": 0.6},
                                 {"to": {"yaw": 0}, "over": 0.6}, {"to": {"roll": 18}, "over": 0.4},
                                 {"to": {"roll": 0}, "over": 0.4}, {"to": {"yaw": 15}, "over": 0.6}, {"hold": 0.3}]})"));
        QVERIFY(run.errors.empty());
        const auto enabled_report = std::find_if(run.settings_reports.begin(), run.settings_reports.end(),
                                                 [](const HeadMouseSettings& s) { return s.enabled; });
        QVERIFY(enabled_report != run.settings_reports.end());
        // No movement from the first turn, while it was off; movement from the second.
        std::size_t toggled = 0;
        for (std::size_t i = 0; i < run.frames.size(); ++i) {
            if (run.frames[i].status.front().start_active) {
                toggled = i;
                break;
            }
        }
        QVERIFY(toggled > 0);
        long before = 0, after = 0;
        for (std::size_t i = 0; i < run.frames.size(); ++i)
            (i < toggled ? before : after) += std::abs(run.frames[i].head_mouse.dx);
        QCOMPARE(before, 0L);
        QVERIFY2(after > 80, qPrintable(QString::number(after)));
        QVERIFY(recorded.sum("moveBy", 0) > 80);
    }

    // hold() freezes the pointer while the head moves; release() resumes
    // without a jump.
    void clutch()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, enabled(),
                                 harness::bindingProfile("headRoll > 12", "headmouse.hold()", "headRoll < 4",
                                                         "headmouse.release()"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 18}, "over": 0.4},
                                 {"to": {"yaw": 20, "roll": 18}, "over": 0.8}, {"to": {"yaw": 20, "roll": 0}, "over": 0.4},
                                 {"hold": 0.5}]})"));
        QVERIFY(run.errors.empty());
        // The yaw turn happened while held.
        long held = 0;
        for (const auto& f : run.frames) {
            if (f.status.front().start_active && !f.status.front().stop_active)
                held += std::abs(f.head_mouse.dx);
        }
        QCOMPARE(held, 0L);
        QVERIFY2(std::abs(recorded.sum("moveBy", 0)) < 40, qPrintable(QString::number(recorded.sum("moveBy", 0))));
    }

    // Clicking with an expression moves the head a little; the pointer
    // holds still for click_freeze_ms after the click.
    void clickFreeze()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, enabled(), harness::bindingProfile("headRoll > 12", "mouse.click(BTN_LEFT)"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 18, "yaw": 6}, "over": 0.27},
                                 {"to": {"roll": 18, "yaw": 20}, "over": 0.8}, {"hold": 0.2}]})"));
        QCOMPARE(recorded.count("mouse BTN_LEFT down"), 1);
        // Find the click in the recording; no moveBy for the next 2 frames (133 ms).
        const auto click = std::find_if(recorded.lines.begin(), recorded.lines.end(),
                                        [](const std::string& l) { return l.starts_with("mouse BTN_LEFT down"); });
        std::size_t clicked_frame = 0;
        for (std::size_t i = 0; i < run.frames.size(); ++i) {
            if (run.frames[i].status.front().start_active) {
                clicked_frame = i;
                break;
            }
        }
        QVERIFY(click != recorded.lines.end() && clicked_frame > 0);
        QCOMPARE(run.frames[clicked_frame].head_mouse.dx, 0);
        QCOMPARE(run.frames[clicked_frame + 1].head_mouse.dx, 0);
        QCOMPARE(run.frames[clicked_frame + 2].head_mouse.dx, 0);
        // It moves again afterwards.
        long later = 0;
        for (std::size_t i = clicked_frame + 3; i < run.frames.size(); ++i)
            later += run.frames[i].head_mouse.dx;
        QVERIFY2(later > 50, qPrintable(QString::number(later)));
    }

    void absolute()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, enabled(HeadMouseMode::Absolute));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 22}, "over": 0.6}, {"hold": 0.4}]})"));
        const auto at = recorded.lastMoveTo();
        QVERIFY(at);
        QVERIFY2(at->first > 1700, qPrintable(QString("%1, %2").arg(at->first).arg(at->second)));  // right edge
        QVERIFY2(std::abs(at->second - 540) < 200, qPrintable(QString("%1, %2").arg(at->first).arg(at->second)));
    }

    void mixedCommandSendsNothing()
    {
        harness::Run run;
        Recorded recorded;
        auto worker = makeWorker(run, recorded, HeadMouseSettings{},
                                 harness::bindingProfile("headRoll > 12", "headmouse.toggle(); mouse.click(BTN_LEFT)"));
        harness::play(*worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"roll": 18}, "over": 0.4}, {"hold": 0.2}]})"));
        QCOMPARE(run.errors.size(), std::size_t(1));
        QCOMPARE(recorded.count("mouse BTN_LEFT"), 0);
    }
};

QTEST_GUILESS_MAIN(TestHeadMouseE2E)
#include "tst_head_mouse_e2e.moc"
