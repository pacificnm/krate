#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "app/InstrumentRegistry.h"
#include "song/Score.h"
#include "song/Song.h"
#include "song/SongPlayer.h"

class QQmlEngine;
class QJSEngine;
class RackController;
namespace krate {
class IInstrument;
}

// The rack's Song screen (docs/SONG_PLAYER.md): opens a .gp tab file and/or a
// .mid file of the same song, lists its tracks, assigns each to one of the
// rack's instruments (or none), with level, mute, solo, which file it plays
// from, and "my track" (the one you play); shows the position. Settings are
// remembered per song in songs.json, and the last song reopens with the rack.
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
                   std::vector<krate::IInstrument *> instruments, const QString &settingsPath, QObject *parent);
    ~SongController() override;

    static SongController *create(QQmlEngine *, QJSEngine *);

    bool loaded() const { return m_song.has_value(); }
    QString title() const;
    QString artist() const;
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

    // Files from the dialog or a drop; a lone .gp or .mid also brings its
    // namesake of the other kind from the same folder.
    Q_INVOKABLE void open(const QList<QUrl> &urls);
    Q_INVOKABLE void openPaths(const QStringList &paths);
    Q_INVOKABLE void close();
    Q_INVOKABLE void setInstrument(int track, int choice);
    Q_INVOKABLE void setLevel(int track, qreal level);
    Q_INVOKABLE void setMute(int track, bool mute);
    Q_INVOKABLE void setSolo(int track, bool solo);
    Q_INVOKABLE void setFromMidi(int track, bool fromMidi); // play this track from the .mid file
    Q_INVOKABLE void backToStart();

    // For the session and tests.
    void reopenLast();
    const std::vector<Choice> &choices() const { return m_choices; }
    const krate::song::Song *song() const { return m_song ? &*m_song : nullptr; }
    void pollPosition(); // the timer's tick (public for tests)

signals:
    void songChanged();
    void tracksChanged();
    void positionChanged();
    void errorChanged();

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
    QString songKey() const;
    void loadSettings();
    void saveSettings() const;
    void setSequencers(bool enabled);

    static inline SongController *s_instance = nullptr;

    krate::song::SongPlayer &m_player;
    RackController &m_rack;
    std::vector<krate::IInstrument *> m_instruments;
    std::vector<Choice> m_choices;
    QString m_settingsPath;

    std::optional<krate::song::Score> m_score;  // from the tab file
    std::optional<krate::song::Song> m_tab;     // its playable song
    std::optional<krate::song::Song> m_midi;    // from the MIDI file
    std::optional<krate::song::Song> m_song;    // what plays (the GUI's copy)
    QString m_tabPath, m_midiPath, m_error;
    std::vector<TrackSettings> m_tracks;
    int m_myTrack = -1;

    QTimer m_timer;
    int m_bar = 1, m_beat = 1;
    QString m_time;
};
