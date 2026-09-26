#include <game_face/core/profiles.h>

#include <QTemporaryDir>
#include <QTest>

using namespace game_face;

namespace {

std::filesystem::path data(const char* name)
{
    return std::filesystem::path(GAME_FACE_TEST_DATA_DIR) / name;
}

} // namespace

class TestProfiles : public QObject {
    Q_OBJECT

private slots:
    void readsTheOriginalUserProfiles()
    {
        auto set = loadProfiles(data("user.profiles.json"));
        QVERIFY2(set.has_value(), set ? "" : set.error().message.c_str());
        QCOMPARE(set->profiles.size(), std::size_t(2));
        QCOMPARE(set->selection, 1);
        QCOMPARE(set->selected()->name, std::string("default"));

        const Profile& bed = set->profiles[0];
        QCOMPARE(bed.name, std::string("bed - dark"));
        QCOMPARE(bed.bindings.size(), std::size_t(6));

        const Binding& first = bed.bindings[0];
        QVERIFY(first.simplified);
        QVERIFY(first.enabled);
        QCOMPARE(first.simple.blendshape, std::string("browInnerUp"));
        QCOMPARE(first.simple.threshold, 2.0);  // stored as the string "2"
        QCOMPARE(first.simple.start_command, std::string("mouse.press(BTN_LEFT)"));
        QCOMPARE(first.simple.stop_command, std::string("mouse.release(BTN_LEFT)"));
        QCOMPARE(bed.bindings[1].simple.start_command, std::string("keyboard.press(KEY_KP4)"));
    }

    void readsTheOlderLayout()
    {
        auto set = loadProfiles(data("default.profile.json"));
        QVERIFY2(set.has_value(), set ? "" : set.error().message.c_str());
        QCOMPARE(set->profiles.size(), std::size_t(1));
        const auto& bindings = set->profiles[0].bindings;
        QCOMPARE(bindings.size(), std::size_t(4));
        QVERIFY(!bindings[0].simplified);
        QCOMPARE(bindings[0].start.expression, std::string("browInnerUp > 40"));
        QCOMPARE(bindings[0].start.command, std::string("{f14 down}"));
        QCOMPARE(bindings[0].stop.expression, std::string("browInnerUp < 40"));
    }

    void emptySetSelectsZero()
    {
        auto set = loadProfiles(data("empty.profile.json"));
        QVERIFY(set.has_value());
        QVERIFY(set->profiles.empty());
        QCOMPARE(set->selection, 0);
        QVERIFY(set->selected() == nullptr);
    }

    void selectionIsClamped()
    {
        auto set = profilesFromJson(R"({"selection": 9, "items": [{"name": "a"}, {"name": "b"}]})");
        QVERIFY(set.has_value());
        QCOMPARE(set->selection, 1);
    }

    void roundTrip()
    {
        auto original = loadProfiles(data("user.profiles.json"));
        QVERIFY(original.has_value());
        original->profiles[0].bindings[0].start = {"jawOpen > 5", "keyboard.tap(KEY_A)", 150};
        original->profiles[0].bindings[0].enabled = false;

        const std::string json = profilesToJson(*original);
        QVERIFY(json.find("\"schemaVersion\": 2") != std::string::npos);
        QVERIFY(json.find("\"activated\"") == std::string::npos);  // no runtime state
        QVERIFY(json.find("\"fn\"") == std::string::npos);

        auto reread = profilesFromJson(json);
        QVERIFY(reread.has_value());
        QCOMPARE(reread->selection, original->selection);
        QCOMPARE(reread->profiles.size(), original->profiles.size());
        const Binding& b = reread->profiles[0].bindings[0];
        QVERIFY(!b.enabled);
        QCOMPARE(b.start.expression, std::string("jawOpen > 5"));
        QCOMPARE(b.start.debounce_ms, 150.0);
        QCOMPARE(b.simple.threshold, 2.0);
    }

    void saveAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdU16String()) / "nested" / "profiles.json";

        ProfileSet set;
        set.profiles.push_back({"one", {Binding{}}});
        QVERIFY(saveProfiles(set, path).has_value());

        auto loaded = loadProfiles(path);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->profiles[0].name, std::string("one"));
        QCOMPARE(loaded->profiles[0].bindings.size(), std::size_t(1));
    }

    void rejectsBadInput()
    {
        QVERIFY(!profilesFromJson("not json").has_value());
        QVERIFY(!profilesFromJson("[]").has_value());
        auto newer = profilesFromJson(R"({"schemaVersion": 99, "items": []})");
        QVERIFY(!newer.has_value());
        QVERIFY(newer.error().message.find("newer version") != std::string::npos);
        QVERIFY(!loadProfiles(data("missing.json")).has_value());
    }
};

QTEST_APPLESS_MAIN(TestProfiles)
#include "tst_profiles.moc"
