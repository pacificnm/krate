#pragma once

#include <array>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "app/InstrumentRegistry.h"
#include "rec/AudioTrack.h"
#include "rec/Recorder.h"

class QQmlEngine;
class QJSEngine;
class RackController;

// QML singleton "Tracks": Krate's audio tracks (docs/DAW.md, D2). Eight
// tracks, each a rack device (added from +, like an instrument): it records
// its input (the interface, or another device in stereo) into takes, WAV
// files in ~/.local/share/krate/recordings/audioN/. REC (the transport bar)
// arms recording: armed tracks record while playing.
//
// Each track has its takes and its lane (docs/DAW.md, D3): clips, takes
// placed at a beat, a later one on top. A new take goes on the lane where it
// was recorded; the panel takes one off or puts it back. The disk thread
// (Recorder) writes the takes; those on the lane (and the one selected, for
// its waveform) are loaded off the GUI thread and handed to the track as a
// playlist. A take from the interface skips its round trip (what you played
// reached Krate that late); one from another device doesn't. Takes, clips
// and knobs are kept in recordings/tracks.json.
class AudioTracksController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Tracks)
    QML_SINGLETON

public:
    static constexpr int kTracks = 8;
    // A take placed on a lane (D3), trimmed and faded (D4); frames are the take's.
    struct LaneClip
    {
        QString file; // the take's
        double startBeat = 0.0;
        qint64 trim = 0;   // frames off its start (past the round trip)
        qint64 length = 0; // frames it plays (0: to the take's end)
        qint64 fadeIn = 0, fadeOut = 0;
        bool operator==(const LaneClip &) const = default;
    };
    using Lanes = std::array<std::vector<LaneClip>, kTracks>;

    Q_PROPERTY(bool recording READ recording WRITE setRecording NOTIFY recordingChanged) // REC
    Q_PROPERTY(int revision READ revision NOTIFY changed)  // takes or the active one changed
    Q_PROPERTY(int frame READ frame NOTIFY frameChanged)   // ~20 Hz: meters, recording state

    explicit AudioTracksController(const QString &dir, QObject *parent = nullptr);
    ~AudioTracksController() override;
    static AudioTracksController *create(QQmlEngine *, QJSEngine *);

    krate::AudioTrack &engine(int track) { return *m_tracks[std::size_t(track)].engine; }
    // The rack device each track is, and the rack (to know a track's input source).
    void setDevice(int track, int rackDevice) { m_tracks[std::size_t(track)].device = rackDevice; }
    void setRack(RackController *rack) { m_rack = rack; }
    // The interface's round trip in frames (the host's), for a take from its inputs.
    void setRoundTrip(std::function<int()> roundTrip) { m_roundTrip = std::move(roundTrip); }

    bool recording() const { return m_recording; }
    void setRecording(bool on);
    // The arranger's punch range (bars [from, to)): armed tracks record only inside it.
    void setPunch(bool on, int fromBar, int toBar)
    {
        for (Track &t : m_tracks)
            t.engine->setPunch(on, fromBar, toBar);
    }
    int revision() const { return m_revision; }
    int frame() const { return m_frame; }

    Q_INVOKABLE int trackOf(int rackDevice) const; // -1: not a track
    Q_INVOKABLE QVariantList takes(int track) const; // [{name, seconds, mono, onLane, selected}]
    Q_INVOKABLE int selectedTake(int track) const;
    Q_INVOKABLE void selectTake(int track, int index);    // the panel's waveform
    Q_INVOKABLE bool onLane(int track, int index) const;
    Q_INVOKABLE void setOnLane(int track, int index, bool on); // at the beat it was recorded on
    Q_INVOKABLE void deleteTake(int track, int index);    // its clips and its file
    Q_INVOKABLE QVariantList peaks(int track) const;      // the selected take's waveform: 0..1 each
    // The lane, for the arranger: [{take, name, startBeat, seconds (as it
    // plays), from / to (0..1 of the take shown), fadeIn, fadeOut (seconds),
    // loaded}], later on top.
    Q_INVOKABLE QVariantList clips(int track) const;
    // Editing a lane's clips (D4); beats at `bpm`. A clip moved or made comes
    // out on top (the lane's last); each returns its new index (-1: no change).
    Q_INVOKABLE int moveClip(int track, int clip, double startBeat);
    Q_INVOKABLE void trimClipStart(int track, int clip, double startBeat, double bpm);
    Q_INVOKABLE void trimClipEnd(int track, int clip, double endBeat, double bpm);
    Q_INVOKABLE int splitClip(int track, int clip, double beat, double bpm); // the second half's index
    Q_INVOKABLE int duplicateClip(int track, int clip, double bpm);          // right after it
    Q_INVOKABLE void setClipFades(int track, int clip, double fadeInSeconds, double fadeOutSeconds);

    // Undo (the arranger, D4): every lane as it is, and put back as it was;
    // `before` is told before any lane changes (a recording, the panel, an edit).
    Lanes lanes() const;
    void setLanes(const Lanes &lanes);
    void setBeforeLaneEdit(std::function<void()> before) { m_beforeLaneEdit = std::move(before); }
    Q_INVOKABLE QVariantList takePeaks(int track, int index) const; // [] until loaded
    Q_INVOKABLE void removeClip(int track, int clip);
    Q_INVOKABLE bool isRecording(int track) const;
    Q_INVOKABLE qreal inputLevel(int track) const;        // 0..1 (-60..0 dB)
    // The track's knobs: Arm, Monitor, Input (krate::AudioTrack::Param).
    Q_INVOKABLE qreal param(int track, int index) const;
    Q_INVOKABLE void setParam(int track, int index, qreal value);
    Q_INVOKABLE QString paramText(int track, int index) const;

    QString directory() const { return m_dir; }
    void save();
    // Quitting, before the rack and the host go: a take still recording is
    // finished and kept. Nothing records after this.
    void finish();

