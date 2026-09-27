#include <game_face/core/binding_engine.h>

#include <QTest>

#include <string>
#include <vector>

using namespace game_face;
using Commands = std::vector<std::string>;

namespace {

std::size_t index(std::string_view name)
{
    return *blendshapeIndex(name);
}

BlendshapeScores scores(std::string_view name, double value)
{
    BlendshapeScores s{};
    s[index(name)] = value;
    return s;
}

Binding simpleBinding(std::string blendshape, double threshold, std::string start, std::string stop)
{
    Binding b;
    b.simplified = true;
    b.simple = {std::move(blendshape), threshold, std::move(start), std::move(stop)};
    return b;
}

Binding advancedBinding(std::string start_expr, std::string start_cmd,
                        std::string stop_expr, std::string stop_cmd, double debounce_ms = 0)
{
    Binding b;
    b.simplified = false;
    b.start = {std::move(start_expr), std::move(start_cmd), debounce_ms};
    b.stop = {std::move(stop_expr), std::move(stop_cmd), debounce_ms};
    return b;
}

struct Harness {
    Commands sent;
    BindingEngine engine{[this](const std::string& c) { sent.push_back(c); }};

    explicit Harness(std::vector<Binding> bindings)
    {
        engine.setProfile(Profile{"test", std::move(bindings)});
    }

    Commands feed(std::string_view name, std::initializer_list<double> values, double step_ms = 33)
    {
        sent.clear();
        for (double v : values) {
            engine.process(scores(name, v), time_);
            time_ += step_ms;
        }
        return sent;
    }

    double time_ = 0;
};

} // namespace

class TestBindingEngine : public QObject {
    Q_OBJECT

private slots:
    void simpleFiresOncePerCrossing()
    {
        Harness h({simpleBinding("jawOpen", 50, "keyboard.press(KEY_A)", "keyboard.release(KEY_A)")});
        QCOMPARE(h.feed("jawOpen", {0, 10, 60, 70, 80}), Commands{"keyboard.press(KEY_A)"});
        QVERIFY(h.engine.status()[0].simple_active);
        QCOMPARE(h.feed("jawOpen", {70, 40, 30, 60}),
                 (Commands{"keyboard.release(KEY_A)", "keyboard.press(KEY_A)"}));
    }

    void simpleIgnoresValueEqualToThreshold()
    {
        Harness h({simpleBinding("jawOpen", 50, "start", "stop")});
        QCOMPARE(h.feed("jawOpen", {50, 50}), Commands{});
        QCOMPARE(h.feed("jawOpen", {51, 50}), Commands{"start"});
        QCOMPARE(h.feed("jawOpen", {50, 49}), Commands{"stop"});
    }

    void simpleWithUnknownBlendshapeDoesNothing()
    {
        Harness h({simpleBinding("notAShape", 10, "start", "stop")});
        QCOMPARE(h.feed("jawOpen", {0, 100}), Commands{});
    }

    void blankCommandsAreNotSent()
    {
        Harness h({simpleBinding("jawOpen", 50, "  ", "")});
        QCOMPARE(h.feed("jawOpen", {60, 40}), Commands{});
    }

    void advancedPressAndRelease()
    {
        // The original default profile: press above 40, release below 40.
        Harness h({advancedBinding("browInnerUp > 40", "press", "browInnerUp < 40", "release")});
        // The stop condition is already true on the first frame but must not fire.
        QCOMPARE(h.feed("browInnerUp", {0, 10}), Commands{});
        QCOMPARE(h.feed("browInnerUp", {50, 60, 45}), Commands{"press"});
        QVERIFY(h.engine.status()[0].start_active);
        QCOMPARE(h.feed("browInnerUp", {30, 20}), Commands{"release"});
        QCOMPARE(h.feed("browInnerUp", {40}), Commands{});  // neither condition
        QCOMPARE(h.feed("browInnerUp", {41}), Commands{"press"});
    }

