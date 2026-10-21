#include "face_render.h"

#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <random>

namespace face_render {

using game_face::failure;

namespace {

// How the scan sits in its file: units to cm, and the point that becomes
// the model origin (roughly the middle of the head, behind the eyes).
// Measured so the frontal render tracks as a frontal face at 50 cm; see
// tst_face_signals.
constexpr float kCmPerUnit = 5.5f;
constexpr float kOriginX = 0.0f, kOriginY = 1.9f, kOriginZ = 0.9f;

struct GlbChunk {
    QByteArray json;
    QByteArray bin;
};

Result<GlbChunk> readGlb(const std::filesystem::path& path)
{
    QFile file(QString::fromStdU16String(path.u16string()));
    if (!file.open(QIODevice::ReadOnly))
        return failure("could not open " + path.string());
    const QByteArray data = file.readAll();
    auto u32 = [&](qsizetype at) {
        std::uint32_t v = 0;
        std::memcpy(&v, data.constData() + at, 4);
        return v;
    };
    if (data.size() < 20 || u32(0) != 0x46546C67)  // "glTF"
        return failure(path.string() + " is not a binary glTF file");
    GlbChunk chunks;
    qsizetype at = 12;
    while (at + 8 <= data.size()) {
        const qsizetype length = u32(at);
        const std::uint32_t type = u32(at + 4);
        if (at + 8 + length > data.size())
            break;
        const QByteArray body = data.mid(at + 8, length);
        if (type == 0x4E4F534A)  // "JSON"
            chunks.json = body;
        else if (type == 0x004E4942)  // "BIN\0"
            chunks.bin = body;
        at += 8 + length;
    }
    if (chunks.json.isEmpty() || chunks.bin.isEmpty())
        return failure(path.string() + ": missing JSON or BIN chunk");
    return chunks;
}

// Reads accessor `index` as floats (or integers as uint32), `components` per element.
template <class T>
Result<std::vector<T>> readAccessor(const QJsonObject& gltf, const QByteArray& bin, int index, int components)
{
    const QJsonObject accessor = gltf["accessors"].toArray().at(index).toObject();
    const QJsonObject view = gltf["bufferViews"].toArray().at(accessor["bufferView"].toInt()).toObject();
    const int type = accessor["componentType"].toInt();
    const qsizetype count = accessor["count"].toInteger();
    const qsizetype offset = view["byteOffset"].toInteger() + accessor["byteOffset"].toInteger();
    const int size = type == 5126 || type == 5125 ? 4 : type == 5123 ? 2 : 1;
    const qsizetype stride = view.contains("byteStride") ? view["byteStride"].toInteger() : size * components;
    if (offset + (count - 1) * stride + size * components > bin.size())
        return failure("accessor " + std::to_string(index) + " runs past the buffer");
    std::vector<T> out;
    out.reserve(static_cast<std::size_t>(count * components));
    for (qsizetype i = 0; i < count; ++i) {
        const char* p = bin.constData() + offset + i * stride;
        for (int c = 0; c < components; ++c) {
            if (type == 5126) {
                float f;
                std::memcpy(&f, p + c * 4, 4);
                out.push_back(static_cast<T>(f));
            } else if (type == 5125) {
                std::uint32_t v;
                std::memcpy(&v, p + c * 4, 4);
                out.push_back(static_cast<T>(v));
            } else if (type == 5123) {
                std::uint16_t v;
                std::memcpy(&v, p + c * 2, 2);
                out.push_back(static_cast<T>(v));
            } else {
                out.push_back(static_cast<T>(static_cast<std::uint8_t>(p[c])));
            }
        }
    }
    return out;
}

struct Vec3 {
    double x, y, z;
};

Vec3 rotate(const std::array<float, 16>& m, double x, double y, double z)
{
    return {m[0] * x + m[4] * y + m[8] * z, m[1] * x + m[5] * y + m[9] * z, m[2] * x + m[6] * y + m[10] * z};
}

QRgb sampleBilinear(const QImage& texture, double u, double v)
{
    const double x = std::clamp(u, 0.0, 1.0) * (texture.width() - 1);
    const double y = std::clamp(v, 0.0, 1.0) * (texture.height() - 1);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, texture.width() - 1), y1 = std::min(y0 + 1, texture.height() - 1);
    const double fx = x - x0, fy = y - y0;
    const auto* r0 = reinterpret_cast<const QRgb*>(texture.constScanLine(y0));
    const auto* r1 = reinterpret_cast<const QRgb*>(texture.constScanLine(y1));
    auto mix = [&](int shift) {
        auto channel = [shift](QRgb c) { return static_cast<double>((c >> shift) & 0xff); };
        const double top = channel(r0[x0]) * (1 - fx) + channel(r0[x1]) * fx;
        const double bottom = channel(r1[x0]) * (1 - fx) + channel(r1[x1]) * fx;
        return static_cast<int>(top * (1 - fy) + bottom * fy);
    };
    return qRgb(mix(16), mix(8), mix(0));
}

} // namespace