signals:
    void recordingChanged();
    void changed();
    void frameChanged();

private:
    struct TakeInfo
    {
        QString file; // in the track's folder
        double startBeat = 0.0;
        qint64 offset = 0;
        int rate = 48000;
        qint64 frames = 0;
        int channels = 2;
    };
    struct Loaded
    {
        std::unique_ptr<krate::Take> take;
        std::vector<float> peaks;
    };
    // A playlist the engine let go of, with the takes only it used.
    struct Retired
    {
        std::unique_ptr<krate::Playlist> playlist;
        std::vector<std::unique_ptr<krate::Take>> takes;
        std::uint64_t block = 0; // freed once the engine is two blocks on
    };
    struct Track
    {
        std::unique_ptr<krate::AudioTrack> engine;
        std::vector<TakeInfo> takes;
        std::vector<LaneClip> clips;
        int selected = -1;
        int device = -1;
        std::map<QString, Loaded> cache;             // loaded takes, by file
        std::unique_ptr<krate::Playlist> playlist;   // handed to the engine
        std::vector<Retired> retired;
        std::future<Loaded> loading;
        QString loadingFile; // the take being loaded
        qreal level = 0.0;
    };

    QString trackDir(int track) const;
    void beforeEdit() { if (m_beforeLaneEdit) m_beforeLaneEdit(); }
    qint64 playable(const Track &t, const LaneClip &c) const; // frames it plays
    void laneEdited(int track); // play it, save, tell the screen
    void load();
    void poll();
    int takeIndex(const Track &t, const QString &file) const;
    void refresh(int track); // loads what the lane needs, hands the engine its playlist
    void addFinished(const krate::Recorder::Finished &f);
    void bump();

    static inline AudioTracksController *s_instance = nullptr;

    QString m_dir;
    std::array<Track, kTracks> m_tracks;
    std::unique_ptr<krate::Recorder> m_recorder;
    RackController *m_rack = nullptr;
    std::function<int()> m_roundTrip = [] { return 0; };
    std::function<void()> m_beforeLaneEdit;
    bool m_recording = false;
    bool m_finished = false, m_stopping = false;
    int m_revision = 0, m_frame = 0;
    QTimer m_pollTimer;
};

// The tracks' registry entries: "audio1".."audio8", each a rack device with
// its panel (AudioTrackPanel), in the rack once added from +.
QList<krate::InstrumentEntry> audioTrackEntries(AudioTracksController &tracks);
