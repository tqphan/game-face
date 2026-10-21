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
        QVERIFY(!s->settle_on_stop);    // not in the original file: default off
        QVERIFY(s->start_sound.empty());  // no sounds by default
        QVERIFY(s->stop_sound.empty());
        QCOMPARE(s->mesh_lips.color, std::string("#0b006f"));  // mesh defaults
        QVERIFY(s->mesh_lips.visible);
        QCOMPARE(s->mesh_contour_width, 1.0);
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

    void meshColorsAndWidths()
    {
        // An ordinary string: moc misreads '#' inside a multi-line raw string.
        auto s = settingsFromJson("{\"mesh.eyes.color\": \"#80ff0000\", \"mesh.lips.color\": \"red\","
                                  " \"mesh.irises.color\": \"#12345g\", \"mesh.faceOval.visible\": false,"
                                  " \"mesh.contour.width\": 20, \"mesh.tessellation.width\": 0.1}");
        QVERIFY(s.has_value());
        QCOMPARE(s->mesh_eyes.color, std::string("#80ff0000"));
        QCOMPARE(s->mesh_lips.color, Settings{}.mesh_lips.color);      // not hex: default
        QCOMPARE(s->mesh_irises.color, Settings{}.mesh_irises.color);  // bad digit: default
        QVERIFY(!s->mesh_face_oval.visible);
        QCOMPARE(s->mesh_contour_width, 6.0);       // clamped
        QCOMPARE(s->mesh_tessellation_width, 0.5);  // clamped
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
        s.settle_on_stop = true;
        s.start_sound = "click1";
        s.stop_sound = "mouserelease1";
        s.sound_volume = 0.75;
        s.mesh_eyebrows = {"#123456", false};
        s.mesh_contour_width = 3.5;

        auto reread = settingsFromJson(settingsToJson(s));
        QVERIFY(reread.has_value());
        QCOMPARE(reread->theme, s.theme);
        QCOMPARE(reread->camera_id, s.camera_id);
        QCOMPARE(reread->presence_confidence, 0.25);
        QVERIFY(reread->allow_input_simulation);
        QVERIFY(reread->lock_ui);
        QVERIFY(!reread->confirm_on_close);
        QVERIFY(reread->settle_on_stop);
        QCOMPARE(reread->start_sound, s.start_sound);
        QCOMPARE(reread->stop_sound, s.stop_sound);
        QCOMPARE(reread->sound_volume, 0.75);
        QCOMPARE(reread->mesh_eyebrows.color, std::string("#123456"));
        QVERIFY(!reread->mesh_eyebrows.visible);
        QCOMPARE(reread->mesh_contour_width, 3.5);
    }

    void textScale()
    {
        QCOMPARE(Settings{}.text_scale, 1.25);  // larger than Material's default text
        auto s = settingsFromJson(R"({"text.scale": 1.5})");
        QVERIFY(s.has_value());
        QCOMPARE(s->text_scale, 1.5);
        QCOMPARE(settingsFromJson(settingsToJson(*s))->text_scale, 1.5);
        QCOMPARE(settingsFromJson(R"({"text.scale": 9})")->text_scale, 2.0);    // clamped
        QCOMPARE(settingsFromJson(R"({"text.scale": 0.2})")->text_scale, 1.0);  // clamped
    }

    void headMouseDefaults()
    {
        // Files from before the head mouse load with its defaults.
        auto s = loadSettings(std::filesystem::path(GAME_FACE_TEST_DATA_DIR) / "user.settings.json");
        QVERIFY(s.has_value());
        const HeadMouseSettings& hm = s->head_mouse;
        QVERIFY(!hm.enabled);
        QCOMPARE(hm.click_freeze_ms, 150.0);
        QCOMPARE(hm.pointer.mode, HeadMouseMode::Relative);
        QCOMPARE(hm.pointer.x.source, std::string("headYaw"));
        QCOMPARE(hm.pointer.y.source, std::string("headPitch"));
        QVERIFY(!hm.pointer.x.neutral);
        QCOMPARE(hm.scroll.mode, HeadMouseMode::Joystick);
        QVERIFY(hm.scroll.x.source.empty());
        QVERIFY(hm.scroll.y.source.empty());
    }

    void headMouseRoundTrip()
    {
        Settings s;
        HeadMouseSettings& hm = s.head_mouse;
        hm.enabled = true;
        hm.click_freeze_ms = 200;
        hm.pointer.mode = HeadMouseMode::Joystick;
        hm.pointer.deadzone = 0.1;
        hm.pointer.smoothing = 0.8;
        hm.pointer.x.source = "headYaw + 2 * headX";
        hm.pointer.x.neutral = -3.25;
        hm.pointer.x.range_negative = 12;
        hm.pointer.x.range_positive = 18;
        hm.pointer.x.gain = 1.5;
        hm.pointer.x.invert = true;
        hm.scroll.mode = HeadMouseMode::Relative;
        hm.scroll.y.source = "headRoll";

        auto reread = settingsFromJson(settingsToJson(s));
        QVERIFY(reread.has_value());
        const HeadMouseSettings& r = reread->head_mouse;
        QVERIFY(r.enabled);
        QCOMPARE(r.click_freeze_ms, 200.0);
        QCOMPARE(r.pointer.mode, HeadMouseMode::Joystick);
        QCOMPARE(r.pointer.deadzone, 0.1);
        QCOMPARE(r.pointer.smoothing, 0.8);
        QCOMPARE(r.pointer.x.source, hm.pointer.x.source);
        QCOMPARE(r.pointer.x.neutral, std::optional<double>(-3.25));
        QCOMPARE(r.pointer.x.range_negative, 12.0);
        QCOMPARE(r.pointer.x.range_positive, 18.0);
        QCOMPARE(r.pointer.x.gain, 1.5);
        QVERIFY(r.pointer.x.invert);
        QVERIFY(!r.pointer.y.neutral);  // unset stays unset
        QCOMPARE(r.scroll.mode, HeadMouseMode::Relative);
        QCOMPARE(r.scroll.y.source, std::string("headRoll"));
        QVERIFY(r.scroll.x.source.empty());
    }

    void headMouseMalformed()
    {
        auto s = settingsFromJson(R"({"head.mouse.pointer.mode": "teleport", "head.mouse.pointer.x.gain": 99,
                                      "head.mouse.pointer.x.range.negative": -4, "head.mouse.pointer.deadzone": 3,
                                      "head.mouse.pointer.x.neutral": "abc", "head.mouse.scroll.y.source": 7})");
        QVERIFY(s.has_value());
        const HeadMouseSettings& hm = s->head_mouse;
        QCOMPARE(hm.pointer.mode, HeadMouseMode::Relative);
        QCOMPARE(hm.pointer.x.gain, 10.0);             // clamped
        QCOMPARE(hm.pointer.x.range_negative, 20.0);   // not positive: default
        QCOMPARE(hm.pointer.deadzone, 0.9);            // clamped
        QVERIFY(!hm.pointer.x.neutral);                // not a number: unset
        QVERIFY(hm.scroll.y.source.empty());           // not a string
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
