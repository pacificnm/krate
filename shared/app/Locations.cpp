#include "app/Locations.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

namespace krate::locations {

QString key(Folder folder)
{
    switch (folder) {
    case Folder::Data: return QStringLiteral("data");
    case Folder::Recordings: return QStringLiteral("recordings");
    case Folder::Library: return QStringLiteral("library");
    case Folder::Exports: return QStringLiteral("exports");
    case Folder::Instruments: return QStringLiteral("instruments");
    }
    return {};
}

QString settingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/krate/settings.json");
}

QJsonObject readSettings(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString defaultFolder(Folder folder, const QJsonObject &settings)
{
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    switch (folder) {
    case Folder::Data: return genericData + QStringLiteral("/krate");
    case Folder::Recordings: return locations::folder(Folder::Data, settings) + QStringLiteral("/recordings");
    case Folder::Library: return locations::folder(Folder::Data, settings) + QStringLiteral("/library");
    case Folder::Exports: return QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + QStringLiteral("/Krate");
    case Folder::Instruments: return genericData;
    }
    return {};
}

QString folder(Folder folder, const QJsonObject &settings)
{
    const QString set = settings.value(u"folders").toObject().value(key(folder)).toString();
    return set.isEmpty() ? defaultFolder(folder, settings) : QDir::cleanPath(set);
}

QString folder(Folder f) { return folder(f, readSettings()); }

QString instrumentData(const QString &id) { return QDir(folder(Folder::Instruments)).filePath(id); }

bool isEmptyOrMissing(const QString &path)
{
    const QDir dir(path);
    return !dir.exists() || dir.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
}

namespace {

struct Tally
{
    qint64 files = 0, bytes = 0;
};

Tally tally(const QString &path)
{
    Tally t;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++t.files;
        t.bytes += it.fileInfo().size();
    }
    return t;
}

bool copyTree(const QString &from, const QString &to)
{
    if (!QDir().mkpath(to))
        return false;
    const QDir source(from);
    QDirIterator it(from, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        const QString target = QDir(to).filePath(source.relativeFilePath(info.filePath()));
        if (info.isDir() && !info.isSymLink()) {
            if (!QDir().mkpath(target))
                return false;
        } else if (info.isSymLink()) {
            if (!QFile::link(info.symLinkTarget(), target))
                return false;
        } else if (!QFile::copy(info.filePath(), target)) {
            return false;
        }
    }
    return true;
}

} // namespace

QString moveFolder(const QString &from, const QString &to)
{
    const QString a = QDir::cleanPath(QFileInfo(from).absoluteFilePath()), b = QDir::cleanPath(QFileInfo(to).absoluteFilePath());
    if (a == b)
        return {};
    if (!QFileInfo(a).isDir())
        return {}; // nothing there yet: nothing to move
    if (b.startsWith(a + u'/'))
        return QStringLiteral("%1 is inside %2").arg(b, a);
    if (!isEmptyOrMissing(b))
        return QStringLiteral("%1 isn't empty").arg(b);
    QDir().mkpath(QFileInfo(b).absolutePath());
    QDir().rmdir(b); // an empty one is in the way of a rename
    if (QDir().rename(a, b))
        return {};
    // Another drive: copy, check everything arrived, then let go of the old one.
    const Tally before = tally(a);
    if (!copyTree(a, b)) {
        QDir(b).removeRecursively();
        return QStringLiteral("couldn't copy into %1 (full, or not writable?)").arg(b);
    }
    const Tally after = tally(b);
    if (after.files != before.files || after.bytes != before.bytes) {
        QDir(b).removeRecursively();
        return QStringLiteral("the copy in %1 didn't match: left where it was").arg(b);
    }
    QDir(a).removeRecursively();
    return {};
}

} // namespace krate::locations
