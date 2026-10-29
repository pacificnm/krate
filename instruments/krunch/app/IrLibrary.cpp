#include "IrLibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include "dsp/Wav.h"
#include "song/Zip.h"

Q_LOGGING_CATEGORY(lcIrs, "krunch.irs")

namespace {

constexpr qsizetype kMaxErrors = 5;
constexpr qint64 kMaxFileBytes = qint64(64) << 20;

void noteError(IrLibrary::ImportResult &result, const QString &what)
{
    ++result.failed;
    if (result.errors.size() < kMaxErrors)
        result.errors << what;
}

// macOS leaves ._ files and __MACOSX folders in zips; they aren't IRs.
bool isJunk(const QString &path)
{
    return path.contains(u"__MACOSX") || QFileInfo(path).fileName().startsWith(u"._");
}

bool isWav(const QString &path) { return path.endsWith(u".wav", Qt::CaseInsensitive); }

// A null QString binds as SQL NULL; the columns want text.
QVariant text(const QString &s) { return QVariant(s.isNull() ? QStringLiteral("") : s); }

} // namespace

IrLibrary::IrLibrary(const QString &directory)
    : m_directory(QDir(directory).absolutePath())
    , m_connection(QStringLiteral("irs-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QDir().mkpath(m_directory);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(QDir(m_directory).filePath(QStringLiteral("irs.db")));
    if (!db.open()) {
        qCWarning(lcIrs) << "can't open the IR library:" << db.lastError().text();
        return;
    }
    m_open = createSchema();
}

IrLibrary::~IrLibrary()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

bool IrLibrary::createSchema()
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("PRAGMA user_version"));
    const int version = q.next() ? q.value(0).toInt() : 0;
    if (version == kSchemaVersion)
        return true;
    if (version > kSchemaVersion) {
        qCWarning(lcIrs) << "the IR library was made by a newer Krunch";
        return false;
    }
    for (const QString &sql : {
             QStringLiteral("CREATE TABLE IF NOT EXISTS irs ("
                            " id TEXT PRIMARY KEY,"
                            " pack TEXT NOT NULL DEFAULT '',"
                            " folder TEXT NOT NULL DEFAULT '',"
                            " name TEXT NOT NULL,"
                            " channels INTEGER NOT NULL,"
                            " rate INTEGER NOT NULL,"
                            " frames INTEGER NOT NULL,"
                            " favourite INTEGER NOT NULL DEFAULT 0,"
                            " added TEXT NOT NULL DEFAULT (datetime('now')))"),
             QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion),
         }) {
        if (!q.exec(sql)) {
            qCWarning(lcIrs) << "IR library schema:" << q.lastError().text();
            return false;
        }
    }
    return true;
}

IrLibrary::ImportResult IrLibrary::import(const QStringList &paths)
{
    ImportResult result;
    if (!m_open)
        return result;
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isDir()) {
            const QDir root(info.absoluteFilePath());
            QDirIterator it(root.path(), {QStringLiteral("*.wav"), QStringLiteral("*.WAV"), QStringLiteral("*.zip"), QStringLiteral("*.ZIP")},
                            QDir::Files, QDirIterator::Subdirectories);
            QStringList found;
            while (it.hasNext())
                found << it.next();
            found.sort();
            for (const QString &file : found) {
                if (isJunk(file))
                    continue;
                if (isWav(file)) // the folder is the pack, its subfolders the folders
                    importFile(file, info.fileName(), QFileInfo(root.relativeFilePath(file)).path().replace(u'.', QString()), result);
                else
                    importZip(file, result);
            }
        } else if (path.endsWith(u".zip", Qt::CaseInsensitive)) {
            importZip(path, result);
        } else if (isWav(path) && !isJunk(path)) {
            importFile(path, info.dir().dirName(), QString(), result);
        } else {
            noteError(result, info.fileName() + QStringLiteral(": not a WAV file, zip or folder"));
        }
    }
    db.commit();
    return result;
}

void IrLibrary::importFile(const QString &path, const QString &pack, const QString &folder, ImportResult &result)
{
    QFile file(path);
    if (file.size() > kMaxFileBytes || !file.open(QIODevice::ReadOnly)) {
        noteError(result, QFileInfo(path).fileName() + QStringLiteral(": can't read it"));
        return;
    }
    importBytes(file.readAll(), pack, folder, QFileInfo(path).completeBaseName(), result);
}

