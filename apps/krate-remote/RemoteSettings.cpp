#include "RemoteSettings.h"

#include <optional>

#include <QDir>
#include <QJsonArray>
#include <QVariantMap>

namespace loc = krate::locations;
using loc::Folder;

namespace remote {

namespace {

std::optional<Folder> folderOf(const QString &key)
{
    for (Folder f : loc::kFolders)
        if (loc::key(f) == key)
            return f;
    return std::nullopt;
}

// The settings with what waits applied: where the folders will be.
QJsonObject after(const QJsonObject &settings)
{
    QJsonObject next = settings;
    QJsonObject folders = next.value(u"folders").toObject();
    const QJsonObject pending = settings.value(u"pending").toObject();
    for (auto it = pending.begin(); it != pending.end(); ++it)
        folders[it.key()] = it.value().toObject().value(u"path").toString();
    next[QStringLiteral("folders")] = folders;
    return next;
}

} // namespace

QVariantList folderRows(const QJsonObject &settings, const loc::Roots &roots)
{
    const QJsonObject pending = settings.value(u"pending").toObject();
    const QJsonObject next = after(settings);
    QVariantList rows;
    for (const Folder f : loc::kShown) {
        const QString key = loc::key(f);
        const bool waiting = pending.contains(key);
        rows << QVariantMap{
            {QStringLiteral("key"), key},
            {QStringLiteral("label"), loc::label(f)},
            {QStringLiteral("about"), loc::about(f)},
            {QStringLiteral("path"), loc::folder(f, settings, roots)},
            {QStringLiteral("defaultPath"), loc::defaultFolder(f, settings, roots)},
            {QStringLiteral("isDefault"), settings.value(u"folders").toObject().value(key).toString().isEmpty()},
            {QStringLiteral("pending"), waiting ? loc::folder(f, next, roots) : QString()},
            {QStringLiteral("pendingMove"), pending.value(key).toObject().value(u"move").toBool()},
            {QStringLiteral("atOnce"), f == Folder::Exports},
        };
    }
    return rows;
}

QJsonObject setFolder(QJsonObject settings, const QString &key, const QString &path, bool move, const loc::Roots &roots)
{
    const auto f = folderOf(key);
    if (!f)
        return settings;
    const QString clean = path.trimmed().isEmpty() ? QString() : QDir::cleanPath(path.trimmed());
    if (*f == Folder::Exports) { // nothing holds it open: at once
        QJsonObject folders = settings.value(u"folders").toObject();
        folders[key] = clean;
        settings[QStringLiteral("folders")] = folders;
        return settings;
    }
    QJsonObject next = settings;
    QJsonObject folders = next.value(u"folders").toObject();
    folders[key] = clean;
    next[QStringLiteral("folders")] = folders;
    if (loc::folder(*f, next, roots) == loc::folder(*f, settings, roots))
        return cancel(settings, key);
    QJsonObject pending = settings.value(u"pending").toObject();
    pending[key] = QJsonObject{{QStringLiteral("path"), clean}, {QStringLiteral("move"), move}};
    settings[QStringLiteral("pending")] = pending;
    return settings;
}

QJsonObject cancel(QJsonObject settings, const QString &key)
{
    QJsonObject pending = settings.value(u"pending").toObject();
    pending.remove(key);
    if (pending.isEmpty())
        settings.remove(QStringLiteral("pending"));
    else
        settings[QStringLiteral("pending")] = pending;
    return settings;
}

QJsonObject setPluginFolders(QJsonObject settings, const QStringList &folders)
{
    QStringList clean;
    for (const QString &f : folders)
        if (!f.trimmed().isEmpty() && !clean.contains(QDir::cleanPath(f.trimmed())))
            clean << QDir::cleanPath(f.trimmed());
    settings[QStringLiteral("pluginFolders")] = QJsonArray::fromStringList(clean);
    return settings;
}

} // namespace remote
