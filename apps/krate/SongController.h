#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "SongLibrary.h"
#include "app/InstrumentRegistry.h"
#include "song/Score.h"
#include "song/Song.h"
#include "song/SongPlayer.h"

class QQmlEngine;
class QJSEngine;
class RackController;
namespace krate {
class IInstrument;
class InstrumentInstance;
}

// The rack's Song screen (docs/SONG_PLAYER.md): opens a .gp tab file and/or a
// .mid file of the same song, lists its tracks, assigns each to one of the
// rack's instruments (or none), with level, mute, solo, which file it plays
// from, and "my track" (the one you play); shows the position. Every song
// opened goes into the song library (SongLibrary), which keeps its files and
// its setup; the last song reopens with the rack. SAVE keeps every
// instrument's sound and the mixer with the song; opening the song puts them
// on the rack, and closing it (or opening another, or quitting) puts back
// what was there before.
// While a song is loaded the instruments' own sequencers are off, so only the
// song plays them. QML singleton "Song".
class SongController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Song)
    QML_SINGLETON

    Q_PROPERTY(bool loaded READ loaded NOTIFY songChanged)
    Q_PROPERTY(QString title READ title NOTIFY songChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY songChanged)
    Q_PROPERTY(QString album READ album NOTIFY songChanged)
    Q_PROPERTY(int trackNumber READ trackNumber NOTIFY songChanged)
    Q_PROPERTY(qint64 songId READ songId NOTIFY songChanged) // in the library, 0 = not in it
    Q_PROPERTY(bool soundsSaved READ soundsSaved NOTIFY songChanged) // SAVE has kept sounds with this song
    Q_PROPERTY(int libraryRevision READ libraryRevision NOTIFY libraryChanged) // bumped when the library changes
    Q_PROPERTY(QString files READ files NOTIFY songChanged) // "name.gp + name.mid"
    Q_PROPERTY(bool hasTab READ hasTab NOTIFY songChanged)
    Q_PROPERTY(bool hasMidi READ hasMidi NOTIFY songChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList instrumentChoices READ instrumentChoices CONSTANT) // index 0 = none
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(int myTrack READ myTrack WRITE setMyTrack NOTIFY tracksChanged)
    Q_PROPERTY(int bar READ bar NOTIFY positionChanged)        // 1-based
    Q_PROPERTY(int beat READ beat NOTIFY positionChanged)      // 1-based
    Q_PROPERTY(int barCount READ barCount NOTIFY songChanged)
    Q_PROPERTY(QString time READ time NOTIFY positionChanged)  // "1:23 / 5:34"
    Q_PROPERTY(qint64 tick READ tick NOTIFY tickChanged)       // the song position, in the score's ticks as played

