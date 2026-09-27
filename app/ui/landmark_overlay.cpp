#include "landmark_overlay.h"

#include "app_controller.h"
#include "face_connections.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <span>

namespace fc = game_face::face_connections;

namespace {

struct Layer {
    std::span<const fc::Edge> edges;
    QColor color;
};

// Same colours as the original app. Its thin white tessellation (0.25 px)
// becomes a faint 1 px line here.
const Layer kLayers[] = {
    {fc::kTesselation, QColor(255, 255, 255, 70)},
    {fc::kRightEyebrow, QColor(0xba, 0xff, 0x98)},
    {fc::kRightEye, QColor(0xff, 0x06, 0xbd)},
    {fc::kRightIris, QColor(0xff, 0x30, 0x30)},
    {fc::kLeftEyebrow, QColor(0xff, 0xf7, 0x00)},
    {fc::kLeftEye, QColor(0x00, 0x42, 0x42)},
    {fc::kLeftIris, QColor(0x00, 0xff, 0xff)},
    {fc::kFaceOval, QColor(0x12, 0xff, 0x32)},
    {fc::kLips, QColor(0x0b, 0x00, 0x6f)},
};

QSGGeometryNode* makeLayerNode(const QColor& color)
{
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(1);

    auto* material = new QSGFlatColorMaterial;
    material->setColor(color);

    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

} // namespace

LandmarkOverlay::LandmarkOverlay(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

QObject* LandmarkOverlay::source() const
{
    return source_;
}

void LandmarkOverlay::setSource(QObject* source)
{
    auto* app = qobject_cast<AppController*>(source);
    if (app == source_)
        return;
    if (source_)
        disconnect(source_, nullptr, this, nullptr);
    source_ = app;
    if (app)
        connect(app, &AppController::landmarksChanged, this, &QQuickItem::update);
    emit sourceChanged();
    update();
}

void LandmarkOverlay::setContentRect(const QRectF& rect)
{
    if (rect == contentRect_)
        return;
    contentRect_ = rect;
    emit contentRectChanged();
    update();
}

QSGNode* LandmarkOverlay::updatePaintNode(QSGNode* old, UpdatePaintNodeData*)
{
    QSGNode* root = old;
    if (!root) {
        root = new QSGNode;
        for (const Layer& layer : kLayers)
            root->appendChildNode(makeLayerNode(layer.color));
    }

    static const std::vector<game_face::Landmark> kNone;
    const auto& landmarks = source_ ? source_->landmarks() : kNone;
    const QRectF rect = contentRect_.isEmpty() ? boundingRect() : contentRect_;

    QSGNode* child = root->firstChild();
    for (const Layer& layer : kLayers) {
        auto* node = static_cast<QSGGeometryNode*>(child);
        QSGGeometry* geometry = node->geometry();
        const int vertices = landmarks.empty() ? 0 : static_cast<int>(layer.edges.size() * 2);
        geometry->allocate(vertices);

        if (vertices > 0) {
            QSGGeometry::Point2D* points = geometry->vertexDataAsPoint2D();
            for (const fc::Edge& edge : layer.edges) {
                for (auto index : {edge.start, edge.end}) {
                    const auto& lm = index < landmarks.size() ? landmarks[index] : landmarks.front();
                    points->set(static_cast<float>(rect.x() + lm.x * rect.width()),
                                static_cast<float>(rect.y() + lm.y * rect.height()));
                    ++points;
                }
            }
        }
        node->markDirty(QSGNode::DirtyGeometry);
        child = child->nextSibling();
    }
    return root;
}
