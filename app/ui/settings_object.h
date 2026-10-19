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
    Q_PROPERTY(bool confirmOnClose MEMBER confirmOnClose NOTIFY changed)
    Q_PROPERTY(bool settleOnStop MEMBER settleOnStop NOTIFY changed)
    Q_PROPERTY(QString startSound MEMBER startSound NOTIFY changed)
    Q_PROPERTY(QString stopSound MEMBER stopSound NOTIFY changed)
    Q_PROPERTY(double soundVolume MEMBER soundVolume NOTIFY changed)
    // Face mesh: a "#rrggbb"/"#aarrggbb" colour and a visible flag per group.
    Q_PROPERTY(QString meshTessellationColor MEMBER meshTessellationColor NOTIFY changed)
    Q_PROPERTY(bool meshTessellationVisible MEMBER meshTessellationVisible NOTIFY changed)
    Q_PROPERTY(QString meshFaceOvalColor MEMBER meshFaceOvalColor NOTIFY changed)
    Q_PROPERTY(bool meshFaceOvalVisible MEMBER meshFaceOvalVisible NOTIFY changed)
    Q_PROPERTY(QString meshLipsColor MEMBER meshLipsColor NOTIFY changed)
    Q_PROPERTY(bool meshLipsVisible MEMBER meshLipsVisible NOTIFY changed)
    Q_PROPERTY(QString meshEyesColor MEMBER meshEyesColor NOTIFY changed)
    Q_PROPERTY(bool meshEyesVisible MEMBER meshEyesVisible NOTIFY changed)
    Q_PROPERTY(QString meshEyebrowsColor MEMBER meshEyebrowsColor NOTIFY changed)
    Q_PROPERTY(bool meshEyebrowsVisible MEMBER meshEyebrowsVisible NOTIFY changed)
    Q_PROPERTY(QString meshIrisesColor MEMBER meshIrisesColor NOTIFY changed)
    Q_PROPERTY(bool meshIrisesVisible MEMBER meshIrisesVisible NOTIFY changed)
    Q_PROPERTY(double meshContourWidth MEMBER meshContourWidth NOTIFY changed)
    Q_PROPERTY(double meshTessellationWidth MEMBER meshTessellationWidth NOTIFY changed)

public:
    using QObject::QObject;

    void load(const game_face::Settings& s);
    game_face::Settings toSettings() const;
    // Restores the face mesh colours, visibility and widths to their defaults.
    Q_INVOKABLE void resetMeshStyle();

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
    bool confirmOnClose = true;
    bool settleOnStop = false;
    QString startSound;
    QString stopSound;
    double soundVolume = 0.5;
    QString meshTessellationColor;
    bool meshTessellationVisible = true;
    QString meshFaceOvalColor;
    bool meshFaceOvalVisible = true;
    QString meshLipsColor;
    bool meshLipsVisible = true;
    QString meshEyesColor;
    bool meshEyesVisible = true;
    QString meshEyebrowsColor;
    bool meshEyebrowsVisible = true;
    QString meshIrisesColor;
    bool meshIrisesVisible = true;
    double meshContourWidth = 1;
    double meshTessellationWidth = 1;

    // Not shown in the UI yet, but kept so saving doesn't drop them.
    bool autoStartWithOs = false;
    bool runOnSecureDesktop = false;

signals:
    void changed();
};
