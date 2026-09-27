#include "app_controller.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("game-face");
    QGuiApplication::setApplicationDisplayName("game-face");
    QGuiApplication::setWindowIcon(QIcon(":/game-face/icon.png"));
    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("GameFace", "Main");

    // The engine owns the App singleton; it is created while Main.qml loads.
    auto* controller = engine.singletonInstance<AppController*>("GameFace", "App");
    if (!controller)
        return 1;
    controller->initialize();
    return app.exec();
}
