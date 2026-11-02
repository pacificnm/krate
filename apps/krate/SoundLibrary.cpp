#include "SoundLibrary.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {
constexpr int kSchemaVersion = 2; // 2: fingerprint
}

SoundLibrary::SoundLibrary(const QString &file)
    : m_connection(QStringLiteral("sounds-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(file);
    if (!db.open())
        return;
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA user_version"));
    const int version = q.next() ? q.value(0).toInt() : 0;
    if (version > kSchemaVersion)
        return; // made by a newer Krate
    m_open = q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS folders (path TEXT PRIMARY KEY)"))
             && q.exec(QStringLiteral(R"(CREATE TABLE IF NOT EXISTS sounds (
                    id INTEGER PRIMARY KEY,
                    path TEXT UNIQUE NOT NULL,
                    folder TEXT NOT NULL,
                    name TEXT NOT NULL,
                    dir TEXT NOT NULL,
                    category INTEGER NOT NULL,
                    manual INTEGER NOT NULL DEFAULT 0,
                    favorite INTEGER NOT NULL DEFAULT 0,
                    seconds REAL NOT NULL DEFAULT 0,
                    rate INTEGER NOT NULL DEFAULT 0,
                    channels INTEGER NOT NULL DEFAULT 0,
                    fingerprint TEXT NOT NULL DEFAULT ''))"))
             && (version != 1 || q.exec(QStringLiteral("ALTER TABLE sounds ADD COLUMN fingerprint TEXT NOT NULL DEFAULT ''")))
             && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS sounds_category ON sounds(category)"))
             && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS sounds_fingerprint ON sounds(fingerprint)"))
             && q.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
}

SoundLibrary::~SoundLibrary()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

QStringList SoundLibrary::folders() const
{
    QStringList list;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT path FROM folders ORDER BY path"));
    while (q.next())
        list << q.value(0).toString();
    return list;
}

bool SoundLibrary::addFolder(const QString &folder)
{
    const QString path = QDir(folder).absolutePath();
    for (const QString &f : folders())
        if (path == f || path.startsWith(f + u'/'))
            return false;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT INTO folders (path) VALUES (?)"));
    q.addBindValue(path);
    return q.exec();
}

void SoundLibrary::removeFolder(const QString &folder)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM sounds WHERE folder = ?"));
    q.addBindValue(folder);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM folders WHERE path = ?"));
    q.addBindValue(folder);
    q.exec();
}

