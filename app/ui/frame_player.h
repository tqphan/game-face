#pragma once

#include <QImage>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVideoFrame>

#include <memory>

class QMediaPlayer;
class QVideoSink;

// Plays recorded frames in place of the camera (game-face --frames), so the
// whole app can run on a known head motion without a camera or a person.
// A folder of images plays in name order at the rate in its fps.txt (as
// face-render --motion writes it) or the given one; anything else is opened
// as a video file.
class FramePlayer : public QObject {
    Q_OBJECT

public:
    explicit FramePlayer(QObject* parent = nullptr);
    ~FramePlayer() override;

    // Returns an error message, or an empty string. fps <= 0 uses the
    // folder's fps.txt, or 30.
    QString open(const QString& path, double fps = 0);
    void start();
    void stop();
    bool isOpen() const { return !files_.isEmpty() || player_; }

signals:
    void frameReady(const QVideoFrame& frame);
    // The last frame has been sent.
    void finished();

private:
    void nextImage();

    QStringList files_;
    int next_ = 0;
    QTimer timer_;
    std::unique_ptr<QMediaPlayer> player_;
    std::unique_ptr<QVideoSink> sink_;
};
