#pragma once

#include <optional>

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

// The song library (docs/SONG_PLAYER.md#library): one SQLite database in the
// library folder, next to copies of every song's files, filed by artist and
// album. Each song keeps its details, its tracks' setup, and (L2) the
// instruments' sounds and the mixer saved with it. GUI thread.
class SongLibrary
{
public:
    struct Details
    {
        QString artist, album, title;
        int trackNumber = 0;
    };

    struct Song
    {
        qint64 id = 0;
        Details details;
        QString tabFile, midiFile; // absolute paths of the library's copies ("" = none)
        QDateTime added, opened;
        bool soundsSaved = false;
        QString backingFile;       // the backing track's copy ("" = none)
        double backingOffset = 0.0; // where bar 1 is in it, seconds
    };

    struct TrackSetup
    {
        QString instrument; // a choice key ("k303", "krimson:B"), "" = none
        double level = 1.0;
        bool mute = false;
        bool solo = false;
        bool fromMidi = false;
    };

    // Opens (creating if needed) <directory>/library.db.
    explicit SongLibrary(const QString &directory);
    ~SongLibrary();
    SongLibrary(const SongLibrary &) = delete;
    SongLibrary &operator=(const SongLibrary &) = delete;

    bool isOpen() const { return m_open; }
    QString directory() const { return m_directory; }
    QString lastError() const { return m_error; }

    // Copies the files in and adds the song, or finds the song already holding
    // these files (by content). Either path may be empty, not both.
    std::optional<Song> add(const QString &tabPath, const QString &midiPath, const Details &details);
    std::optional<Song> song(qint64 id) const;
    // By artist, album, track number and title; `search` matches any of artist, album, title.
    QList<Song> songs(const QString &search = {}) const;
    bool setDetails(qint64 id, const Details &details);
    bool remove(qint64 id); // and its copied files
    void markOpened(qint64 id);

    // The track setup ("my track" too).
    void saveSetup(qint64 id, int myTrack, const QList<TrackSetup> &tracks);
    int myTrack(qint64 id) const; // -1 if none
    QList<TrackSetup> setup(qint64 id) const;

    // Sounds (instrument id -> its sound) and the mixer, saved together.
    void saveSounds(qint64 id, const QJsonObject &sounds, const QJsonObject &mixer);

    // The song's backing track (docs/SONG_PLAYER.md#backing-track): a copy of
    // the file in the song's folder (replacing any earlier one) and where bar 1
    // is in it. Returns the copy's path ("" on failure, see error()).
    QString setBacking(qint64 id, const QString &source);
    void setBackingOffset(qint64 id, double seconds);
    void clearBacking(qint64 id); // deletes the copy
    QJsonObject sounds(qint64 id) const;
    QJsonObject mixer(qint64 id) const;

    // Saved sequences (docs/SEQUENCER.md#saved-sequences): the sequencer's
    // file, the instruments' sounds and the mixer, by name.
    struct Sequence
    {
        qint64 id = 0;
        QString name;
        QDateTime saved;
        QJsonObject data, sounds, mixer; // empty in sequences()
    };
    // Saves a new one (id 0) or over one; returns its id (0 on failure).
    qint64 saveSequence(qint64 id, const QString &name, const QJsonObject &data, const QJsonObject &sounds,
                        const QJsonObject &mixer);
    QList<Sequence> sequences() const; // by name, without their contents
    std::optional<Sequence> sequence(qint64 id) const;
    bool renameSequence(qint64 id, const QString &name);
    bool removeSequence(qint64 id);

    // The song open when the rack last closed (0 = none).
    void setLastSong(qint64 id);
    qint64 lastSong() const;

    // Artist and title from a file name like "Artist-Song_Title-10-24-2026.mid".
    static Details detailsFromFileName(const QString &path);

private:
    bool createSchema();
    QString copyIn(const QString &source, const Details &details, const QString &extension);
    Song fromRow(const class QSqlQuery &q) const;
    QString setting(const QString &key) const;
    void setSetting(const QString &key, const QString &value);

    QString m_directory;
    QString m_connection;
    bool m_open = false;
    mutable QString m_error;
};