public:
    // An instrument a track can play on.
    struct Choice
    {
        QString label; // "Krimson B"
        QString key;   // "krimson:B" (stable, for songs.json)
        int device = -1;
        int channel = 0;
        bool drumMap = false;
    };

    SongController(krate::song::SongPlayer &player, RackController &rack, const QList<krate::InstrumentEntry> &entries,
                   std::vector<krate::InstrumentInstance *> instances, SongLibrary &library, QObject *parent);
    ~SongController() override;

    static SongController *create(QQmlEngine *, QJSEngine *);

    bool loaded() const { return m_song.has_value(); }
    QString title() const;
    QString artist() const;
    QString album() const { return m_details.album; }
    int trackNumber() const { return m_details.trackNumber; }
    qint64 songId() const { return m_songId; }
    bool soundsSaved() const { return m_soundsSaved; }
    int libraryRevision() const { return m_libraryRevision; }
    QString files() const;
    bool hasTab() const { return m_tab.has_value(); }
    bool hasMidi() const { return m_midi.has_value(); }
    QString error() const { return m_error; }
    QStringList instrumentChoices() const;
    QVariantList tracks() const;
    int myTrack() const { return m_myTrack; }
    void setMyTrack(int track);
    int bar() const { return m_bar; }
    int beat() const { return m_beat; }
    int barCount() const;
    QString time() const { return m_time; }
    qint64 tick() const { return m_tick; }

    // Files from the dialog or a drop; a lone .gp or .mid also brings its
    // namesake of the other kind from the same folder.
    Q_INVOKABLE void open(const QList<QUrl> &urls);
    Q_INVOKABLE void openPaths(const QStringList &paths);
    Q_INVOKABLE void close();
    // A song from the library.
    Q_INVOKABLE void openSong(qint64 id);
    // The open song's artist, album, title and track number (in the library).
    Q_INVOKABLE void setDetails(const QString &artist, const QString &album, const QString &title, int trackNumber);
    // Keep every instrument's sound and the mixer with the open song.
    Q_INVOKABLE void saveSounds();
    // The library for its list, as rows in order: {kind: "artist"|"album"|"song",
    // text, and for songs id, track, soundsSaved, open}; `search` matches
    // artist, album or title.
    Q_INVOKABLE QVariantList libraryRows(const QString &search) const;
    // Removes a song and its copied files (closing it first if it's open).
    Q_INVOKABLE void removeFromLibrary(qint64 id);
    Q_INVOKABLE void setInstrument(int track, int choice);
    Q_INVOKABLE void setLevel(int track, qreal level);
    Q_INVOKABLE void setMute(int track, bool mute);
    Q_INVOKABLE void setSolo(int track, bool solo);
    Q_INVOKABLE void setFromMidi(int track, bool fromMidi); // play this track from the .mid file
    Q_INVOKABLE void backToStart();
    // Jump to a written bar (from the tab view): its next time through from
    // where the song is, else its first.
    Q_INVOKABLE void jumpToBar(int writtenBar);

    // For the session and tests.
    void reopenLast();
    // Once: S4's songs.json (its last song and that song's track setup) into
    // the library; the file is then renamed to songs.json.old.
    void importSongsJson(const QString &path);
    const std::vector<Choice> &choices() const { return m_choices; }
    const krate::song::Song *song() const { return m_song ? &*m_song : nullptr; }
    const krate::song::Score *score() const { return m_score ? &*m_score : nullptr; }
    void pollPosition(); // the timer's tick (public for tests)

signals:
    void songChanged();
    void tracksChanged();
    void positionChanged();
    void tickChanged();
    void errorChanged();
    void libraryChanged();

private:
    struct TrackSettings
    {
        int choice = 0; // into m_choices + 1; 0 = none
        double level = 1.0;
        bool mute = false;
        bool solo = false;
        bool fromMidi = false;
    };

    void setError(const QString &error);
    bool lineUp(int track) const; // the .mid file has a matching track
    void rebuild();               // the playable song from the chosen sources, to the player
    void applyTrack(int track);
    void suggest();               // defaults for a song opened for the first time
    bool load(const QString &tabPath, const QString &midiPath, qint64 songId, const SongLibrary::Details &details);
    void loadSettings();
    void saveSettings() const;
    void setSequencers(bool enabled);

    // Every instrument's sound (by id) and the mixer.
    struct Snapshot
    {
        QJsonObject sounds, mixer;
    };
    Snapshot capture() const;
    void restore(const Snapshot &snapshot);

    static inline SongController *s_instance = nullptr;

    krate::song::SongPlayer &m_player;
    RackController &m_rack;
    std::vector<krate::InstrumentInstance *> m_instances; // in the entries' order
    QStringList m_ids;
    std::vector<Choice> m_choices;
    SongLibrary &m_library;
    qint64 m_songId = 0;
    SongLibrary::Details m_details;
    bool m_soundsSaved = false;
    int m_libraryRevision = 0;
    std::optional<Snapshot> m_before; // the sounds and mixer from before the open song

    std::optional<krate::song::Score> m_score;  // from the tab file
    std::optional<krate::song::Song> m_tab;     // its playable song
    std::optional<krate::song::Song> m_midi;    // from the MIDI file
    std::optional<krate::song::Song> m_song;    // what plays (the GUI's copy)
    QString m_tabPath, m_midiPath, m_error;
    std::vector<TrackSettings> m_tracks;
    int m_myTrack = -1;

    QTimer m_timer;
    int m_bar = 1, m_beat = 1;
    qint64 m_tick = 0;
    QString m_time;
};
