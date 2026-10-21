#include "frame_player.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoSink>

#include <cmath>

FramePlayer::FramePlayer(QObject* parent)
    : QObject(parent)
{
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &FramePlayer::nextImage);
}

FramePlayer::~FramePlayer() = default;

QString FramePlayer::open(const QString& path, double fps)
{
    stop();
    files_.clear();
    player_.reset();
    sink_.reset();

    const QFileInfo info(path);
    if (info.isDir()) {
        const QDir dir(path);
        for (const QString& name : dir.entryList({"*.png", "*.jpg", "*.jpeg", "*.bmp"}, QDir::Files, QDir::Name))
            files_ << dir.filePath(name);
        if (files_.isEmpty())
            return tr("No images in %1").arg(path);
        if (fps <= 0) {
            QFile rate(dir.filePath("fps.txt"));
            if (rate.open(QIODevice::ReadOnly))
                fps = rate.readAll().trimmed().toDouble();
        }
        if (fps <= 0)
            fps = 30;
        timer_.setInterval(static_cast<int>(std::lround(1000.0 / fps)));
        return {};
    }
    if (!info.isFile())
        return tr("%1 doesn't exist").arg(path);

    player_ = std::make_unique<QMediaPlayer>();
    sink_ = std::make_unique<QVideoSink>();
    player_->setVideoSink(sink_.get());
    player_->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
    connect(sink_.get(), &QVideoSink::videoFrameChanged, this, &FramePlayer::frameReady);
    connect(player_.get(), &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia)
            emit finished();
    });
    connect(player_.get(), &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString&) { emit finished(); });
    return {};
}

void FramePlayer::start()
{
    if (player_) {
        player_->play();
    } else if (!files_.isEmpty()) {
        next_ = 0;
        timer_.start();
    }
}

void FramePlayer::stop()
{
    timer_.stop();
    if (player_)
        player_->stop();
}

void FramePlayer::nextImage()
{
    if (next_ >= files_.size()) {
        timer_.stop();
        emit finished();
        return;
    }
    const QImage image(files_[next_++]);
    if (!image.isNull())
        emit frameReady(QVideoFrame(image));
}