Result<HeadModel> HeadModel::load(const std::filesystem::path& glb, const std::filesystem::path& texture)
{
    auto chunks = readGlb(glb);
    if (!chunks)
        return std::unexpected(chunks.error());
    const QJsonObject gltf = QJsonDocument::fromJson(chunks->json).object();
    const QJsonObject primitive =
        gltf["meshes"].toArray().at(0)["primitives"].toArray().at(0).toObject();
    const QJsonObject attributes = primitive["attributes"].toObject();
    if (!attributes.contains("POSITION") || !attributes.contains("NORMAL") || !attributes.contains("TEXCOORD_0")
        || !primitive.contains("indices"))
        return failure(glb.string() + ": the first mesh needs POSITION, NORMAL, TEXCOORD_0 and indices");

    auto positions = readAccessor<float>(gltf, chunks->bin, attributes["POSITION"].toInt(), 3);
    auto normals = readAccessor<float>(gltf, chunks->bin, attributes["NORMAL"].toInt(), 3);
    auto uvs = readAccessor<float>(gltf, chunks->bin, attributes["TEXCOORD_0"].toInt(), 2);
    auto indices = readAccessor<std::uint32_t>(gltf, chunks->bin, primitive["indices"].toInt(), 1);
    for (const auto* r : {&positions, &normals, &uvs}) {
        if (!*r)
            return std::unexpected(r->error());
    }
    if (!indices)
        return std::unexpected(indices.error());

    HeadModel model;
    const std::size_t count = positions->size() / 3;
    if (normals->size() != count * 3 || uvs->size() != count * 2)
        return failure(glb.string() + ": attribute counts differ");
    model.vertices_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float* p = positions->data() + i * 3;
        const float* n = normals->data() + i * 3;
        const float* t = uvs->data() + i * 2;
        model.vertices_.push_back({(p[0] - kOriginX) * kCmPerUnit, (p[1] - kOriginY) * kCmPerUnit,
                                   (p[2] - kOriginZ) * kCmPerUnit, n[0], n[1], n[2], t[0], t[1]});
    }
    model.indices_ = std::move(*indices);
    for (auto index : model.indices_) {
        if (index >= count)
            return failure(glb.string() + ": index out of range");
    }

    model.texture_ = QImage(QString::fromStdU16String(texture.u16string()));
    if (model.texture_.isNull())
        return failure("could not read " + texture.string());
    model.texture_ = model.texture_.convertToFormat(QImage::Format_RGB32);
    return model;
}

