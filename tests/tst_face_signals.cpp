// Does MediaPipe report the head signals we expect, with the right signs?
// Runs the real face tracker on images whose head pose is known: renders of a
// 3D head scan, and a real photo rotated, shifted and scaled in software
// (docs/head-mouse-plan.md, 8.2). No camera.

#include <face_render.h>

#include <game_face/core/face_tracker.h>
#include <game_face/core/signals.h>

#include <QCoreApplication>
#include <QPainter>
#include <QTest>
#include <QTransform>

#include <cmath>
#include <memory>

using namespace game_face;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kLibrary = "libmediapipe.dll";
#elif defined(Q_OS_MACOS)
constexpr auto kLibrary = "libmediapipe.dylib";
#else
constexpr auto kLibrary = "libmediapipe.so";
#endif

std::filesystem::path besideTest(const QString& name)
{
    return std::filesystem::path((QCoreApplication::applicationDirPath() + "/" + name).toStdU16String());
}

std::filesystem::path asset(const char* name)
{
    return std::filesystem::path(GAME_FACE_TEST_ASSETS_DIR) / name;
}

double sig(const SignalValues& v, HeadSignal s)
{
    return v[signalIndex(s)];
}

struct Delta {
    double yaw, pitch, roll, x, y, z, nose_x, nose_y;
};

Delta delta(const SignalValues& a, const SignalValues& base)
{
    auto d = [&](HeadSignal s) { return sig(a, s) - sig(base, s); };
    return {d(HeadSignal::Yaw), d(HeadSignal::Pitch), d(HeadSignal::Roll), d(HeadSignal::X),
            d(HeadSignal::Y),   d(HeadSignal::Z),     d(HeadSignal::NoseX), d(HeadSignal::NoseY)};
}

QString describe(const Delta& d)
{
    return QString("dyaw %1 dpitch %2 droll %3 dx %4 dy %5 dz %6 dnoseX %7 dnoseY %8")
        .arg(d.yaw, 0, 'f', 2).arg(d.pitch, 0, 'f', 2).arg(d.roll, 0, 'f', 2)
        .arg(d.x, 0, 'f', 2).arg(d.y, 0, 'f', 2).arg(d.z, 0, 'f', 2)
        .arg(d.nose_x, 0, 'f', 3).arg(d.nose_y, 0, 'f', 3);
}

// The same image moved, turned or scaled about its center, on a grey canvas
// the same size.
QImage transformed(const QImage& image, double rotate_deg, QPointF shift, double scale)
{
    QImage out(image.size(), QImage::Format_RGB888);
    out.fill(QColor(120, 120, 120));
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.translate(image.width() / 2.0 + shift.x(), image.height() / 2.0 + shift.y());
    p.rotate(rotate_deg);  // clockwise on screen
    p.scale(scale, scale);
    p.drawImage(QPointF(-image.width() / 2.0, -image.height() / 2.0), image);
    p.end();
    return out;
}

} // namespace

class TestFaceSignals : public QObject {
    Q_OBJECT

    std::unique_ptr<FaceTracker> image_tracker_;
    std::unique_ptr<FaceTracker> video_tracker_;
    std::optional<face_render::HeadModel> head_;
    SignalValues frontal_{};
    QImage portrait_;
    SignalValues portrait_signals_{};

    std::unique_ptr<FaceTracker> makeTracker(FaceTrackerOptions::Mode mode)
    {
        FaceTrackerOptions options;
        options.library_path = besideTest(kLibrary);
        options.model_path = besideTest("face_landmarker.task");
        options.mode = mode;
        auto tracker = FaceTracker::create(options);
        if (!tracker)
            qWarning("%s", tracker.error().message.c_str());
        return tracker ? std::move(*tracker) : nullptr;
    }

    std::optional<SignalValues> track(FaceTracker& tracker, const QImage& image, std::int64_t time_ms = 0)
    {
        const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        auto frame = tracker.detect({rgb.constBits(), rgb.width(), rgb.height(), static_cast<int>(rgb.bytesPerLine())},
                                    time_ms);
        if (!frame || !frame->face_found || !frame->pose)
            return std::nullopt;
        return toSignals(*frame);
    }

