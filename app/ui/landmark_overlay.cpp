#include "landmark_overlay.h"

#include "app_controller.h"
#include "face_connections.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>
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

// A circle outline as line segments.
void appendCircle(QSGGeometry::Point2D*& out, QPointF centre, double radius, double width, int segments)
{
    for (int i = 0; i < segments; ++i) {
        const double a = 2 * std::numbers::pi * i / segments;
        const double b = 2 * std::numbers::pi * (i + 1) / segments;
        appendSegment(out, centre + QPointF(std::cos(a), std::sin(a)) * radius,
                      centre + QPointF(std::cos(b), std::sin(b)) * radius, width);
    }
}

constexpr int kCircleSegments = 48;
constexpr int kDotSegments = 16;

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
        connect(app, &AppController::headMouseChanged, this, &QQuickItem::update);
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
        root->appendChildNode(makeLayerNode());  // head mouse: range and deadzone rings
        root->appendChildNode(makeLayerNode());  // head mouse: the puck
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

    // The head mouse puck, top right: the outer ring is a full deflection,
    // the inner one the deadzone, the dot where the head is (x right, y up).
    auto* rings = static_cast<QSGGeometryNode*>(child);
    auto* puck = static_cast<QSGGeometryNode*>(child->nextSibling());
    const bool showPuck = source_ && source_->showHeadMousePuck();
    const double radius = std::clamp(std::min(rect.width(), rect.height()) * 0.09, 28.0, 70.0);
    const QPointF centre(rect.right() - radius - 14, rect.top() + radius + 14);
    const double deadzone = source_ ? std::clamp(source_->headMouseDeadzone(), 0.0, 0.9) : 0;
    rings->geometry()->allocate(showPuck ? 2 * kCircleSegments * kVerticesPerEdge : 0);
    if (showPuck) {
        QSGGeometry::Point2D* out = rings->geometry()->vertexDataAsPoint2D();
        appendCircle(out, centre, radius, 1.5, kCircleSegments);
        appendCircle(out, centre, std::max(radius * deadzone, 2.0), 1, kCircleSegments);
    }
    puck->geometry()->allocate(showPuck ? kDotSegments * kVerticesPerEdge : 0);
    if (showPuck) {
        const QPointF n = source_->headMousePuck();
        const QPointF at = centre + QPointF(std::clamp(n.x(), -1.0, 1.0), -std::clamp(n.y(), -1.0, 1.0)) * radius;
        QSGGeometry::Point2D* out = puck->geometry()->vertexDataAsPoint2D();
        appendCircle(out, at, 3, 6, kDotSegments);  // a filled dot: a thick ring around a tiny hole
    }
    const QColor ringColor(255, 255, 255, 170);
    const QColor puckColor(0x26, 0xa6, 0x9a);  // Material teal
    for (auto [node, color] : {std::pair{rings, ringColor}, std::pair{puck, puckColor}}) {
        auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
        if (material->color() != color) {
            material->setColor(color);
            node->markDirty(QSGNode::DirtyMaterial);
        }
        node->markDirty(QSGNode::DirtyGeometry);
    }
    return root;
}