QImage render(const HeadModel& model, const std::optional<HeadPose>& pose, const Camera& camera, const Look& look)
{
    const int ss = std::max(1, camera.supersample);
    const int width = camera.size.width() * ss;
    const int height = camera.size.height() * ss;
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        // A slight vertical gradient, like a wall lit from above.
        const double shade = 1.08 - 0.16 * y / height;
        const QRgb c = qRgb(std::min(255, static_cast<int>(look.background.red() * shade * look.brightness)),
                            std::min(255, static_cast<int>(look.background.green() * shade * look.brightness)),
                            std::min(255, static_cast<int>(look.background.blue() * shade * look.brightness)));
        std::fill_n(reinterpret_cast<QRgb*>(image.scanLine(y)), width, c);
    }

    if (pose) {
        HeadPose placed = *pose;
        placed.z = -kDefaultDistanceCm + pose->z;
        const std::array<float, 16> m = game_face::matrixFromHeadPose(placed);
        const double focal = (height / 2.0) / std::tan(camera.vertical_fov_deg * std::numbers::pi / 360.0);
        double lx = look.light_x, ly = look.light_y, lz = look.light_z;
        const double ll = std::sqrt(lx * lx + ly * ly + lz * lz);
        lx /= ll, ly /= ll, lz /= ll;

        struct Projected {
            double sx, sy, inv_w;  // screen position and 1 / depth
            double u, v, light;
        };
        const auto& vertices = model.vertices();
        std::vector<Projected> projected(vertices.size());
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto& v = vertices[i];
            const Vec3 p = rotate(m, v.x, v.y, v.z);
            const double cx = p.x + m[12], cy = p.y + m[13], cz = p.z + m[14];
            const double depth = std::max(-cz, 1e-3);
            const Vec3 n = rotate(m, v.nx, v.ny, v.nz);
            const double diffuse = std::max(0.0, n.x * lx + n.y * ly + n.z * lz);
            projected[i] = {width / 2.0 + focal * cx / depth, height / 2.0 - focal * cy / depth, 1.0 / depth,
                            v.u, v.v, (look.ambient + (1 - look.ambient) * diffuse) * look.brightness};
        }

        std::vector<double> depth_buffer(static_cast<std::size_t>(width) * height, 0.0);
        const QImage& texture = model.texture();
        const auto& indices = model.indices();
        for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
            const Projected& a = projected[indices[t]];
            const Projected& b = projected[indices[t + 1]];
            const Projected& c = projected[indices[t + 2]];
            const double area = (b.sx - a.sx) * (c.sy - a.sy) - (b.sy - a.sy) * (c.sx - a.sx);
            if (std::abs(area) < 1e-9)
                continue;
            const int x0 = std::max(0, static_cast<int>(std::floor(std::min({a.sx, b.sx, c.sx}))));
            const int x1 = std::min(width - 1, static_cast<int>(std::ceil(std::max({a.sx, b.sx, c.sx}))));
            const int y0 = std::max(0, static_cast<int>(std::floor(std::min({a.sy, b.sy, c.sy}))));
            const int y1 = std::min(height - 1, static_cast<int>(std::ceil(std::max({a.sy, b.sy, c.sy}))));
            for (int y = y0; y <= y1; ++y) {
                auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
                const double py = y + 0.5;
                for (int x = x0; x <= x1; ++x) {
                    const double px = x + 0.5;
                    const double w0 = ((b.sx - px) * (c.sy - py) - (b.sy - py) * (c.sx - px)) / area;
                    const double w1 = ((c.sx - px) * (a.sy - py) - (c.sy - py) * (a.sx - px)) / area;
                    const double w2 = 1 - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0)
                        continue;
                    const double inv_w = w0 * a.inv_w + w1 * b.inv_w + w2 * c.inv_w;
                    double& stored = depth_buffer[static_cast<std::size_t>(y) * width + x];
                    if (inv_w <= stored)
                        continue;
                    stored = inv_w;
                    // Perspective-correct interpolation.
                    const double k0 = w0 * a.inv_w / inv_w, k1 = w1 * b.inv_w / inv_w, k2 = w2 * c.inv_w / inv_w;
                    const double u = k0 * a.u + k1 * b.u + k2 * c.u;
                    const double v = k0 * a.v + k1 * b.v + k2 * c.v;
                    const double light = k0 * a.light + k1 * b.light + k2 * c.light;
                    const QRgb texel = sampleBilinear(texture, u, v);
                    row[x] = qRgb(std::min(255, static_cast<int>(qRed(texel) * light)),
                                  std::min(255, static_cast<int>(qGreen(texel) * light)),
                                  std::min(255, static_cast<int>(qBlue(texel) * light)));
                }
            }
        }
    }

    if (ss > 1)
        image = image.scaled(camera.size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return image.convertToFormat(QImage::Format_RGB888);
}