    std::optional<SignalValues> trackPose(const face_render::HeadPose& pose, const face_render::Degrade& flaws = {})
    {
        return track(*image_tracker_, face_render::degrade(face_render::render(*head_, pose), flaws));
    }

private slots:
    void initTestCase()
    {
        if (!std::filesystem::exists(asset("LeePerrySmith.glb")) || !std::filesystem::exists(asset("portrait.jpg")))
            QSKIP("test assets weren't downloaded (cmake/FetchTestAssets.cmake)");
        image_tracker_ = makeTracker(FaceTrackerOptions::Mode::Image);
        video_tracker_ = makeTracker(FaceTrackerOptions::Mode::Video);
        QVERIFY(image_tracker_ && video_tracker_);

        auto head = face_render::HeadModel::load(asset("LeePerrySmith.glb"), asset("Map-COL.jpg"));
        QVERIFY2(head.has_value(), head ? "" : head.error().message.c_str());
        head_ = std::move(*head);

        auto frontal = trackPose({});
        QVERIFY2(frontal, "the frontal render wasn't recognised as a face");
        frontal_ = *frontal;
        // The renderer and MediaPipe agree on what "facing the camera at 50 cm" is.
        QVERIFY2(std::abs(sig(frontal_, HeadSignal::Yaw)) < 4 && std::abs(sig(frontal_, HeadSignal::Pitch)) < 4
                     && std::abs(sig(frontal_, HeadSignal::Roll)) < 4,
                 qPrintable(QString("frontal yaw %1 pitch %2 roll %3").arg(sig(frontal_, HeadSignal::Yaw))
                                .arg(sig(frontal_, HeadSignal::Pitch)).arg(sig(frontal_, HeadSignal::Roll))));
        QVERIFY2(std::abs(sig(frontal_, HeadSignal::Z) + face_render::kDefaultDistanceCm) < 3,
                 qPrintable(QString("frontal z %1").arg(sig(frontal_, HeadSignal::Z))));

        portrait_ = QImage(QString::fromStdU16String(asset("portrait.jpg").u16string()));
        QVERIFY(!portrait_.isNull());
        portrait_ = portrait_.scaledToWidth(640, Qt::SmoothTransformation);
        auto portrait = track(*image_tracker_, portrait_);
        QVERIFY2(portrait, "no face in portrait.jpg");
        portrait_signals_ = *portrait;
    }

    // ------------------------------------------------------------ renders

    void renderedPoses_data()
    {
        QTest::addColumn<double>("yaw");
        QTest::addColumn<double>("pitch");
        QTest::addColumn<double>("roll");
        QTest::addColumn<double>("x");
        QTest::addColumn<double>("y");
        QTest::addColumn<double>("z");
        QTest::newRow("turn right") << 20.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0;
        QTest::newRow("turn left") << -20.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0;
        QTest::newRow("look up") << 0.0 << 12.0 << 0.0 << 0.0 << 0.0 << 0.0;
        QTest::newRow("look down") << 0.0 << -12.0 << 0.0 << 0.0 << 0.0 << 0.0;
        QTest::newRow("tilt right") << 0.0 << 0.0 << 20.0 << 0.0 << 0.0 << 0.0;
        QTest::newRow("tilt left") << 0.0 << 0.0 << -20.0 << 0.0 << 0.0 << 0.0;
        QTest::newRow("move right") << 0.0 << 0.0 << 0.0 << 5.0 << 0.0 << 0.0;
        QTest::newRow("move left") << 0.0 << 0.0 << 0.0 << -5.0 << 0.0 << 0.0;
        QTest::newRow("move up") << 0.0 << 0.0 << 0.0 << 0.0 << 4.0 << 0.0;
        QTest::newRow("move down") << 0.0 << 0.0 << 0.0 << 0.0 << -4.0 << 0.0;
        QTest::newRow("lean in") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 10.0;
        QTest::newRow("lean back") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << -10.0;
    }

