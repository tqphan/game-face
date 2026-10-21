// Runs the QML tests in tests/qml (Qt Quick Test) with the app's style.

#include <QQuickStyle>
#include <QtQuickTest>

class Setup : public QObject {
    Q_OBJECT

public slots:
    void applicationAvailable() { QQuickStyle::setStyle("Material"); }
};

QUICK_TEST_MAIN_WITH_SETUP(head_mouse_qml, Setup)

#include "tst_head_mouse_qml.moc"