QImage degrade(const QImage& source, const Degrade& flaws)
{
    QImage image = source.convertToFormat(QImage::Format_RGB888);
    const int w = image.width(), h = image.height();
    auto at = [&](int x, int y) { return image.scanLine(y) + x * 3; };

    if (flaws.brightness != 1.0) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w * 3; ++x) {
                uchar& p = image.scanLine(y)[x];
                p = static_cast<uchar>(std::clamp(p * flaws.brightness, 0.0, 255.0));
            }
    }
    if (flaws.blur_px > 0) {
        const int r = static_cast<int>(std::lround(flaws.blur_px));
        QImage copy = image;
        // Horizontal then vertical box blur.
        for (int pass = 0; pass < 2; ++pass) {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    int sum[3] = {0, 0, 0}, n = 0;
                    for (int k = -r; k <= r; ++k) {
                        const int sx = pass == 0 ? std::clamp(x + k, 0, w - 1) : x;
                        const int sy = pass == 1 ? std::clamp(y + k, 0, h - 1) : y;
                        const uchar* p = copy.constScanLine(sy) + sx * 3;
                        sum[0] += p[0], sum[1] += p[1], sum[2] += p[2];
                        ++n;
                    }
                    uchar* out = at(x, y);
                    out[0] = static_cast<uchar>(sum[0] / n);
                    out[1] = static_cast<uchar>(sum[1] / n);
                    out[2] = static_cast<uchar>(sum[2] / n);
                }
            copy = image;
        }
    }
    if (flaws.noise_sigma > 0) {
        std::mt19937 rng(flaws.seed);
        std::normal_distribution<double> noise(0.0, flaws.noise_sigma);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w * 3; ++x) {
                uchar& p = image.scanLine(y)[x];
                p = static_cast<uchar>(std::clamp(p + noise(rng), 0.0, 255.0));
            }
    }
    if (flaws.jpeg_quality > 0) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPG", std::clamp(flaws.jpeg_quality, 1, 100));
        image.loadFromData(bytes, "JPG");
        image = image.convertToFormat(QImage::Format_RGB888);
    }
    return image;
}

namespace {

bool readPose(const QJsonObject& o, HeadPose& pose)
{
    static const std::pair<const char*, double HeadPose::*> kKeys[] = {
        {"yaw", &HeadPose::yaw}, {"pitch", &HeadPose::pitch}, {"roll", &HeadPose::roll},
        {"x", &HeadPose::x},     {"y", &HeadPose::y},         {"z", &HeadPose::z},
    };
    for (auto it = o.begin(); it != o.end(); ++it) {
        bool known = false;
        for (const auto& [key, member] : kKeys) {
            if (it.key() == QLatin1String(key)) {
                if (!it.value().isDouble())
                    return false;
                pose.*member = it.value().toDouble();
                known = true;
            }
        }
        if (!known)
            return false;
    }
    return true;
}

HeadPose lerp(const HeadPose& a, const HeadPose& b, double f)
{
    auto mix = [f](double x, double y) { return x + (y - x) * f; };
    return {mix(a.yaw, b.yaw), mix(a.pitch, b.pitch), mix(a.roll, b.roll),
            mix(a.x, b.x),     mix(a.y, b.y),         mix(a.z, b.z)};
}

} // namespace

Result<Motion> motionFromJson(std::string_view json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json.data(), static_cast<qsizetype>(json.size())), &error);
    if (!doc.isObject())
        return failure("motion script: " + error.errorString().toStdString());
    const QJsonObject root = doc.object();

    Motion motion;
    motion.fps = root.value("fps").toDouble(30);
    if (motion.fps <= 0 || motion.fps > 240)
        return failure("motion script: fps must be 1..240");
    HeadPose pose;
    if (!readPose(root.value("start").toObject(), pose))
        return failure("motion script: bad \"start\" pose");

    const double step = 1.0 / motion.fps;
    double time = 0;
    auto push = [&](const std::optional<HeadPose>& p) {
        motion.frames.push_back({time, p});
        time += step;
    };
    push(pose);
    for (const QJsonValue& value : root.value("steps").toArray()) {
        const QJsonObject s = value.toObject();
        if (s.contains("hold") || s.contains("hide")) {
            const bool hide = s.contains("hide");
            const double seconds = s.value(hide ? "hide" : "hold").toDouble(-1);
            if (seconds < 0)
                return failure("motion script: hold/hide needs seconds");
            const int n = static_cast<int>(std::lround(seconds * motion.fps));
            for (int i = 0; i < n; ++i)
                push(hide ? std::nullopt : std::optional<HeadPose>(pose));
        } else if (s.contains("to")) {
            HeadPose target = pose;
            if (!readPose(s.value("to").toObject(), target))
                return failure("motion script: bad \"to\" pose");
            const double over = s.value("over").toDouble(-1);
            if (over < 0)
                return failure("motion script: \"to\" needs \"over\" seconds");
            const int n = std::max(1, static_cast<int>(std::lround(over * motion.fps)));
            for (int i = 1; i <= n; ++i) {
                const double f = static_cast<double>(i) / n;
                const double eased = f * f * (3 - 2 * f);  // smoothstep
                push(lerp(pose, target, eased));
            }
            pose = target;
        } else {
            return failure("motion script: each step needs hold, hide or to");
        }
    }
    return motion;
}

} // namespace face_render