    // Each movement shows up in its own signal, with the right sign and about
    // the right size, and not much in the others.
    void renderedPoses()
    {
        QFETCH(double, yaw);
        QFETCH(double, pitch);
        QFETCH(double, roll);
        QFETCH(double, x);
        QFETCH(double, y);
        QFETCH(double, z);
        auto measured = trackPose({yaw, pitch, roll, x, y, z});
        QVERIFY2(measured, "no face");
        const Delta d = delta(*measured, frontal_);
        const QString info = describe(d);
        qInfo("%s", qPrintable(info));

        constexpr double kAngle = 4.5;     // degrees
        constexpr double kCrossTalk = 6;   // degrees in the other angles
        constexpr double kPosition = 1.2;  // cm
        QVERIFY2(std::abs(d.yaw - yaw) < (yaw ? kAngle : kCrossTalk), qPrintable(info));
        QVERIFY2(std::abs(d.pitch - pitch) < (pitch ? kAngle : kCrossTalk), qPrintable(info));
        QVERIFY2(std::abs(d.roll - roll) < (roll ? kAngle : kCrossTalk), qPrintable(info));
        if (x != 0) {
            QVERIFY2(std::abs(d.x - x) < kPosition, qPrintable(info));
            QVERIFY2((d.nose_x > 0) == (x > 0), qPrintable(info));
        }
        if (y != 0) {
            QVERIFY2(std::abs(d.y - y) < kPosition, qPrintable(info));
            QVERIFY2((d.nose_y > 0) == (y > 0), qPrintable(info));
        }
        if (z != 0)
            QVERIFY2(std::abs(d.z - z) < 2 * kPosition, qPrintable(info));
        if (yaw != 0)  // the nose follows the turn
            QVERIFY2((d.nose_x > 0) == (yaw > 0), qPrintable(info));
    }

    void combinedPose()
    {
        auto measured = trackPose({15, 10, -10, 3, -2, 5});
        QVERIFY(measured);
        const Delta d = delta(*measured, frontal_);
        const QString info = describe(d);
        QVERIFY2(d.yaw > 8 && d.pitch > 4 && d.roll < -5, qPrintable(info));
        QVERIFY2(d.x > 1.5 && d.y < -0.5 && d.z > 3, qPrintable(info));
    }

    // ------------------------------------------------------------ photo

    void photoRotated_data()
    {
        QTest::addColumn<double>("degrees");
        QTest::newRow("clockwise") << 15.0;
        QTest::newRow("anticlockwise") << -15.0;
    }

    // Turning the picture clockwise tips the head toward the image's right,
    // the person's left shoulder: roll goes negative by the same amount.
    void photoRotated()
    {
        QFETCH(double, degrees);
        auto measured = track(*image_tracker_, transformed(portrait_, degrees, {}, 1));
        QVERIFY(measured);
        const Delta d = delta(*measured, portrait_signals_);
        QVERIFY2(std::abs(d.roll + degrees) < 2.5, qPrintable(describe(d)));
        QVERIFY2(std::abs(d.yaw) < 4 && std::abs(d.pitch) < 4, qPrintable(describe(d)));
    }

    // Moving the picture right moves the face to the person's left.
    void photoShifted()
    {
        const double shift = 60;
        auto measured = track(*image_tracker_, transformed(portrait_, 0, {shift, 0}, 1));
        QVERIFY(measured);
        const Delta d = delta(*measured, portrait_signals_);
        QVERIFY2(std::abs(d.nose_x + shift / portrait_.width()) < 0.01, qPrintable(describe(d)));
        QVERIFY2(d.x < -1, qPrintable(describe(d)));

        auto up = track(*image_tracker_, transformed(portrait_, 0, {0, -shift}, 1));
        QVERIFY(up);
        const Delta du = delta(*up, portrait_signals_);
        QVERIFY2(std::abs(du.nose_y - shift / portrait_.height()) < 0.01, qPrintable(describe(du)));
        QVERIFY2(du.y > 1, qPrintable(describe(du)));
    }

    // A bigger face is a closer face.
    void photoScaled()
    {
        auto measured = track(*image_tracker_, transformed(portrait_, 0, {}, 1.2));
        QVERIFY(measured);
        const Delta d = delta(*measured, portrait_signals_);
        // Distance shrinks by 1 / 1.2.
        const double expected = -sig(portrait_signals_, HeadSignal::Z) * (1 - 1 / 1.2);
        QVERIFY2(std::abs(d.z - expected) < expected * 0.25, qPrintable(describe(d) + QString(" expected %1").arg(expected)));
    }

    // ------------------------------------------------------------ robustness

