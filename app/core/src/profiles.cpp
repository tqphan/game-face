#include "game_face/core/profiles.h"

#include "json_util.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

namespace game_face {

using detail::toBool;
using detail::toNumber;
using detail::toQString;
using detail::toStdString;

namespace {

constexpr int kSchemaVersion = 2;

Trigger readTrigger(const QJsonObject& o)
{
    Trigger t;
    t.expression = toStdString(o.value("logic"));
    t.command = toStdString(o.value("pynput"));
    t.debounce_ms = std::max(0.0, toNumber(o.value("debounce"), 0));
    return t;
}

QJsonObject writeTrigger(const Trigger& t)
{
    return {
        {"logic", toQString(t.expression)},
        {"pynput", toQString(t.command)},
        {"debounce", t.debounce_ms},
    };
}

Binding readBinding(const QJsonObject& o)
{
    Binding b;
    b.name = toStdString(o.value("name"));
    b.enabled = toBool(o.value("enabled"), true);

    if (o.contains("advance") || o.contains("simple")) {
        b.simplified = toBool(o.value("simplified"), true);
        const QJsonObject simple = o.value("simple").toObject();
        b.simple.blendshape = toStdString(simple.value("blendshape"), b.simple.blendshape);
        b.simple.threshold = toNumber(simple.value("threshold"), b.simple.threshold);
        const QJsonObject commands = simple.value("pynput").toObject();
        b.simple.start_command = toStdString(commands.value("start"));
        b.simple.stop_command = toStdString(commands.value("stop"));

        const QJsonObject advanced = o.value("advance").toObject();
        b.start = readTrigger(advanced.value("start").toObject());
        b.stop = readTrigger(advanced.value("stop").toObject());
    } else {
        // Older layout: start/stop triggers directly on the binding.
        b.simplified = false;
        b.start = readTrigger(o.value("start").toObject());
        b.stop = readTrigger(o.value("stop").toObject());
    }
    return b;
}

QJsonObject writeBinding(const Binding& b)
{
    return {
        {"name", toQString(b.name)},
        {"enabled", b.enabled},
        {"simplified", b.simplified},
        {"simple", QJsonObject{
            {"blendshape", toQString(b.simple.blendshape)},
            {"threshold", b.simple.threshold},
            {"pynput", QJsonObject{
                {"start", toQString(b.simple.start_command)},
                {"stop", toQString(b.simple.stop_command)},
            }},
        }},
        {"advance", QJsonObject{
            {"start", writeTrigger(b.start)},
            {"stop", writeTrigger(b.stop)},
        }},
    };
}

} // namespace

const Profile* ProfileSet::selected() const
{
    if (selection < 0 || selection >= static_cast<int>(profiles.size()))
        return nullptr;
    return &profiles[static_cast<std::size_t>(selection)];
}

Result<ProfileSet> profilesFromJson(std::string_view json)
{
    auto root = detail::parseObject(json);
    if (!root)
        return std::unexpected(root.error());

    const int version = static_cast<int>(toNumber(root->value("schemaVersion"), 1));
    if (version > kSchemaVersion)
        return failure("profiles were saved by a newer version of game-face (schema " +
                       std::to_string(version) + ")");

    ProfileSet set;
    for (const QJsonValue& item : root->value("items").toArray()) {
        const QJsonObject o = item.toObject();
        Profile profile;
        profile.name = toStdString(o.value("name"));
        for (const QJsonValue& binding : o.value("bindings").toArray())
            profile.bindings.push_back(readBinding(binding.toObject()));
        set.profiles.push_back(std::move(profile));
    }

    const int count = static_cast<int>(set.profiles.size());
    set.selection = count == 0 ? 0 : std::clamp(static_cast<int>(toNumber(root->value("selection"), 0)), 0, count - 1);
    return set;
}

std::string profilesToJson(const ProfileSet& set)
{
    QJsonArray items;
    for (const Profile& profile : set.profiles) {
        QJsonArray bindings;
        for (const Binding& binding : profile.bindings)
            bindings.append(writeBinding(binding));
        items.append(QJsonObject{{"name", toQString(profile.name)}, {"bindings", bindings}});
    }
    const QJsonObject root{
        {"schemaVersion", kSchemaVersion},
        {"selection", set.selection},
        {"items", items},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString();
}

Result<ProfileSet> loadProfiles(const std::filesystem::path& path)
{
    auto text = detail::readFile(path);
    if (!text)
        return std::unexpected(text.error());
    auto set = profilesFromJson(*text);
    if (!set)
        return failure(path.string() + ": " + set.error().message);
    return set;
}

Result<void> saveProfiles(const ProfileSet& profiles, const std::filesystem::path& path)
{
    return detail::writeFileAtomically(path, QByteArray::fromStdString(profilesToJson(profiles)));
}

} // namespace game_face
