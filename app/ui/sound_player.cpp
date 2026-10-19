#include "sound_player.h"

#include <QAudio>
#include <QUrl>
#include <QVariantMap>

#include <array>

namespace {

struct Sound {
    const char* id;    // file name in assets/sounds, without .wav
    const char* name;  // shown in Settings
};

// Kenney's UI Audio pack, curated; see assets/sounds/README.md.
constexpr std::array kSounds{
    Sound{"click1", QT_TRANSLATE_NOOP("SoundPlayer", "Click 1")},
    Sound{"click3", QT_TRANSLATE_NOOP("SoundPlayer", "Click 2")},
    Sound{"click5", QT_TRANSLATE_NOOP("SoundPlayer", "Click 3")},
    Sound{"mouseclick1", QT_TRANSLATE_NOOP("SoundPlayer", "Mouse click")},
    Sound{"mouserelease1", QT_TRANSLATE_NOOP("SoundPlayer", "Mouse release")},
    Sound{"rollover2", QT_TRANSLATE_NOOP("SoundPlayer", "Soft 1")},
    Sound{"rollover4", QT_TRANSLATE_NOOP("SoundPlayer", "Soft 2")},
    Sound{"switch12", QT_TRANSLATE_NOOP("SoundPlayer", "Switch 1")},
    Sound{"switch13", QT_TRANSLATE_NOOP("SoundPlayer", "Switch 2")},
    Sound{"switch7", QT_TRANSLATE_NOOP("SoundPlayer", "Switch 3")},
};

bool isKnown(const QString& name)
{
    for (const Sound& sound : kSounds) {
        if (name == QLatin1String(sound.id))
            return true;
    }
    return false;
}

} // namespace

SoundPlayer::SoundPlayer(QObject* parent)
    : QObject(parent)
{
}

QVariantList SoundPlayer::sounds()
{
    QVariantList list;
    for (const Sound& sound : kSounds)
        list.append(QVariantMap{{"id", QString::fromLatin1(sound.id)},
                                {"name", tr(sound.name)}});
    return list;
}

void SoundPlayer::setSound(QSoundEffect& effect, const QString& name)
{
    const QUrl source = isKnown(name) ? QUrl("qrc:/game-face/sounds/" + name + ".wav") : QUrl();
    if (effect.source() != source)
        effect.setSource(source);
}

void SoundPlayer::configure(const QString& start, const QString& stop, double volume)
{
    setSound(start_, start);
    setSound(stop_, stop);
    const float linear = QAudio::convertVolume(static_cast<float>(volume), QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);
    start_.setVolume(linear);
    stop_.setVolume(linear);
    preview_.setVolume(linear);
}

void SoundPlayer::play(game_face::BindingEngine::Edge edge)
{
    QSoundEffect& effect = edge == game_face::BindingEngine::Edge::Start ? start_ : stop_;
    if (!effect.source().isEmpty())
        effect.play();
}

void SoundPlayer::preview(const QString& name)
{
    setSound(preview_, name);
    if (!preview_.source().isEmpty())
        preview_.play();
}
