#include "json_util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace game_face::detail {

namespace {

QString toQPath(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

} // namespace

Result<QJsonObject> parseObject(std::string_view json)
{
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(
        QByteArray(json.data(), static_cast<qsizetype>(json.size())), &error);
    if (error.error != QJsonParseError::NoError)
        return failure("invalid JSON at offset " + std::to_string(error.offset) + ": " +
                       error.errorString().toStdString());
    if (!document.isObject())
        return failure("expected a JSON object");
    return document.object();
}

Result<std::string> readFile(const std::filesystem::path& path)
{
    QFile file(toQPath(path));
    if (!file.open(QIODevice::ReadOnly))
        return failure("could not open " + path.string() + ": " + file.errorString().toStdString());
    return file.readAll().toStdString();
}

Result<void> writeFileAtomically(const std::filesystem::path& path, const QByteArray& data)
{
    const QString target = toQPath(path);
    if (!QDir().mkpath(QFileInfo(target).absolutePath()))
        return failure("could not create the folder for " + path.string());

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return failure("could not write " + path.string() + ": " + file.errorString().toStdString());
    return {};
}

} // namespace game_face::detail
