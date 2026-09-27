#include "game_face/core/settings.h"

#include "json_util.h"

#include <QJsonDocument>

#include <algorithm>

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
    return s;
}

std::string settingsToJson(const Settings& s)
{
    const QJsonObject root{
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
    };
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
