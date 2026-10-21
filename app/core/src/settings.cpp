#include "game_face/core/settings.h"

#include "json_util.h"

#include <QJsonDocument>

#include <QJsonObject>

#include <algorithm>
#include <cctype>

namespace game_face {

using detail::toBool;
using detail::toNumber;
using detail::toQString;
using detail::toStdString;

namespace {

constexpr int kSchemaVersion = 2;

double unit(const QJsonValue& value, double fallback)
{
    return std::clamp(toNumber(value, fallback), 0.0, 1.0);
}

// "#rrggbb" or "#aarrggbb"; anything else keeps the fallback.
std::string hexColor(const QJsonValue& value, const std::string& fallback)
{
    const std::string s = toStdString(value);
    const bool valid = (s.size() == 7 || s.size() == 9) && s[0] == '#'
                       && std::all_of(s.begin() + 1, s.end(), [](unsigned char c) { return std::isxdigit(c); });
    return valid ? s : fallback;
}

void readMesh(const QJsonObject& o, const char* group, MeshStyle& style)
{
    const QString prefix = QStringLiteral("mesh.%1.").arg(QLatin1String(group));
    style.color = hexColor(o.value(prefix + "color"), style.color);
    style.visible = toBool(o.value(prefix + "visible"), style.visible);
}

void writeMesh(QJsonObject& o, const char* group, const MeshStyle& style)
{
    const QString prefix = QStringLiteral("mesh.%1.").arg(QLatin1String(group));
    o.insert(prefix + "color", toQString(style.color));
    o.insert(prefix + "visible", style.visible);
}

void readAxis(const QJsonObject& o, const QString& prefix, HeadMouseAxis& axis)
{
    if (o.contains(prefix + "source"))
        axis.source = toStdString(o.value(prefix + "source"));
    const QJsonValue neutral = o.value(prefix + "neutral");
    axis.neutral = neutral.isDouble() ? std::optional<double>(neutral.toDouble()) : std::nullopt;
    auto range = [](double value, double fallback) { return value > 0 ? value : fallback; };
    axis.range_negative = range(toNumber(o.value(prefix + "range.negative"), 0), axis.range_negative);
    axis.range_positive = range(toNumber(o.value(prefix + "range.positive"), 0), axis.range_positive);
    axis.gain = std::clamp(toNumber(o.value(prefix + "gain"), axis.gain), 0.05, 10.0);
    axis.invert = toBool(o.value(prefix + "invert"), axis.invert);
}

void writeAxis(QJsonObject& o, const QString& prefix, const HeadMouseAxis& axis)
{
    o.insert(prefix + "source", toQString(axis.source));
    o.insert(prefix + "neutral", axis.neutral ? QJsonValue(*axis.neutral) : QJsonValue(QJsonValue::Null));
    o.insert(prefix + "range.negative", axis.range_negative);
    o.insert(prefix + "range.positive", axis.range_positive);
    o.insert(prefix + "gain", axis.gain);
    o.insert(prefix + "invert", axis.invert);
}

void readGroup(const QJsonObject& o, const char* name, HeadMouseGroup& group)
{
    const QString prefix = QStringLiteral("head.mouse.%1.").arg(QLatin1String(name));
    if (auto mode = headMouseModeFromString(toStdString(o.value(prefix + "mode"))))
        group.mode = *mode;
    group.deadzone = std::clamp(toNumber(o.value(prefix + "deadzone"), group.deadzone), 0.0, 0.9);
    group.smoothing = std::clamp(toNumber(o.value(prefix + "smoothing"), group.smoothing), 0.0, 1.0);
    readAxis(o, prefix + "x.", group.x);
    readAxis(o, prefix + "y.", group.y);
}

void writeGroup(QJsonObject& o, const char* name, const HeadMouseGroup& group)
{
    const QString prefix = QStringLiteral("head.mouse.%1.").arg(QLatin1String(name));
    o.insert(prefix + "mode", toQString(std::string(headMouseModeName(group.mode))));
    o.insert(prefix + "deadzone", group.deadzone);
    o.insert(prefix + "smoothing", group.smoothing);
    writeAxis(o, prefix + "x.", group.x);
    writeAxis(o, prefix + "y.", group.y);
}

} // namespace

Result<Settings> settingsFromJson(std::string_view json)
{
    auto root = detail::parseObject(json);
    if (!root)
        return std::unexpected(root.error());
    const QJsonObject& o = *root;

    Settings s;
    s.theme = toStdString(o.value("theme"), s.theme);
    if (s.theme != "dark" && s.theme != "light")
        s.theme = "dark";
    s.language = toStdString(o.value("language"), s.language);
    s.camera_id = toStdString(o.value("camera.id"));
    s.webcam_opacity = unit(o.value("webcam.opacity"), s.webcam_opacity);
    s.landmarks_opacity = unit(o.value("landmarks.opacity"), s.landmarks_opacity);
    s.detection_confidence = unit(o.value("detection.confidence"), s.detection_confidence);
    s.presence_confidence = unit(o.value("presence.confidence"), s.presence_confidence);
    s.tracking_confidence = unit(o.value("tracking.confidence"), s.tracking_confidence);
    s.auto_start_tracking = toBool(o.value("auto.start.prediction"), s.auto_start_tracking);
    s.auto_start_with_os = toBool(o.value("auto.start.with.windows"), s.auto_start_with_os);
    s.run_on_secure_desktop = toBool(o.value("run.on.secured.desktop"), s.run_on_secure_desktop);
    s.allow_input_simulation = toBool(o.value("allow.input.simulation"), s.allow_input_simulation);
    s.auto_save_settings = toBool(o.value("auto.save.settings"), s.auto_save_settings);
    s.auto_save_profiles = toBool(o.value("auto.save.profiles"), s.auto_save_profiles);
    s.lock_ui = toBool(o.value("lock.ui"), s.lock_ui);
    s.confirm_on_close = toBool(o.value("confirm.on.close"), s.confirm_on_close);
    s.settle_on_stop = toBool(o.value("settle.on.stop"), s.settle_on_stop);
    s.start_sound = toStdString(o.value("sound.start"));
    s.stop_sound = toStdString(o.value("sound.stop"));
    s.sound_volume = unit(o.value("sound.volume"), s.sound_volume);
    readMesh(o, "tessellation", s.mesh_tessellation);
    readMesh(o, "faceOval", s.mesh_face_oval);
    readMesh(o, "lips", s.mesh_lips);
    readMesh(o, "eyes", s.mesh_eyes);
    readMesh(o, "eyebrows", s.mesh_eyebrows);
    readMesh(o, "irises", s.mesh_irises);
    s.mesh_contour_width = std::clamp(toNumber(o.value("mesh.contour.width"), s.mesh_contour_width), 1.0, 6.0);
    s.mesh_tessellation_width =
        std::clamp(toNumber(o.value("mesh.tessellation.width"), s.mesh_tessellation_width), 0.5, 3.0);
    s.text_scale = std::clamp(toNumber(o.value("text.scale"), s.text_scale), 1.0, 2.0);
    HeadMouseSettings& hm = s.head_mouse;
    hm.enabled = toBool(o.value("head.mouse.enabled"), hm.enabled);
    hm.click_freeze_ms = std::clamp(toNumber(o.value("head.mouse.click.freeze"), hm.click_freeze_ms), 0.0, 1000.0);
    readGroup(o, "pointer", hm.pointer);
    readGroup(o, "scroll", hm.scroll);
    return s;
}

std::string settingsToJson(const Settings& s)
{
    QJsonObject root{
        {"schemaVersion", kSchemaVersion},
        {"theme", toQString(s.theme)},
        {"language", toQString(s.language)},
        {"camera.id", toQString(s.camera_id)},
        {"webcam.opacity", s.webcam_opacity},
        {"landmarks.opacity", s.landmarks_opacity},
        {"detection.confidence", s.detection_confidence},
        {"presence.confidence", s.presence_confidence},
        {"tracking.confidence", s.tracking_confidence},
        {"auto.start.prediction", s.auto_start_tracking},
        {"auto.start.with.windows", s.auto_start_with_os},
        {"run.on.secured.desktop", s.run_on_secure_desktop},
        {"allow.input.simulation", s.allow_input_simulation},
        {"auto.save.settings", s.auto_save_settings},
        {"auto.save.profiles", s.auto_save_profiles},
        {"lock.ui", s.lock_ui},
        {"confirm.on.close", s.confirm_on_close},
        {"settle.on.stop", s.settle_on_stop},
        {"sound.start", toQString(s.start_sound)},
        {"sound.stop", toQString(s.stop_sound)},
        {"sound.volume", s.sound_volume},
        {"mesh.contour.width", s.mesh_contour_width},
        {"mesh.tessellation.width", s.mesh_tessellation_width},
        {"text.scale", s.text_scale},
    };
    writeMesh(root, "tessellation", s.mesh_tessellation);
    writeMesh(root, "faceOval", s.mesh_face_oval);
    writeMesh(root, "lips", s.mesh_lips);
    writeMesh(root, "eyes", s.mesh_eyes);
    writeMesh(root, "eyebrows", s.mesh_eyebrows);
    writeMesh(root, "irises", s.mesh_irises);
    root.insert("head.mouse.enabled", s.head_mouse.enabled);
    root.insert("head.mouse.click.freeze", s.head_mouse.click_freeze_ms);
    writeGroup(root, "pointer", s.head_mouse.pointer);
    writeGroup(root, "scroll", s.head_mouse.scroll);
    return QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString();
}

Result<Settings> loadSettings(const std::filesystem::path& path)
{
    auto text = detail::readFile(path);
    if (!text)
        return std::unexpected(text.error());
    auto settings = settingsFromJson(*text);
    if (!settings)
        return failure(path.string() + ": " + settings.error().message);
    return settings;
}

Result<void> saveSettings(const Settings& settings, const std::filesystem::path& path)
{
    return detail::writeFileAtomically(path, QByteArray::fromStdString(settingsToJson(settings)));
}

} // namespace game_face
