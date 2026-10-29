#include "SongLibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(lcLibrary, "krate.library")

namespace {

constexpr int kSchemaVersion = 1;

// A name that is safe as a folder or file name.
QString safeName(QString name, const QString &fallback)
{
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral(R"([/\\:*?"<>|\x00-\x1F])")), QStringLiteral("_"));
    while (name.startsWith(u'.'))
        name.remove(0, 1);
    name = name.left(80).trimmed();
    return name.isEmpty() ? fallback : name;
}

// A string to bind as TEXT: a null QString would be SQL NULL.
QString text(const QString &s) { return s.isNull() ? QStringLiteral("") : s; }

QByteArray fileHash(QCryptographicHash &hash, const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
        hash.addData(&f);
    return hash.result();
}

} // namespace

SongLibrary::SongLibrary(const QString &directory)
    : m_directory(QDir(directory).absolutePath())
    , m_connection(QStringLiteral("library-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QDir().mkpath(m_directory);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(QDir(m_directory).filePath(QStringLiteral("library.db")));
    if (!db.open()) {
        m_error = db.lastError().text();
        qCWarning(lcLibrary) << "can't open the library:" << m_error;
        return;
    }
    QSqlQuery(db).exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    m_open = createSchema();
}

SongLibrary::~SongLibrary()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

bool SongLibrary::createSchema()
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA user_version"));
    const int version = q.next() ? q.value(0).toInt() : 0;
    if (version == kSchemaVersion)
        return true;
    if (version > kSchemaVersion) {
        m_error = QStringLiteral("the library was made by a newer Krate");
        return false;
    }
    const QStringList statements{
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS songs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            artist TEXT NOT NULL DEFAULT '',
            album TEXT NOT NULL DEFAULT '',
            title TEXT NOT NULL DEFAULT '',
            track_number INTEGER NOT NULL DEFAULT 0,
            tab_file TEXT NOT NULL DEFAULT '',
            midi_file TEXT NOT NULL DEFAULT '',
            content_hash TEXT NOT NULL UNIQUE,
            added TEXT NOT NULL,
            opened TEXT,
            my_track INTEGER NOT NULL DEFAULT -1,
            sounds_saved TEXT))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS songs_by_artist ON songs (artist, album, track_number, title)"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS song_tracks (
            song_id INTEGER NOT NULL REFERENCES songs (id) ON DELETE CASCADE,
            track INTEGER NOT NULL,
            instrument TEXT NOT NULL DEFAULT '',
            level REAL NOT NULL DEFAULT 1,
            mute INTEGER NOT NULL DEFAULT 0,
            solo INTEGER NOT NULL DEFAULT 0,
            from_midi INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (song_id, track)))"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS song_sounds (
            song_id INTEGER NOT NULL REFERENCES songs (id) ON DELETE CASCADE,
            instrument TEXT NOT NULL,
            state TEXT NOT NULL,
            PRIMARY KEY (song_id, instrument)))"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS song_mixer (
            song_id INTEGER PRIMARY KEY REFERENCES songs (id) ON DELETE CASCADE,
            state TEXT NOT NULL))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion),
    };
    db.transaction();
    for (const QString &sql : statements)
        if (!q.exec(sql)) {
            m_error = q.lastError().text();
            qCWarning(lcLibrary) << "schema:" << m_error;
            db.rollback();
            return false;
        }
    return db.commit();
}

SongLibrary::Details SongLibrary::detailsFromFileName(const QString &path)
{
    QString base = QFileInfo(path).completeBaseName();
    base.remove(QRegularExpression(QStringLiteral(R"(-\d{1,2}-\d{1,2}-\d{2,4}$)"))); // a download date
    Details d;
    const qsizetype dash = base.indexOf(u'-');
    if (dash > 0 && dash < base.size() - 1) {
        d.artist = base.left(dash);
        d.title = base.mid(dash + 1);
    } else {
        d.title = base;
    }
    d.artist.replace(u'_', u' ');
    d.title.replace(u'_', u' ');
    d.artist = d.artist.trimmed();
    d.title = d.title.trimmed();
    return d;
}

