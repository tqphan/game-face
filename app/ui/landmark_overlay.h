#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QRectF>

class AppController;

// Draws the face mesh from App's latest landmarks over the camera preview.
// `contentRect` is where the video is drawn inside this item (bind it to
// VideoOutput.contentRect), since landmarks are normalised to the frame.
class LandmarkOverlay : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect WRITE setContentRect NOTIFY contentRectChanged)

public:
    explicit LandmarkOverlay(QQuickItem* parent = nullptr);

    QObject* source() const;
    void setSource(QObject* source);
    QRectF contentRect() const { return contentRect_; }
    void setContentRect(const QRectF& rect);

signals:
    void sourceChanged();
    void contentRectChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override;

private:
    QPointer<AppController> source_;
    QRectF contentRect_;
};
