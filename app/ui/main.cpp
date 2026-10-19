#include "app_controller.h"

#include <QGuiApplication>
#include <QIcon>
#include <QPointerEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>

namespace {

// Clears a text field's focus when the user presses anywhere outside it, which
// also commits the edit (editingFinished). Filters the window rather than using
// a QML TapHandler, because Flickables accept the press before a handler
// behind them would see it. The press is still delivered, so pressing another
// control focuses that control as usual.
class TextFocusClearer : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::TouchBegin)
            return false;
        auto* window = qobject_cast<QQuickWindow*>(watched);
        QQuickItem* focused = window ? window->activeFocusItem() : nullptr;
        if (!focused || !(focused->inherits("QQuickTextInput") || focused->inherits("QQuickTextEdit")))
            return false;
        const auto* pointer = static_cast<QPointerEvent*>(event);
        if (pointer->points().isEmpty())
            return false;
        const QPointF position = focused->mapFromScene(pointer->points().first().scenePosition());
        // Active focus moves to the enclosing focus scope, so a dialog keeps it.
        if (!focused->contains(position))
            focused->setFocus(false);
        return false;
    }
};

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("game-face");
    QGuiApplication::setApplicationDisplayName("game-face");
    QGuiApplication::setApplicationVersion(GAME_FACE_VERSION);
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
    TextFocusClearer focusClearer;
    for (QObject* root : engine.rootObjects()) {
        if (auto* window = qobject_cast<QQuickWindow*>(root))
            window->installEventFilter(&focusClearer);
    }
    controller->initialize();
    return app.exec();
}
