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
    };
    writeMesh(root, "tessellation", s.mesh_tessellation);
    writeMesh(root, "faceOval", s.mesh_face_oval);
    writeMesh(root, "lips", s.mesh_lips);
    writeMesh(root, "eyes", s.mesh_eyes);
    writeMesh(root, "eyebrows", s.mesh_eyebrows);
    writeMesh(root, "irises", s.mesh_irises);
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