int SoundLibrary::update(const QString &folder, const std::vector<sounds::FileInfo> &files)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    QSqlQuery insert(db), seconds(db), drop(db), existing(db);
    insert.prepare(QStringLiteral("INSERT OR IGNORE INTO sounds (path, folder, name, dir, category, seconds, rate, channels, fingerprint) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery contents(db); // a file changed (or indexed before fingerprints)
    contents.prepare(QStringLiteral("UPDATE sounds SET fingerprint = ? WHERE path = ?"));
    seconds.prepare(QStringLiteral("UPDATE sounds SET seconds = ?, rate = ?, channels = ? WHERE path = ? AND ? > 0"));
    QSet<QString> found;
    const QDir root(folder);
    for (const sounds::FileInfo &f : files) {
        found.insert(f.path);
        const QString relative = root.relativeFilePath(f.path);
        // "" (not a null string, which is SQL's NULL: refused by the column) for the folder's own files.
        const QString dir = QFileInfo(relative).path() == u"." ? QStringLiteral("") : QFileInfo(relative).path();
        insert.addBindValue(f.path);
        insert.addBindValue(folder);
        insert.addBindValue(QFileInfo(f.path).completeBaseName());
        insert.addBindValue(dir);
        insert.addBindValue(int(sounds::guessCategory(relative)));
        insert.addBindValue(f.seconds);
        insert.addBindValue(f.rate);
        insert.addBindValue(f.channels);
        insert.addBindValue(f.fingerprint);
        insert.exec();
        contents.addBindValue(f.fingerprint);
        contents.addBindValue(f.path);
        contents.exec();
        seconds.addBindValue(f.seconds);
        seconds.addBindValue(f.rate);
        seconds.addBindValue(f.channels);
        seconds.addBindValue(f.path);
        seconds.addBindValue(f.seconds);
        seconds.exec();
    }
    // Gone from the folder: gone from the library.
    existing.prepare(QStringLiteral("SELECT path FROM sounds WHERE folder = ?"));
    existing.addBindValue(folder);
    existing.exec();
    QStringList gone;
    while (existing.next())
        if (!found.contains(existing.value(0).toString()))
            gone << existing.value(0).toString();
    drop.prepare(QStringLiteral("DELETE FROM sounds WHERE path = ?"));
    for (const QString &path : gone) {
        drop.addBindValue(path);
        drop.exec();
    }
    db.commit();
    QSqlQuery count(db);
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM sounds WHERE folder = ?"));
    count.addBindValue(folder);
    count.exec();
    return count.next() ? count.value(0).toInt() : 0;
}

namespace {
QVariantMap row(const QSqlQuery &q)
{
    return {{QStringLiteral("id"), q.value(0)},       {QStringLiteral("name"), q.value(1)},
            {QStringLiteral("dir"), q.value(2)},      {QStringLiteral("path"), q.value(3)},
            {QStringLiteral("category"), q.value(4)}, {QStringLiteral("favorite"), q.value(5).toBool()},
            {QStringLiteral("seconds"), q.value(6)}};
}
const QString kColumns = QStringLiteral("SELECT id, name, dir, path, category, favorite, seconds FROM sounds");
// One sound however many copies of it there are (the same file in your folder and
// in Klang's samples, sent to a pad): the first indexed.
const QString kOnce = QStringLiteral(
    "(fingerprint = '' OR id = (SELECT MIN(s2.id) FROM sounds s2 WHERE s2.fingerprint = sounds.fingerprint))");
} // namespace

QVariantList SoundLibrary::sounds(const Query &query) const
{
    QString sql = kColumns + QStringLiteral(" WHERE 1");
    if (query.folder.isEmpty())
        sql += QStringLiteral(" AND ") + kOnce;
    if (query.category >= 0)
        sql += QStringLiteral(" AND category = :category");
    if (query.favourites)
        sql += QStringLiteral(" AND favorite = 1");
    if (!query.search.trimmed().isEmpty())
        sql += QStringLiteral(" AND (name LIKE :search OR dir LIKE :search)");
    if (!query.folder.isEmpty())
        sql += QStringLiteral(" AND folder = :folder");
    sql += QStringLiteral(" ORDER BY name COLLATE NOCASE, dir COLLATE NOCASE LIMIT :limit");
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(sql);
    if (query.category >= 0)
        q.bindValue(QStringLiteral(":category"), query.category);
    if (!query.search.trimmed().isEmpty())
        q.bindValue(QStringLiteral(":search"), u'%' + query.search.trimmed() + u'%');
    if (!query.folder.isEmpty())
        q.bindValue(QStringLiteral(":folder"), query.folder);
    q.bindValue(QStringLiteral(":limit"), query.limit);
    q.exec();
    QVariantList list;
    while (q.next())
        list.append(row(q));
    return list;
}

QVariantMap SoundLibrary::sound(int id) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(kColumns + QStringLiteral(" WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
    return q.next() ? row(q) : QVariantMap{};
}

std::vector<int> SoundLibrary::categoryCounts() const
{
    std::vector<int> counts(std::size_t(sounds::Category::Count), 0);
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT category, COUNT(*) FROM sounds WHERE ") + kOnce + QStringLiteral(" GROUP BY category"));
    while (q.next())
        if (const int c = q.value(0).toInt(); c >= 0 && c < int(counts.size()))
            counts[std::size_t(c)] = q.value(1).toInt();
    return counts;
}

int SoundLibrary::favouriteCount() const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT COUNT(*) FROM sounds WHERE favorite = 1 AND ") + kOnce);
    return q.next() ? q.value(0).toInt() : 0;
}

int SoundLibrary::total() const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT COUNT(*) FROM sounds WHERE ") + kOnce);
    return q.next() ? q.value(0).toInt() : 0;
}

void SoundLibrary::setCategory(int id, int category)
{
    if (category < 0 || category >= int(sounds::Category::Count))
        return;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE sounds SET category = ?, manual = 1 WHERE id = ?"));
    q.addBindValue(category);
    q.addBindValue(id);
    q.exec();
}

void SoundLibrary::setFavourite(int id, bool favourite)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE sounds SET favorite = ? WHERE id = ?"));
    q.addBindValue(favourite ? 1 : 0);
    q.addBindValue(id);
    q.exec();
}

void SoundLibrary::setSeconds(int id, double seconds)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE sounds SET seconds = ? WHERE id = ?"));
    q.addBindValue(seconds);
    q.addBindValue(id);
    q.exec();
}
