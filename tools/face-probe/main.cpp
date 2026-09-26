// face-probe: phase-2 check that libmediapipe works from C++ on this OS.
//
//   face-probe --image face.jpg               detect once on a still image
//   face-probe --camera [--device N] [--seconds S]
//                                              live webcam; prints fps and the
//                                              strongest blendshapes each second

#include <game_face/core/face_tracker.h>

#include <QCamera>
#include <QCameraDevice>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <cstdio>
#include <memory>

namespace {

constexpr int kTopBlendshapes = 5;

game_face::RgbImageView view(const QImage& rgb)
{
    return {rgb.constBits(), rgb.width(), rgb.height(), static_cast<int>(rgb.bytesPerLine())};
}

void printTop(const game_face::FaceFrame& frame, int count)
{
    if (!frame.face_found) {
        std::printf("  no face\n");
        return;
    }
    auto shapes = frame.blendshapes;
    std::ranges::sort(shapes, [](const auto& a, const auto& b) { return a.score > b.score; });
    for (int i = 0; i < count && i < static_cast<int>(shapes.size()); ++i)
        std::printf("  %-20s %3d\n", shapes[i].name.c_str(), static_cast<int>(shapes[i].score * 100 + 0.5f));
}

std::unique_ptr<game_face::FaceTracker> makeTracker(const QCommandLineParser& args,
                                                    game_face::FaceTrackerOptions::Mode mode)
{
    const QString dir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
    const QString library_name = QStringLiteral("libmediapipe.dll");
#elif defined(Q_OS_MACOS)
    const QString library_name = QStringLiteral("libmediapipe.dylib");
#else
    const QString library_name = QStringLiteral("libmediapipe.so");
#endif

    game_face::FaceTrackerOptions options;
    options.library_path = (args.isSet("library") ? args.value("library") : dir + "/" + library_name).toStdWString();
    options.model_path = (args.isSet("model") ? args.value("model") : dir + "/face_landmarker.task").toStdWString();
    options.mode = mode;

    QElapsedTimer timer;
    timer.start();
    auto tracker = game_face::FaceTracker::create(options);
    if (!tracker) {
        std::fprintf(stderr, "error: %s\n", tracker.error().message.c_str());
        return nullptr;
    }
    std::printf("face landmarker ready in %lld ms\n", static_cast<long long>(timer.elapsed()));
    return std::move(*tracker);
}

int runImage(const QCommandLineParser& args)
{
    const QString path = args.value("image");
    QImage image(path);
    if (image.isNull()) {
        std::fprintf(stderr, "error: could not read %s\n", qPrintable(path));
        return 1;
    }
    auto tracker = makeTracker(args, game_face::FaceTrackerOptions::Mode::Image);
    if (!tracker)
        return 1;

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    QElapsedTimer timer;
    timer.start();
    auto frame = tracker->detect(view(rgb), 0);
    if (!frame) {
        std::fprintf(stderr, "error: %s\n", frame.error().message.c_str());
        return 1;
    }
    std::printf("%s (%dx%d): %s, %zu landmarks, %zu blendshapes, %lld ms\n",
                qPrintable(path), rgb.width(), rgb.height(),
                frame->face_found ? "face found" : "no face",
                frame->landmarks.size(), frame->blendshapes.size(),
                static_cast<long long>(timer.elapsed()));
    printTop(*frame, 10);
    return frame->face_found ? 0 : 2;
}

int runCamera(QGuiApplication& app, const QCommandLineParser& args)
{
    const auto devices = QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        std::fprintf(stderr, "error: no camera found\n");
        return 1;
    }
    for (int i = 0; i < devices.size(); ++i)
        std::printf("camera %d: %s\n", i, qPrintable(devices[i].description()));

    const int index = args.value("device").toInt();
    if (index < 0 || index >= devices.size()) {
        std::fprintf(stderr, "error: --device must be 0..%lld\n", static_cast<long long>(devices.size() - 1));
        return 1;
    }

    auto tracker = makeTracker(args, game_face::FaceTrackerOptions::Mode::Video);
    if (!tracker)
        return 1;

    QCamera camera(devices[index]);
    QMediaCaptureSession session;
    QVideoSink sink;
    session.setCamera(&camera);
    session.setVideoSink(&sink);

    QElapsedTimer clock;
    clock.start();
    int processed = 0;
    qint64 detect_ms = 0;
    game_face::FaceFrame latest;
    int exit_code = 0;

    QObject::connect(&camera, &QCamera::errorOccurred, &app, [&](QCamera::Error, const QString& message) {
        std::fprintf(stderr, "camera error: %s\n", qPrintable(message));
        exit_code = 1;
        app.quit();
    });

    // Frames arrive on the GUI thread; detection runs inline, so frames that
    // arrive while it runs are simply skipped.
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame& video_frame) {
        const QImage rgb = video_frame.toImage().convertToFormat(QImage::Format_RGB888);
        if (rgb.isNull())
            return;
        QElapsedTimer timer;
        timer.start();
        auto frame = tracker->detect(view(rgb), clock.elapsed());
        detect_ms += timer.elapsed();
        if (!frame) {
            std::fprintf(stderr, "error: %s\n", frame.error().message.c_str());
            return;
        }
        latest = std::move(*frame);
        ++processed;
    });

    QTimer report;
    QObject::connect(&report, &QTimer::timeout, &app, [&] {
        std::printf("%d fps, %.1f ms/detect\n", processed,
                    processed ? static_cast<double>(detect_ms) / processed : 0.0);
        printTop(latest, kTopBlendshapes);
        std::fflush(stdout);
        processed = 0;
        detect_ms = 0;
    });
    report.start(1000);

    QTimer::singleShot(args.value("seconds").toInt() * 1000, &app, &QCoreApplication::quit);

    camera.start();
    app.exec();
    camera.stop();
    return exit_code;
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("face-probe");

    QCommandLineParser parser;
    parser.setApplicationDescription("Checks MediaPipe face tracking through libmediapipe.");
    parser.addHelpOption();
    parser.addOptions({
        {"image", "Detect on a still image.", "file"},
        {"camera", "Track the webcam live."},
        {"device", "Camera index (default 0).", "index", "0"},
        {"seconds", "How long to run the camera (default 10).", "seconds", "10"},
        {"library", "Path to libmediapipe (default: next to this program).", "path"},
        {"model", "Path to face_landmarker.task (default: next to this program).", "path"},
    });
    parser.process(app);

    if (parser.isSet("image"))
        return runImage(parser);
    if (parser.isSet("camera"))
        return runCamera(app, parser);
    parser.showHelp(1);
}
