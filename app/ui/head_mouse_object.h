#pragma once

#include <game_face/core/head_mouse_settings.h>

#include <QObject>
#include <QQmlEngine>
#include <QString>

// game_face::HeadMouseAxis as QML properties. `error` is the source's compile
// error, if any. Every write emits changed().
class HeadMouseAxisObject : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool hasNeutral READ hasNeutral NOTIFY changed)
    Q_PROPERTY(double neutral READ neutral WRITE setNeutral NOTIFY changed)
    Q_PROPERTY(double rangeNegative MEMBER rangeNegative NOTIFY changed)
    Q_PROPERTY(double rangePositive MEMBER rangePositive NOTIFY changed)
    Q_PROPERTY(double gain MEMBER gain NOTIFY changed)
    Q_PROPERTY(bool invert MEMBER invert NOTIFY changed)

public:
    using QObject::QObject;

    void load(const game_face::HeadMouseAxis& axis);
    game_face::HeadMouseAxis toAxis() const;

    QString source() const { return source_; }
    void setSource(const QString& source);
    QString error() const { return error_; }
    bool hasNeutral() const { return hasNeutral_; }
    double neutral() const { return neutral_; }
    void setNeutral(double neutral);
    // Forgets the neutral; the next frame with a face captures it.
    Q_INVOKABLE void clearNeutral();

    double rangeNegative = 20;
    double rangePositive = 20;
    double gain = 1;
    bool invert = false;

signals:
    void changed();

private:
    void compile();

    QString source_;
    QString error_;
    bool hasNeutral_ = false;
    double neutral_ = 0;
};

// game_face::HeadMouseGroup (pointer or scroll). `preset` is the id of the
// preset whose sources the axes use, or "" for custom sources.
class HeadMouseGroupObject : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(double deadzone MEMBER deadzone NOTIFY changed)
    Q_PROPERTY(double smoothing MEMBER smoothing NOTIFY changed)
    Q_PROPERTY(HeadMouseAxisObject* x READ x CONSTANT)
    Q_PROPERTY(HeadMouseAxisObject* y READ y CONSTANT)
    Q_PROPERTY(QString preset READ preset NOTIFY changed)

public:
    explicit HeadMouseGroupObject(QObject* parent = nullptr);

    void load(const game_face::HeadMouseGroup& group);
    game_face::HeadMouseGroup toGroup() const;

    QString mode() const { return mode_; }
    void setMode(const QString& mode);
    HeadMouseAxisObject* x() const { return x_; }
    HeadMouseAxisObject* y() const { return y_; }
    QString preset() const;
    Q_INVOKABLE void applyPreset(const QString& id);

    double deadzone = 0.05;
    double smoothing = 0.5;

signals:
    void changed();

private:
    QString mode_ = QStringLiteral("relative");
    HeadMouseAxisObject* x_;
    HeadMouseAxisObject* y_;
};

// game_face::HeadMouseSettings (App.settings.headMouse).
class HeadMouseObject : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(bool enabled MEMBER enabled NOTIFY changed)
    Q_PROPERTY(double clickFreezeMs MEMBER clickFreezeMs NOTIFY changed)
    Q_PROPERTY(HeadMouseGroupObject* pointer READ pointer CONSTANT)
    Q_PROPERTY(HeadMouseGroupObject* scroll READ scroll CONSTANT)

public:
    explicit HeadMouseObject(QObject* parent = nullptr);

    void load(const game_face::HeadMouseSettings& settings);
    game_face::HeadMouseSettings toSettings() const;

    HeadMouseGroupObject* pointer() const { return pointer_; }
    HeadMouseGroupObject* scroll() const { return scroll_; }

    bool enabled = false;
    double clickFreezeMs = 150;

signals:
    void changed();

private:
    HeadMouseGroupObject* pointer_;
    HeadMouseGroupObject* scroll_;
};