QString SongLibrary::copyIn(const QString &source, const Details &details, const QString &extension)
{
    const QString folder = QDir(m_directory).filePath(safeName(details.artist, QStringLiteral("Unknown artist")) + u'/'
                                                      + safeName(details.album, QStringLiteral("Unknown album")));
    QDir().mkpath(folder);
    const QString name = safeName(details.title, QStringLiteral("Song"));
    QString target = QDir(folder).filePath(name + extension);
    for (int n = 2; QFileInfo::exists(target); ++n)
        target = QDir(folder).filePath(QStringLiteral("%1 (%2)%3").arg(name).arg(n).arg(extension));
    if (!QFile::copy(source, target)) {
        m_error = QStringLiteral("can't copy %1 into the library").arg(QFileInfo(source).fileName());
        return {};
    }
    return QDir(m_directory).relativeFilePath(target);
}

std::optional<SongLibrary::Song> SongLibrary::add(const QString &tabPath, const QString &midiPath, const Details &details)
{
    if (!m_open || (tabPath.isEmpty() && midiPath.isEmpty()))
        return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayLiteral("tab:"));
    if (!tabPath.isEmpty())
        fileHash(hash, tabPath);
    hash.addData(QByteArrayLiteral("midi:"));
    if (!midiPath.isEmpty())
        fileHash(hash, midiPath);
    const QString contentHash = QString::fromLatin1(hash.result().toHex());

    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id FROM songs WHERE content_hash = ?"));
    q.addBindValue(contentHash);
    if (q.exec() && q.next())
        return song(q.value(0).toLongLong()); // already here

    const QString tab = tabPath.isEmpty() ? QString() : copyIn(tabPath, details, QStringLiteral(".gp"));
    const QString midi = midiPath.isEmpty() ? QString() : copyIn(midiPath, details, QStringLiteral(".mid"));
    if ((!tabPath.isEmpty() && tab.isEmpty()) || (!midiPath.isEmpty() && midi.isEmpty()))
        return std::nullopt;
    q.prepare(QStringLiteral("INSERT INTO songs (artist, album, title, track_number, tab_file, midi_file, content_hash, added) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(text(details.artist));
    q.addBindValue(text(details.album));
    q.addBindValue(text(details.title));
    q.addBindValue(details.trackNumber);
    q.addBindValue(text(tab));
    q.addBindValue(text(midi));
    q.addBindValue(contentHash);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qCWarning(lcLibrary) << "add:" << m_error;
        return std::nullopt;
    }
    return song(q.lastInsertId().toLongLong());
}

SongLibrary::Song SongLibrary::fromRow(const QSqlQuery &q) const
{
    Song s;
    s.id = q.value(0).toLongLong();
    s.details = {q.value(1).toString(), q.value(2).toString(), q.value(3).toString(), q.value(4).toInt()};
    const QString tab = q.value(5).toString(), midi = q.value(6).toString();
    s.tabFile = tab.isEmpty() ? QString() : QDir(m_directory).filePath(tab);
    s.midiFile = midi.isEmpty() ? QString() : QDir(m_directory).filePath(midi);
    s.added = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
    s.opened = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
    s.soundsSaved = !q.value(9).toString().isEmpty();
    return s;
}

namespace {
const QString kColumns = QStringLiteral("id, artist, album, title, track_number, tab_file, midi_file, added, opened, sounds_saved");
}

std::optional<SongLibrary::Song> SongLibrary::song(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT %1 FROM songs WHERE id = ?").arg(kColumns));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return fromRow(q);
}

QList<SongLibrary::Song> SongLibrary::songs(const QString &search) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    const QString order = QStringLiteral(" ORDER BY artist COLLATE NOCASE, album COLLATE NOCASE, track_number, title COLLATE NOCASE");
    if (search.trimmed().isEmpty()) {
        q.prepare(QStringLiteral("SELECT %1 FROM songs").arg(kColumns) + order);
    } else {
        QString pattern = search.trimmed();
        pattern.replace(u'\\', QStringLiteral("\\\\")).replace(u'%', QStringLiteral("\\%")).replace(u'_', QStringLiteral("\\_"));
        q.prepare(QStringLiteral("SELECT %1 FROM songs WHERE artist LIKE ? ESCAPE '\\' OR album LIKE ? ESCAPE '\\' "
                                 "OR title LIKE ? ESCAPE '\\'").arg(kColumns) + order);
        for (int i = 0; i < 3; ++i)
            q.addBindValue(u'%' + pattern + u'%');
    }
    QList<Song> out;
    if (q.exec())
        while (q.next())
            out << fromRow(q);
    return out;
}

