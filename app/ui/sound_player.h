#pragma once

#include <game_face/core/binding_engine.h>

#include <QObject>
#include <QSoundEffect>
#include <QString>
#include <QVariantList>

// Plays the start and stop sounds chosen in Settings (assets/sounds). Lives
// on the GUI thread; QSoundEffect preloads each sound, so playing it is quick.
class SoundPlayer : public QObject {
    Q_OBJECT

public:
    explicit SoundPlayer(QObject* parent = nullptr);

    // [{id, name}] for the Settings dropdowns, without "None".
    static QVariantList sounds();

    // Empty or unknown names mean no sound. `volume` is 0..1 as the slider
    // shows it; it is mapped to a perceptually even scale.
    void configure(const QString& start, const QString& stop, double volume);
    void play(game_face::BindingEngine::Edge edge);
    void preview(const QString& name);

private:
    static void setSound(QSoundEffect& effect, const QString& name);

    QSoundEffect start_;
    QSoundEffect stop_;
    QSoundEffect preview_;
};
