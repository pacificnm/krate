#include "app/JsonFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace krate::files {

LoadResult readVersionedJson(const QString &path, int maxVersion, QJsonObject *json, int *version)
{
    QFile file(path);
    if (!file.exists())
        return LoadResult::Missing;
    if (!file.open(QIODevice::ReadOnly))
        return LoadResult::Invalid;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return LoadResult::Invalid;

    const QJsonObject object = doc.object();
    const int v = object.value(u"version").toInt(0);
    if (v > maxVersion)
        return LoadResult::NewerVersion;
    if (v < 1)
        return LoadResult::Invalid;

    *json = object;
    if (version)
        *version = v;
    return LoadResult::Loaded;
}

bool writeJsonFile(const QString &path, const QJsonObject &json)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace krate::files
