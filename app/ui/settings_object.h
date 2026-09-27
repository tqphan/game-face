#pragma once

#include <game_face/core/settings.h>

#include <QObject>
#include <QQmlEngine>
#include <QString>

// game_face::Settings as QML properties (App.settings.*). Every write emits
// changed(); AppController reacts to that.
class SettingsObject : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString theme MEMBER theme NOTIFY changed)
    Q_PROPERTY(QString language MEMBER language NOTIFY changed)
    Q_PROPERTY(QString cameraId MEMBER cameraId NOTIFY changed)
    Q_PROPERTY(double webcamOpacity MEMBER webcamOpacity NOTIFY changed)
    Q_PROPERTY(double landmarksOpacity MEMBER landmarksOpacity NOTIFY changed)
    Q_PROPERTY(double detectionConfidence MEMBER detectionConfidence NOTIFY changed)
    Q_PROPERTY(double presenceConfidence MEMBER presenceConfidence NOTIFY changed)
    Q_PROPERTY(double trackingConfidence MEMBER trackingConfidence NOTIFY changed)
    Q_PROPERTY(bool autoStartTracking MEMBER autoStartTracking NOTIFY changed)
    Q_PROPERTY(bool allowInputSimulation MEMBER allowInputSimulation NOTIFY changed)
    Q_PROPERTY(bool autoSaveSettings MEMBER autoSaveSettings NOTIFY changed)
    Q_PROPERTY(bool autoSaveProfiles MEMBER autoSaveProfiles NOTIFY changed)
    Q_PROPERTY(bool lockUi MEMBER lockUi NOTIFY changed)

public:
    using QObject::QObject;

    void load(const game_face::Settings& s);
    game_face::Settings toSettings() const;

    QString theme;
    QString language;
    QString cameraId;
    double webcamOpacity = 1;
    double landmarksOpacity = 1;
    double detectionConfidence = 0.5;
    double presenceConfidence = 0.5;
    double trackingConfidence = 0.5;
    bool autoStartTracking = false;
    bool allowInputSimulation = false;
    bool autoSaveSettings = false;
    bool autoSaveProfiles = false;
    bool lockUi = false;

    // Not shown in the UI yet, but kept so saving doesn't drop them.
    bool autoStartWithOs = false;
    bool runOnSecureDesktop = false;

signals:
    void changed();
};