bool SongLibrary::setDetails(qint64 id, const Details &details)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE songs SET artist = ?, album = ?, title = ?, track_number = ? WHERE id = ?"));
    q.addBindValue(text(details.artist.trimmed()));
    q.addBindValue(text(details.album.trimmed()));
    q.addBindValue(text(details.title.trimmed()));
    q.addBindValue(std::max(0, details.trackNumber));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool SongLibrary::remove(qint64 id)
{
    const auto s = song(id);
    if (!s)
        return false;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("DELETE FROM songs WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return false;
    for (const QString &file : {s->tabFile, s->midiFile}) {
        if (file.isEmpty())
            continue;
        QFile::remove(file);
        // Tidy the album and artist folders once they're empty.
        QDir dir = QFileInfo(file).dir();
        for (int up = 0; up < 2 && dir.absolutePath() != m_directory && dir.isEmpty(); ++up) {
            const QString name = dir.dirName();
            dir.cdUp();
            dir.rmdir(name);
        }
    }
    if (lastSong() == id)
        setLastSong(0);
    return true;
}

void SongLibrary::markOpened(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE songs SET opened = ? WHERE id = ?"));
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.addBindValue(id);
    q.exec();
}

void SongLibrary::saveSetup(qint64 id, int myTrack, const QList<TrackSetup> &tracks)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE songs SET my_track = ? WHERE id = ?"));
    q.addBindValue(myTrack);
    q.addBindValue(id);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM song_tracks WHERE song_id = ?"));
    q.addBindValue(id);
    q.exec();
    q.prepare(QStringLiteral("INSERT INTO song_tracks (song_id, track, instrument, level, mute, solo, from_midi) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    for (qsizetype t = 0; t < tracks.size(); ++t) {
        const TrackSetup &st = tracks[t];
        q.addBindValue(id);
        q.addBindValue(int(t));
        q.addBindValue(text(st.instrument));
        q.addBindValue(st.level);
        q.addBindValue(st.mute ? 1 : 0);
        q.addBindValue(st.solo ? 1 : 0);
        q.addBindValue(st.fromMidi ? 1 : 0);
        q.exec();
    }
    db.commit();
}

int SongLibrary::myTrack(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT my_track FROM songs WHERE id = ?"));
    q.addBindValue(id);
    return q.exec() && q.next() ? q.value(0).toInt() : -1;
}

QList<SongLibrary::TrackSetup> SongLibrary::setup(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT track, instrument, level, mute, solo, from_midi FROM song_tracks WHERE song_id = ? ORDER BY track"));
    q.addBindValue(id);
    QList<TrackSetup> out;
    if (q.exec())
        while (q.next()) {
            const int track = q.value(0).toInt();
            if (track < 0 || track > 1024)
                continue;
            while (out.size() < track)
                out << TrackSetup{};
            out << TrackSetup{q.value(1).toString(), q.value(2).toDouble(), q.value(3).toBool(), q.value(4).toBool(),
                              q.value(5).toBool()};
        }
    return out;
}

void SongLibrary::saveSounds(qint64 id, const QJsonObject &sounds, const QJsonObject &mixer)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM song_sounds WHERE song_id = ?"));
    q.addBindValue(id);
    q.exec();
    q.prepare(QStringLiteral("INSERT INTO song_sounds (song_id, instrument, state) VALUES (?, ?, ?)"));
    for (auto it = sounds.begin(); it != sounds.end(); ++it) {
        q.addBindValue(id);
        q.addBindValue(it.key());
        q.addBindValue(QString::fromUtf8(QJsonDocument(it.value().toObject()).toJson(QJsonDocument::Compact)));
        q.exec();
    }
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO song_mixer (song_id, state) VALUES (?, ?)"));
    q.addBindValue(id);
    q.addBindValue(QString::fromUtf8(QJsonDocument(mixer).toJson(QJsonDocument::Compact)));
    q.exec();
    q.prepare(QStringLiteral("UPDATE songs SET sounds_saved = ? WHERE id = ?"));
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.addBindValue(id);
    q.exec();
    db.commit();
}

QJsonObject SongLibrary::sounds(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT instrument, state FROM song_sounds WHERE song_id = ?"));
    q.addBindValue(id);
    QJsonObject out;
    if (q.exec())
        while (q.next())
            out[q.value(0).toString()] = QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
    return out;
}

QJsonObject SongLibrary::mixer(qint64 id) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT state FROM song_mixer WHERE song_id = ?"));
    q.addBindValue(id);
    return q.exec() && q.next() ? QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object() : QJsonObject();
}

QString SongLibrary::setting(const QString &key) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

void SongLibrary::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

void SongLibrary::setLastSong(qint64 id) { setSetting(QStringLiteral("last_song"), QString::number(id)); }

qint64 SongLibrary::lastSong() const { return setting(QStringLiteral("last_song")).toLongLong(); }
