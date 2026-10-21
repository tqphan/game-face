// Real pointer movement through the whole chain, for CI only: rendered head
// motion -> tracking -> head mouse -> SendInput, read back with GetCursorPos
// (docs/head-mouse-plan.md, 8.5). It moves the real pointer and briefly
// changes the Windows "Enhance pointer precision" setting, so it skips itself
// unless GAME_FACE_CI_INJECTION=1. Never set that on a machine someone uses.

#include "worker_harness.h"

#include <QTest>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace game_face;
using harness::motion;

class TestInjectionCI : public QObject {
    Q_OBJECT

    harness::Renderer renderer_;

#ifdef Q_OS_WIN
    struct Measured {
        long computed = 0;  // what the head mouse sent
        long moved = 0;     // how far the cursor went
    };

    // Plays a small turn with the real input platform and measures it.
    Measured turn()
    {
        harness::Run run;
        TrackingWorker worker;
        harness::attach(worker, run);
        if (!worker.start(harness::trackerConfig()).isEmpty())
            return {};
        HeadMouseSettings s;
        s.enabled = true;
        s.pointer.x.gain = 0.3;  // stays on screen
        worker.setHeadMouseSettings(s);
        worker.setInjectionEnabled(true);

        SetCursorPos(GetSystemMetrics(SM_CXSCREEN) / 2 - 200, GetSystemMetrics(SM_CYSCREEN) / 2);
        POINT before{};
        GetCursorPos(&before);
        harness::play(worker, renderer_,
                      motion(R"({"fps": 15, "steps": [{"hold": 0.3}, {"to": {"yaw": 15}, "over": 0.8}, {"hold": 0.3}]})"));
        POINT after{};
        GetCursorPos(&after);
        return {run.dx(), after.x - before.x};
    }

    static void setPointerPrecision(bool on)
    {
        int mouse[3] = {};
        SystemParametersInfoW(SPI_GETMOUSE, 0, mouse, 0);
        mouse[2] = on ? 1 : 0;  // acceleration on/off; thresholds stay
        if (on && mouse[0] == 0 && mouse[1] == 0) {
            mouse[0] = 6;  // Windows' defaults
            mouse[1] = 10;
        }
        SystemParametersInfoW(SPI_SETMOUSE, 0, mouse, 0);
    }

    static bool pointerPrecision()
    {
        int mouse[3] = {};
        SystemParametersInfoW(SPI_GETMOUSE, 0, mouse, 0);
        return mouse[2] != 0;
    }
#endif

private slots:
    void initTestCase()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows only");
#else
        if (qEnvironmentVariable("GAME_FACE_CI_INJECTION") != "1")
            QSKIP("moves the real pointer; CI only (set GAME_FACE_CI_INJECTION=1)");
        if (!harness::assetsAvailable())
            QSKIP("test assets weren't downloaded (cmake/FetchTestAssets.cmake)");
        QVERIFY(renderer_.load());
#endif
    }

    void pointerMovesAsComputed()
    {
#ifdef Q_OS_WIN
        const bool original = pointerPrecision();
        setPointerPrecision(false);
        const Measured plain = turn();
        setPointerPrecision(true);
        const Measured accelerated = turn();
        setPointerPrecision(original);

        qInfo("Enhance pointer precision off: sent %ld px, cursor moved %ld px", plain.computed, plain.moved);
        qInfo("Enhance pointer precision on:  sent %ld px, cursor moved %ld px", accelerated.computed,
              accelerated.moved);
        QVERIFY2(plain.computed > 40, "the head mouse didn't move");
        if (plain.moved == 0)
            QSKIP("the cursor didn't move: no interactive desktop for SendInput");
        // Without acceleration (and at the default pointer speed) Windows
        // moves the cursor exactly as far as it was told.
        QVERIFY2(std::abs(plain.moved - plain.computed) <= std::max(2L, plain.computed / 20),
                 qPrintable(QString("sent %1, moved %2").arg(plain.computed).arg(plain.moved)));
#endif
    }
};

QTEST_GUILESS_MAIN(TestInjectionCI)
#include "tst_injection_ci.moc"