    void debounceNeedsSustainedCondition()
    {
        Harness h({advancedBinding("jawOpen > 50", "press", "jawOpen < 50", "release", 100)});
        // 33 ms steps: fires on the first frame more than 100 ms after it began.
        QCOMPARE(h.feed("jawOpen", {60, 60, 60, 60}), Commands{});
        QCOMPARE(h.feed("jawOpen", {60}), Commands{"press"});
    }

    void debounceResetsWhenInterrupted()
    {
        Harness h({advancedBinding("jawOpen > 50", "press", "", "", 100)});
        QCOMPARE(h.feed("jawOpen", {60, 60, 60, 0, 60, 60, 60, 60}), Commands{});
        QCOMPARE(h.feed("jawOpen", {60}), Commands{"press"});
    }

    void invalidExpressionsReportErrorsAndNeverFire()
    {
        Harness h({advancedBinding("jawOpn > 5", "press", "", "release")});
        QVERIFY(h.engine.status()[0].start_error.find("unknown name") != std::string::npos);
        QVERIFY(h.engine.status()[0].stop_error.find("empty") != std::string::npos);
        QCOMPARE(h.feed("jawOpen", {0, 100, 0}), Commands{});
    }

    void disabledBindingsAreSkipped()
    {
        Binding b = simpleBinding("jawOpen", 50, "start", "stop");
        b.enabled = false;
        Harness h({b});
        QCOMPARE(h.feed("jawOpen", {100}), Commands{});
    }

    void bindingsAreIndependent()
    {
        Harness h({simpleBinding("jawOpen", 50, "jaw", "jaw-up"),
                   simpleBinding("eyeBlinkLeft", 50, "blink", "blink-up")});
        BlendshapeScores both{};
        both[index("jawOpen")] = 60;
        both[index("eyeBlinkLeft")] = 60;
        h.engine.process(both, 0);
        QCOMPARE(h.sent, (Commands{"jaw", "blink"}));
    }

    void resetRearms()
    {
        Harness h({simpleBinding("jawOpen", 50, "start", "stop")});
        QCOMPARE(h.feed("jawOpen", {60}), Commands{"start"});
        h.engine.reset();
        QVERIFY(!h.engine.status()[0].simple_active);
        QCOMPARE(h.feed("jawOpen", {60}), Commands{"start"});
    }

    void updateProfileKeepsState()
    {
        Harness h({simpleBinding("jawOpen", 50, "start", "stop")});
        QCOMPARE(h.feed("jawOpen", {60}), Commands{"start"});
        // Dragging the threshold while the key is held must not press it again.
        h.engine.updateProfile(Profile{"test", {simpleBinding("jawOpen", 55, "start", "stop")}});
        QVERIFY(h.engine.status()[0].simple_active);
        QCOMPARE(h.feed("jawOpen", {60, 57}), Commands{});
        QCOMPARE(h.feed("jawOpen", {54}), Commands{"stop"});
    }

    void updateProfileResetsNewBindings()
    {
        Harness h({simpleBinding("jawOpen", 50, "a", "")});
        (void)h.feed("jawOpen", {60});
        h.engine.updateProfile(Profile{"test", {simpleBinding("jawOpen", 50, "a", ""),
                                                simpleBinding("jawOpen", 50, "b", "")}});
        QCOMPARE(h.feed("jawOpen", {61}), Commands{"b"});
    }

    void setProfileReplacesBindings()
    {
        Harness h({simpleBinding("jawOpen", 50, "old", "")});
        h.engine.setProfile(Profile{"other", {simpleBinding("jawOpen", 50, "new", "")}});
        QCOMPARE(h.feed("jawOpen", {60}), Commands{"new"});
        QCOMPARE(h.engine.status().size(), std::size_t(1));
    }
};

QTEST_APPLESS_MAIN(TestBindingEngine)
#include "tst_binding_engine.moc"
