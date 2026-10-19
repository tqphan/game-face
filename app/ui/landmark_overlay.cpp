#include "landmark_overlay.h"

#include "app_controller.h"
#include "face_connections.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <cmath>
#include <iterator>
#include <span>

namespace fc = game_face::face_connections;

namespace {

enum class Group { Tessellation, FaceOval, Lips, Eyes, Eyebrows, Irises };

struct Layer {
    std::span<const fc::Edge> edges;
    Group group;
};

const Layer kLayers[] = {
    {fc::kTesselation, Group::Tessellation},
    {fc::kRightEyebrow, Group::Eyebrows},
    {fc::kRightEye, Group::Eyes},
    {fc::kRightIris, Group::Irises},
    {fc::kLeftEyebrow, Group::Eyebrows},
    {fc::kLeftEye, Group::Eyes},
    {fc::kLeftIris, Group::Irises},
    {fc::kFaceOval, Group::FaceOval},
    {fc::kLips, Group::Lips},
};

struct Style {
    QColor color;
    bool visible = true;
    double width = 1;
};

Style styleFor(const SettingsObject& s, Group group)
{
    switch (group) {
    case Group::Tessellation:
        return {QColor(s.meshTessellationColor), s.meshTessellationVisible, s.meshTessellationWidth};
    case Group::FaceOval:
        return {QColor(s.meshFaceOvalColor), s.meshFaceOvalVisible, s.meshContourWidth};
    case Group::Lips:
        return {QColor(s.meshLipsColor), s.meshLipsVisible, s.meshContourWidth};
    case Group::Eyes:
        return {QColor(s.meshEyesColor), s.meshEyesVisible, s.meshContourWidth};
    case Group::Eyebrows:
        return {QColor(s.meshEyebrowsColor), s.meshEyebrowsVisible, s.meshContourWidth};
    case Group::Irises:
        return {QColor(s.meshIrisesColor), s.meshIrisesVisible, s.meshContourWidth};
    }
    return {};
}

// Lines are drawn as triangles: the scene graph ignores line widths other
// than 1 on Direct3D, Metal and Vulkan.
constexpr int kVerticesPerEdge = 6;

QSGGeometryNode* makeLayerNode()
{
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(new QSGFlatColorMaterial);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

// A rectangle `width` wide around a→b, extended by half the width at both
// ends so consecutive segments overlap instead of leaving notches.
void appendSegment(QSGGeometry::Point2D*& out, QPointF a, QPointF b, double width)
{
    const double h = width / 2;
    QPointF d = b - a;
    const double length = std::hypot(d.x(), d.y());
    d = length > 0 ? d / length * h : QPointF(h, 0);
    const QPointF n(-d.y(), d.x());
    a -= d;
    b += d;
    const QPointF corners[] = {a + n, a - n, b + n, a - n, b - n, b + n};
    for (const QPointF& p : corners)
        (out++)->set(static_cast<float>(p.x()), static_cast<float>(p.y()));
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
    if (source_) {
        disconnect(source_, nullptr, this, nullptr);
        disconnect(source_->settings(), nullptr, this, nullptr);
    }
    source_ = app;
    if (app) {
        connect(app, &AppController::landmarksChanged, this, &QQuickItem::update);
        connect(app->settings(), &SettingsObject::changed, this, &QQuickItem::update);
    }
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
        for (std::size_t i = 0; i < std::size(kLayers); ++i)
            root->appendChildNode(makeLayerNode());
    }

    static const std::vector<game_face::Landmark> kNone;
    const auto& landmarks = source_ ? source_->landmarks() : kNone;
    const QRectF rect = contentRect_.isEmpty() ? boundingRect() : contentRect_;
    const SettingsObject defaults;
    const SettingsObject& settings = source_ ? *source_->settings() : defaults;
    auto point = [&](std::size_t index) {
        const auto& lm = index < landmarks.size() ? landmarks[index] : landmarks.front();
        return QPointF(rect.x() + lm.x * rect.width(), rect.y() + lm.y * rect.height());
    };

    QSGNode* child = root->firstChild();
    for (const Layer& layer : kLayers) {
        auto* node = static_cast<QSGGeometryNode*>(child);
        Style style = styleFor(settings, layer.group);
        // Thinner than a pixel: draw 1 px and fade instead, since sub-pixel
        // triangles break up without antialiasing.
        if (style.width < 1) {
            style.color.setAlphaF(style.color.alphaF() * static_cast<float>(style.width));
            style.width = 1;
        }

        auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
        if (material->color() != style.color) {
            material->setColor(style.color);
            node->markDirty(QSGNode::DirtyMaterial);
        }

        QSGGeometry* geometry = node->geometry();
        const bool draw = style.visible && !landmarks.empty();
        geometry->allocate(draw ? static_cast<int>(layer.edges.size()) * kVerticesPerEdge : 0);
        if (draw) {
            QSGGeometry::Point2D* out = geometry->vertexDataAsPoint2D();
            for (const fc::Edge& edge : layer.edges)
                appendSegment(out, point(edge.start), point(edge.end), style.width);
        }
        node->markDirty(QSGNode::DirtyGeometry);
        child = child->nextSibling();
    }
    return root;
}
