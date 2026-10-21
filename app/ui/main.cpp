#include "app_controller.h"

#include <optional>

#include <QGuiApplication>
#include <QIcon>
#include <QPointerEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

// Makes the window frame follow the app's theme rather than the system's.
// Qt handles the title bar; the title bar's system menu (and other Win32
// menus) only turn dark through uxtheme's unnamed SetPreferredAppMode (#135)
// and FlushMenuThemes (#136), which Windows 10 1903 and later export.
void applyColorScheme(bool dark)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark
                                                       : Qt::ColorScheme::Light);
#endif
#ifdef Q_OS_WIN
    using SetPreferredAppMode = int(WINAPI*)(int);
    using FlushMenuThemes = void(WINAPI*)();
    static const HMODULE uxtheme =
        LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!uxtheme)
        return;
    const auto setMode = reinterpret_cast<SetPreferredAppMode>(
        reinterpret_cast<void*>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135))));
    const auto flush = reinterpret_cast<FlushMenuThemes>(
        reinterpret_cast<void*>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136))));
    if (!setMode)
        return;
    constexpr int kForceDark = 2;
    constexpr int kForceLight = 3;
    setMode(dark ? kForceDark : kForceLight);
    if (flush)
        flush();
#endif
}

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

    // settings->changed() fires for every setting, so only act on a new theme.
    SettingsObject* settings = controller->settings();
    auto applyTheme = [settings, dark = std::optional<bool>()]() mutable {
        const bool nowDark = settings->theme != QLatin1String("light");
        if (dark == nowDark)
            return;
        dark = nowDark;
        applyColorScheme(nowDark);
    };
    applyTheme();
    QObject::connect(settings, &SettingsObject::changed, &app, applyTheme);
    return app.exec();
}
