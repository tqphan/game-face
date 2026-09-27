#include <game_face/core/settings.h>

#include <QTemporaryDir>
#include <QTest>

using namespace game_face;

class TestSettings : public QObject {
    Q_OBJECT

private slots:
    void readsTheOriginalUserSettings()
    {
        auto s = loadSettings(std::filesystem::path(GAME_FACE_TEST_DATA_DIR) / "user.settings.json");
        QVERIFY2(s.has_value(), s ? "" : s.error().message.c_str());
        QCOMPARE(s->theme, std::string("dark"));
        QCOMPARE(s->language, std::string("en"));
        QCOMPARE(s->webcam_opacity, 0.0);         // stored as "0"
        QCOMPARE(s->landmarks_opacity, 1.0);
        QCOMPARE(s->tracking_confidence, 0.35);   // stored as "0.35"
        QCOMPARE(s->presence_confidence, 0.38);
        QCOMPARE(s->detection_confidence, 0.35);
        QVERIFY(s->auto_start_tracking);
        QVERIFY(s->auto_start_with_os);
        QVERIFY(!s->run_on_secure_desktop);
        QVERIFY(!s->allow_input_simulation);
        QVERIFY(s->auto_save_settings);
        QVERIFY(s->auto_save_profiles);
        QVERIFY(s->lock_ui);
        QVERIFY(s->camera_id.empty());  // webcam.deviceId is a browser ID; not imported
        QVERIFY(s->confirm_on_close);   // not in the original file: default on
    }

    void defaultsForMissingAndMalformedValues()
    {
        auto s = settingsFromJson(R"({"theme": "purple", "webcam.opacity": "abc",
                                      "tracking.confidence": 7, "lock.ui": "yes"})");
        QVERIFY(s.has_value());
        QCOMPARE(s->theme, std::string("dark"));
        QCOMPARE(s->webcam_opacity, 1.0);
        QCOMPARE(s->tracking_confidence, 1.0);  // clamped to 0..1
        QVERIFY(!s->lock_ui);
        QCOMPARE(s->language, std::string("en"));
    }

    void roundTrip()
    {
        Settings s;
        s.theme = "light";
        s.camera_id = "\\\\?\\usb#vid_046d&pid_082d";
        s.presence_confidence = 0.25;
        s.allow_input_simulation = true;
        s.lock_ui = true;
        s.confirm_on_close = false;

        auto reread = settingsFromJson(settingsToJson(s));
        QVERIFY(reread.has_value());
        QCOMPARE(reread->theme, s.theme);
        QCOMPARE(reread->camera_id, s.camera_id);
        QCOMPARE(reread->presence_confidence, 0.25);
        QVERIFY(reread->allow_input_simulation);
        QVERIFY(reread->lock_ui);
        QVERIFY(!reread->confirm_on_close);
    }

    void saveAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdU16String()) / "settings.json";
        Settings s;
        s.language = "de";
        QVERIFY(saveSettings(s, path).has_value());
        auto loaded = loadSettings(path);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->language, std::string("de"));
    }
};

QTEST_APPLESS_MAIN(TestSettings)
#include "tst_settings.moc"
