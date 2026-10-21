// face-render: renders the test head at known poses (docs/head-mouse-plan.md, 8.2).
//
//   face-render --pose yaw,pitch,roll[,x,y,z] --out head.png
//   face-render --motion script.json --out-dir frames/
//
// The head scan comes from the build's test assets (cmake/FetchTestAssets.cmake)
// unless --assets points elsewhere. The frames from --motion can be played
// into game-face with --frames.

#include "face_render.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QGuiApplication>

#include <cstdio>

namespace {

std::filesystem::path toPath(const QString& s)
{
    return std::filesystem::path(s.toStdU16String());
}

bool parsePose(const QString& text, face_render::HeadPose& pose)
{
    const QStringList parts = text.split(',');
    if (parts.size() != 3 && parts.size() != 6)
        return false;
    double* fields[] = {&pose.yaw, &pose.pitch, &pose.roll, &pose.x, &pose.y, &pose.z};
    for (int i = 0; i < parts.size(); ++i) {
        bool ok = false;
        *fields[i] = parts[i].trimmed().toDouble(&ok);
        if (!ok)
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("face-render");

    QCommandLineParser parser;
    parser.setApplicationDescription("Renders a 3D head at known poses, for testing face tracking.");
    parser.addHelpOption();
    parser.addOptions({
        {"pose", "yaw,pitch,roll[,x,y,z] in degrees and cm (user's point of view).", "pose"},
        {"out", "Image file for --pose.", "file"},
        {"motion", "A motion script (JSON; see face_render.h).", "file"},
        {"out-dir", "Folder for the --motion frames (frame-00000.png, ...).", "dir"},
        {"size", "Image size, WxH (default 640x480).", "size", "640x480"},
        {"noise", "Gaussian noise sigma, 0..255 levels.", "sigma", "0"},
        {"blur", "Box blur radius in pixels.", "px", "0"},
        {"jpeg", "Recompress as JPEG at this quality (1..100).", "quality", "0"},
        {"brightness", "Lighting brightness (1 is normal).", "factor", "1"},
        {"assets", "Folder with LeePerrySmith.glb and Map-COL.jpg.", "dir", GAME_FACE_TEST_ASSETS_DIR},
    });
    parser.process(app);

    const QStringList size = parser.value("size").split('x');
    face_render::Camera camera;
    if (size.size() == 2)
        camera.size = QSize(size[0].toInt(), size[1].toInt());
    if (camera.size.isEmpty()) {
        std::fprintf(stderr, "error: --size must be WxH\n");
        return 1;
    }
    face_render::Degrade flaws;
    flaws.noise_sigma = parser.value("noise").toDouble();
    flaws.blur_px = parser.value("blur").toDouble();
    flaws.jpeg_quality = parser.value("jpeg").toInt();
    flaws.brightness = parser.value("brightness").toDouble();

    const QString assets = parser.value("assets");
    auto model = face_render::HeadModel::load(toPath(assets + "/LeePerrySmith.glb"), toPath(assets + "/Map-COL.jpg"));
    if (!model) {
        std::fprintf(stderr, "error: %s\n", model.error().message.c_str());
        return 1;
    }

    if (parser.isSet("pose")) {
        face_render::HeadPose pose;
        if (!parsePose(parser.value("pose"), pose) || !parser.isSet("out")) {
            std::fprintf(stderr, "error: --pose needs yaw,pitch,roll[,x,y,z] and --out\n");
            return 1;
        }
        const QImage image = face_render::degrade(face_render::render(*model, pose, camera), flaws);
        if (!image.save(parser.value("out"))) {
            std::fprintf(stderr, "error: could not write %s\n", qPrintable(parser.value("out")));
            return 1;
        }
        return 0;
    }

    if (parser.isSet("motion")) {
        QFile file(parser.value("motion"));
        if (!file.open(QIODevice::ReadOnly) || !parser.isSet("out-dir")) {
            std::fprintf(stderr, "error: --motion needs a readable script and --out-dir\n");
            return 1;
        }
        auto motion = face_render::motionFromJson(file.readAll().toStdString());
        if (!motion) {
            std::fprintf(stderr, "error: %s\n", motion.error().message.c_str());
            return 1;
        }
        const QDir dir(parser.value("out-dir"));
        QDir().mkpath(dir.path());
        int index = 0;
        for (const auto& frame : motion->frames) {
            face_render::Degrade per_frame = flaws;
            per_frame.seed = static_cast<std::uint32_t>(index + 1);  // noise changes frame to frame
            const QImage image = face_render::degrade(face_render::render(*model, frame.pose, camera), per_frame);
            const QString name = dir.filePath(QString("frame-%1.png").arg(index++, 5, 10, QChar('0')));
            if (!image.save(name)) {
                std::fprintf(stderr, "error: could not write %s\n", qPrintable(name));
                return 1;
            }
        }
        // The frame rate, for game-face --frames.
        QFile fps(dir.filePath("fps.txt"));
        if (fps.open(QIODevice::WriteOnly))
            fps.write(QByteArray::number(motion->fps));
        std::printf("%d frames at %g fps in %s\n", index, motion->fps, qPrintable(dir.path()));
        return 0;
    }

    parser.showHelp(1);
}
