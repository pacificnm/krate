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

QString label(Folder folder)
{
    switch (folder) {
    case Folder::Data: return QStringLiteral("Krate's data");
    case Folder::Recordings: return QStringLiteral("Recordings");
    case Folder::Library: return QStringLiteral("Song library");
    case Folder::Exports: return QStringLiteral("Exports");
    case Folder::Instruments: return QStringLiteral("Instruments' sounds");
    }
    return {};
}

QString about(Folder folder)
{
    switch (folder) {
    case Folder::Data:
        return QStringLiteral("Sequences, the sound library's index, crash reports and logs (and recordings and songs, unless they're elsewhere).");
    case Folder::Recordings: return QStringLiteral("The audio tracks' takes (WAV files).");
    case Folder::Library: return QStringLiteral("The songs you've added (tabs, MIDI files, backing tracks) and the library's database.");
    case Folder::Exports: return QStringLiteral("Where EXPORT writes the song and its stems.");
    case Folder::Instruments:
        return QStringLiteral("Each instrument's patterns, patches, kits, samples and cabinets, in a folder of its own (the instruments' own apps use it too).");
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

Roots localRoots()
{
    return {QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation)};
}

QString defaultFolder(Folder folder, const QJsonObject &settings, const Roots &roots)
{
    switch (folder) {
    case Folder::Data: return roots.dataHome + QStringLiteral("/krate");
    case Folder::Recordings: return locations::folder(Folder::Data, settings, roots) + QStringLiteral("/recordings");
    case Folder::Library: return locations::folder(Folder::Data, settings, roots) + QStringLiteral("/library");
    case Folder::Exports: return roots.music + QStringLiteral("/Krate");
    case Folder::Instruments: return roots.dataHome;
    }
    return {};
}

QString folder(Folder folder, const QJsonObject &settings, const Roots &roots)
{
    const QString set = settings.value(u"folders").toObject().value(key(folder)).toString();
    return set.isEmpty() ? defaultFolder(folder, settings, roots) : QDir::cleanPath(set);
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