void IrLibrary::importZip(const QString &path, ImportResult &result)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        noteError(result, QFileInfo(path).fileName() + QStringLiteral(": can't read it"));
        return;
    }
    const QByteArray bytes = file.readAll();
    krate::song::ZipArchive zip;
    if (!zip.open({reinterpret_cast<const std::uint8_t *>(bytes.constData()), std::size_t(bytes.size())})) {
        noteError(result, QFileInfo(path).fileName() + QStringLiteral(": not a zip we can read"));
        return;
    }
    const QString pack = QFileInfo(path).completeBaseName();
    for (const auto &entry : zip.entries()) {
        const QString name = QString::fromStdString(entry.name);
        if (!isWav(name) || isJunk(name))
            continue;
        const auto contents = zip.read(entry, std::size_t(kMaxFileBytes));
        if (!contents) {
            noteError(result, name + QStringLiteral(": damaged in the zip"));
            continue;
        }
        const QString folder = name.contains(u'/') ? name.section(u'/', 0, -2) : QString();
        importBytes(QByteArray(reinterpret_cast<const char *>(contents->data()), qsizetype(contents->size())), pack, folder,
                    QFileInfo(name).completeBaseName(), result);
    }
}

void IrLibrary::importBytes(const QByteArray &bytes, const QString &pack, const QString &folder, const QString &name,
                            ImportResult &result)
{
    krunch::WavData wav;
    std::string error;
    if (!krunch::readWav({reinterpret_cast<const std::uint8_t *>(bytes.constData()), std::size_t(bytes.size())}, &wav, &error)) {
        noteError(result, name + QStringLiteral(": ") + QString::fromStdString(error));
        return;
    }
    const QString id = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    if (find(id)) {
        ++result.duplicates;
        return;
    }
    QFile copy(QDir(m_directory).filePath(id + QStringLiteral(".wav")));
    if (!copy.open(QIODevice::WriteOnly) || copy.write(bytes) != bytes.size()) {
        noteError(result, name + QStringLiteral(": can't write the library's copy"));
        return;
    }
    copy.close();
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT INTO irs (id, pack, folder, name, channels, rate, frames) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    for (const QVariant &v : {text(id), text(pack), text(folder), text(name), QVariant(wav.channels),
                              QVariant(wav.rate), QVariant(qlonglong(wav.left.size()))})
        q.addBindValue(v);
    if (!q.exec()) {
        copy.remove();
        noteError(result, name + QStringLiteral(": ") + q.lastError().text());
        return;
    }
    ++result.added;
}

namespace {
IrLibrary::Ir fromQuery(const QSqlQuery &q)
{
    IrLibrary::Ir ir;
    ir.id = q.value(0).toString();
    ir.pack = q.value(1).toString();
    ir.folder = q.value(2).toString();
    ir.name = q.value(3).toString();
    ir.channels = q.value(4).toInt();
    ir.rate = q.value(5).toInt();
    ir.frames = q.value(6).toInt();
    ir.favourite = q.value(7).toBool();
    return ir;
}
const QString kColumns = QStringLiteral("id, pack, folder, name, channels, rate, frames, favourite");
} // namespace

QList<IrLibrary::Ir> IrLibrary::list(const QString &filter) const
{
    QList<Ir> result;
    if (!m_open)
        return result;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT %1 FROM irs ORDER BY favourite DESC, pack COLLATE NOCASE, folder COLLATE NOCASE, name COLLATE NOCASE").arg(kColumns));
    const QStringList words = filter.split(u' ', Qt::SkipEmptyParts);
    while (q.next()) {
        Ir ir = fromQuery(q);
        const QString text = ir.pack + u' ' + ir.folder + u' ' + ir.name;
        if (std::all_of(words.begin(), words.end(), [&text](const QString &w) { return text.contains(w, Qt::CaseInsensitive); }))
            result << std::move(ir);
    }
    return result;
}

std::optional<IrLibrary::Ir> IrLibrary::find(const QString &id) const
{
    if (!m_open || id.isEmpty())
        return std::nullopt;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT %1 FROM irs WHERE id = ?").arg(kColumns));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return fromQuery(q);
    return std::nullopt;
}

QString IrLibrary::filePath(const QString &id) const
{
    if (!find(id))
        return {};
    return QDir(m_directory).filePath(id + QStringLiteral(".wav"));
}

void IrLibrary::setFavourite(const QString &id, bool favourite)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE irs SET favourite = ? WHERE id = ?"));
    q.addBindValue(favourite ? 1 : 0);
    q.addBindValue(id);
    q.exec();
}

bool IrLibrary::remove(const QString &id)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("DELETE FROM irs WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() < 1)
        return false;
    QFile::remove(QDir(m_directory).filePath(id + QStringLiteral(".wav")));
    return true;
}
