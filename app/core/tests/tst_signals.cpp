#include <game_face/core/face_tracker.h>
#include <game_face/core/signals.h>

#include <QTest>

#include <cmath>
#include <numbers>

using namespace game_face;

namespace {

constexpr double kRad = std::numbers::pi / 180.0;

// A column-major 4x4 matrix with the given 3x3 rotation (rows) and translation.
std::array<float, 16> matrix(const std::array<std::array<double, 3>, 3>& r, double tx = 0, double ty = 0,
                             double tz = 0)
{
    std::array<float, 16> m{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            m[static_cast<std::size_t>(col * 4 + row)] = static_cast<float>(r[row][col]);
    m[12] = static_cast<float>(tx);
    m[13] = static_cast<float>(ty);
    m[14] = static_cast<float>(tz);
    m[15] = 1;
    return m;
}

bool near(double a, double b, double tolerance = 1e-3)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

class TestSignals : public QObject {
    Q_OBJECT

private slots:
    void names()
    {
        QCOMPARE(kSignalNames.size(), std::size_t(60));
        QCOMPARE(kSignalNames[0], std::string_view("_neutral"));
        QCOMPARE(signalIndex("headYaw"), std::optional<std::size_t>(52));
        QCOMPARE(signalIndex("noseY"), std::optional<std::size_t>(59));
        QCOMPARE(signalIndex("jawOpen"), blendshapeIndex("jawOpen"));
        QVERIFY(!signalIndex("headyaw"));
        QCOMPARE(signalIndex(HeadSignal::Roll), std::size_t(54));
    }

    void identityIsFacingTheCamera()
    {
        const HeadPose p = headPoseFromMatrix(matrix({{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}}));
        QVERIFY(near(p.yaw, 0) && near(p.pitch, 0) && near(p.roll, 0));
    }

    // Right-handed rotations in camera space (x right, y up, z toward the
    // camera), and what they mean for the user facing the camera.
    void knownRotations()
    {
        const double c = std::cos(30 * kRad), s = std::sin(30 * kRad);
        // About y: the nose turns toward the image's right, the user's left.
        HeadPose p = headPoseFromMatrix(matrix({{{c, 0, s}, {0, 1, 0}, {-s, 0, c}}}));
        QVERIFY2(near(p.yaw, -30), qPrintable(QString::number(p.yaw)));
        QVERIFY(near(p.pitch, 0) && near(p.roll, 0));

        // About x: the nose turns down.
        p = headPoseFromMatrix(matrix({{{1, 0, 0}, {0, c, -s}, {0, s, c}}}));
        QVERIFY2(near(p.pitch, -30), qPrintable(QString::number(p.pitch)));
        QVERIFY(near(p.yaw, 0) && near(p.roll, 0));

        // About z: the top of the head tips toward the image's left, the
        // user's right shoulder.
        p = headPoseFromMatrix(matrix({{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}}));
        QVERIFY2(near(p.roll, 30), qPrintable(QString::number(p.roll)));
        QVERIFY(near(p.yaw, 0) && near(p.pitch, 0));
    }

    void translation()
    {
        const HeadPose p = headPoseFromMatrix(matrix({{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}}, 5, 2, -50));
        QVERIFY(near(p.x, -5));  // camera +x is the user's left
        QVERIFY(near(p.y, 2));
        QVERIFY(near(p.z, -50));
    }

    void scaleCancelsOut()
    {
        const double c = std::cos(20 * kRad) * 1.3, s = std::sin(20 * kRad) * 1.3;
        const HeadPose p = headPoseFromMatrix(matrix({{{c, 0, s}, {0, 1.3, 0}, {-s, 0, c}}}));
        QVERIFY(near(p.yaw, -20));
        QVERIFY(near(p.pitch, 0));
    }

    void roundTrip_data()
    {
        QTest::addColumn<double>("yaw");
        QTest::addColumn<double>("pitch");
        QTest::addColumn<double>("roll");
        QTest::newRow("yaw") << 25.0 << 0.0 << 0.0;
        QTest::newRow("pitch") << 0.0 << -18.0 << 0.0;
        QTest::newRow("roll") << 0.0 << 0.0 << 22.0;
        QTest::newRow("all") << -31.0 << 14.0 << -9.0;
        QTest::newRow("large") << 60.0 << -45.0 << 40.0;
    }

    void roundTrip()
    {
        QFETCH(double, yaw);
        QFETCH(double, pitch);
        QFETCH(double, roll);
        const HeadPose in{yaw, pitch, roll, 3.5, -2.0, -45.0};
        const HeadPose out = headPoseFromMatrix(matrixFromHeadPose(in));
        QVERIFY2(near(out.yaw, yaw) && near(out.pitch, pitch) && near(out.roll, roll),
                 qPrintable(QString("%1 %2 %3").arg(out.yaw).arg(out.pitch).arg(out.roll)));
        QVERIFY(near(out.x, 3.5) && near(out.y, -2.0) && near(out.z, -45.0));
    }

    void fromFrame()
    {
        FaceFrame frame;
        frame.face_found = true;
        frame.blendshapes = {{"_neutral", 0.0f}, {"browDownLeft", 0.25f}};
        frame.landmarks = {{0.5f, 0.5f, 0}, {0.3f, 0.4f, 0}};
        frame.pose = HeadPose{10, -5, 2, 1, 2, -40};
        const SignalValues v = toSignals(frame);
        QCOMPARE(v[*signalIndex("browDownLeft")], 25.0);
        QCOMPARE(v[signalIndex(HeadSignal::Yaw)], 10.0);
        QCOMPARE(v[signalIndex(HeadSignal::Pitch)], -5.0);
        QCOMPARE(v[signalIndex(HeadSignal::Roll)], 2.0);
        QCOMPARE(v[signalIndex(HeadSignal::X)], 1.0);
        QCOMPARE(v[signalIndex(HeadSignal::Y)], 2.0);
        QCOMPARE(v[signalIndex(HeadSignal::Z)], -40.0);
        QVERIFY(near(v[signalIndex(HeadSignal::NoseX)], 0.7));  // mirrored
        QVERIFY(near(v[signalIndex(HeadSignal::NoseY)], 0.6));  // up is positive
    }

    void noPoseMeansZero()
    {
        FaceFrame frame;
        const SignalValues v = toSignals(frame);
        QCOMPARE(v[signalIndex(HeadSignal::Yaw)], 0.0);
        QCOMPARE(v[signalIndex(HeadSignal::NoseX)], 0.0);
    }
};

QTEST_APPLESS_MAIN(TestSignals)
#include "tst_signals.moc"