    void cameraFlaws_data()
    {
        QTest::addColumn<double>("yaw");
        QTest::addColumn<double>("noise");
        QTest::addColumn<double>("blur");
        QTest::addColumn<int>("jpeg");
        QTest::addColumn<double>("brightness");
        QTest::newRow("noise") << 0.0 << 8.0 << 0.0 << 0 << 1.0;
        QTest::newRow("blur") << 0.0 << 0.0 << 2.0 << 0 << 1.0;
        QTest::newRow("jpeg") << 0.0 << 0.0 << 0.0 << 30 << 1.0;
        QTest::newRow("dim") << 0.0 << 0.0 << 0.0 << 0 << 0.4;
        QTest::newRow("everything") << 0.0 << 6.0 << 1.0 << 50 << 0.5;
        QTest::newRow("everything turned") << 20.0 << 6.0 << 1.0 << 50 << 0.5;
    }

    void cameraFlaws()
    {
        QFETCH(double, yaw);
        QFETCH(double, noise);
        QFETCH(double, blur);
        QFETCH(int, jpeg);
        QFETCH(double, brightness);
        auto clean = trackPose({yaw, 0, 0, 0, 0, 0});
        QVERIFY(clean);
        face_render::Degrade flaws;
        flaws.noise_sigma = noise;
        flaws.blur_px = blur;
        flaws.jpeg_quality = jpeg;
        flaws.brightness = brightness;
        auto flawed = trackPose({yaw, 0, 0, 0, 0, 0}, flaws);
        QVERIFY2(flawed, "lost the face");
        const Delta d = delta(*flawed, *clean);
        QVERIFY2(std::abs(d.yaw) < 3 && std::abs(d.pitch) < 3 && std::abs(d.roll) < 3, qPrintable(describe(d)));
    }

    // A still head behind a noisy camera, tracked as video: how much do the
    // signals wobble? This is what the head mouse's smoothing and deadzone
    // have to absorb.
    void jitterWhenStill()
    {
        constexpr int kFrames = 40;
        std::vector<double> yaw, pitch, nose_x;
        const QImage still = face_render::render(*head_, face_render::HeadPose{});
        for (int i = 0; i < kFrames; ++i) {
            face_render::Degrade flaws;
            flaws.noise_sigma = 4;
            flaws.seed = static_cast<std::uint32_t>(i + 1);
            auto v = track(*video_tracker_, face_render::degrade(still, flaws), 1000 + i * 33);
            QVERIFY(v);
            yaw.push_back(sig(*v, HeadSignal::Yaw));
            pitch.push_back(sig(*v, HeadSignal::Pitch));
            nose_x.push_back(sig(*v, HeadSignal::NoseX));
        }
        auto stddev = [](const std::vector<double>& v) {
            double mean = 0, sq = 0;
            for (double x : v)
                mean += x;
            mean /= static_cast<double>(v.size());
            for (double x : v)
                sq += (x - mean) * (x - mean);
            return std::sqrt(sq / static_cast<double>(v.size()));
        };
        const double sy = stddev(yaw), sp = stddev(pitch), sn = stddev(nose_x);
        qInfo("still-head jitter (std dev): yaw %.3f deg, pitch %.3f deg, noseX %.5f", sy, sp, sn);
        QVERIFY2(sy < 1.0 && sp < 1.0, "the tracker jitters more than the head mouse expects");
    }

    // A turn played as video: the measured yaw follows it.
    void motionAsVideo()
    {
        auto motion = face_render::motionFromJson(
            R"({"fps": 15, "steps": [{"hold": 0.2}, {"to": {"yaw": 20}, "over": 1}, {"hold": 0.3}]})");
        QVERIFY(motion);
        std::vector<double> yaw;
        for (const auto& frame : motion->frames) {
            auto v = track(*video_tracker_, face_render::render(*head_, frame.pose),
                           static_cast<std::int64_t>(5000 + frame.time_s * 1000));
            QVERIFY(v);
            yaw.push_back(sig(*v, HeadSignal::Yaw));
        }
        QVERIFY2(std::abs(yaw.back() - yaw.front() - 20) < 4.5,
                 qPrintable(QString("from %1 to %2").arg(yaw.front()).arg(yaw.back())));
        // Never goes the wrong way by more than tracking noise.
        for (std::size_t i = 1; i < yaw.size(); ++i)
            QVERIFY2(yaw[i] > yaw[i - 1] - 1.0, qPrintable(QString("frame %1: %2 after %3").arg(i).arg(yaw[i]).arg(yaw[i - 1])));
    }
};

QTEST_GUILESS_MAIN(TestFaceSignals)
#include "tst_face_signals.moc"
