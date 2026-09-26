#pragma once

#include "game_face/core/result.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <filesystem>
#include <string>

namespace game_face::detail {

// Numbers from the original app are sometimes stored as strings ("0.35").
inline double toNumber(const QJsonValue& value, double fallback)
{
    if (value.isDouble())
        return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().trimmed().toDouble(&ok);
        if (ok)
            return parsed;
    }
    return fallback;
}

inline bool toBool(const QJsonValue& value, bool fallback)
{
    if (value.isBool())
        return value.toBool();
    if (value.isDouble())
        return value.toDouble() != 0;
    if (value.isString()) {
        const QString s = value.toString().trimmed().toLower();
        if (s == QLatin1String("true"))
            return true;
        if (s == QLatin1String("false"))
            return false;
    }
    return fallback;
}

inline std::string toStdString(const QJsonValue& value, const std::string& fallback = {})
{
    return value.isString() ? value.toString().toStdString() : fallback;
}

inline QString toQString(const std::string& s)
{
    return QString::fromStdString(s);
}

Result<QJsonObject> parseObject(std::string_view json);
Result<std::string> readFile(const std::filesystem::path& path);
Result<void> writeFileAtomically(const std::filesystem::path& path, const QByteArray& data);

} // namespace game_face::detail
