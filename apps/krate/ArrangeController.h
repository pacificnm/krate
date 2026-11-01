#pragma once

#include <array>
#include <vector>

#include <QObject>

#include "AudioTracks.h"
#include "SequencerFile.h"
#include "seq/Arrangement.h"
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class SequencerController;
namespace krate {
class AutomationPlayer;
class IInstrument;
class MidiCapture;
class Transport;
}

// QML singleton "Arrange": the arranger (docs/DAW.md, D3). The sequencer's 8
// tracks are its MIDI lanes, holding clips across bars: a pattern clip plays
// that track's part of a pattern, round and round for as long as the clip;
// a MIDI clip has notes of its own. The audio tracks' lanes are the Tracks
// controller's. SONG mode plays the arrangement instead of the patterns; in
// it, play starts at the start bar, the loop range goes round and armed audio
// tracks record only inside the punch range. All of it is kept with the
// sequence (sequencer.json, saved sequences).
//
// Recording MIDI clips: with SONG and REC on, the keyboard's notes while the
// transport plays go into the armed MIDI lanes (inside the punch range, when
// PUNCH is on). When play stops they become a MIDI clip over the bars they
// span, replacing the clips there; with LOOP, each pass adds to the take.
//
// Automation (D5): lanes of points (beat, value) for instruments' knobs,
// played in SONG mode. With AUTO on while playing, a knob someone turns (on
// its panel or a learned MIDI knob) is recorded: its lane stops playing until
// play stops, then the recorded stretch replaces what the lane had there.
class ArrangeController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Arrange)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool songMode READ songMode WRITE setSongMode NOTIFY changed)
    Q_PROPERTY(int startBar READ startBar WRITE setStartBar NOTIFY changed) // 0-based
    Q_PROPERTY(bool loopOn READ loopOn WRITE setLoopOn NOTIFY changed)
    Q_PROPERTY(int loopFrom READ loopFrom NOTIFY changed) // bars [from, to)
    Q_PROPERTY(int loopTo READ loopTo NOTIFY changed)
    Q_PROPERTY(bool punchOn READ punchOn WRITE setPunchOn NOTIFY changed)
    Q_PROPERTY(int punchFrom READ punchFrom NOTIFY changed)
    Q_PROPERTY(int punchTo READ punchTo NOTIFY changed)
    Q_PROPERTY(int endBar READ endBar NOTIFY changed) // after the last clip
    Q_PROPERTY(int maxBars READ maxBars CONSTANT)
    Q_PROPERTY(qreal position READ position NOTIFY positionChanged) // beats: playing, or where play starts
    Q_PROPERTY(bool playing READ playing NOTIFY positionChanged)
    Q_PROPERTY(bool following READ following NOTIFY positionChanged) // a DAW leads over JACK: its position and loop
    Q_PROPERTY(bool recording READ recording NOTIFY changed)          // REC (the transport bar's)
    Q_PROPERTY(int takeNotes READ takeNotes NOTIFY positionChanged)   // notes in the MIDI take so far
    Q_PROPERTY(bool autoWrite READ autoWrite WRITE setAutoWrite NOTIFY changed) // AUTO
    Q_PROPERTY(bool writing READ writing NOTIFY positionChanged) // recording knobs right now
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    ArrangeController(SequencerController &sequencer, krate::Transport &transport, AudioTracksController *tracks,
                      QObject *parent = nullptr);
    ~ArrangeController() override;
    static ArrangeController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    bool songMode() const;
    void setSongMode(bool on);
    int startBar() const;
    void setStartBar(int bar);
    bool loopOn() const;
    void setLoopOn(bool on);
    int loopFrom() const;
    int loopTo() const;
    bool punchOn() const;
    void setPunchOn(bool on);
    int punchFrom() const;
    int punchTo() const;
    int endBar() const;
    int maxBars() const;
    qreal position() const { return m_position; }
    bool recording() const { return m_recording; }
    void setRecording(bool on); // follows REC
    int takeNotes() const { return int(m_take.size()); }
    // Before the host starts: the rack's devices (ids, names, instruments) and
    // the player that plays their automation.
    void setAutomationTargets(krate::AutomationPlayer *player, QStringList ids, QStringList names,
                              std::vector<krate::IInstrument *> instruments);
    bool autoWrite() const { return m_autoWrite; }
    void setAutoWrite(bool on);
    bool writing() const { return m_writing; }
    // Before the host starts: where the keyboard's notes come from.
    void setMidiCapture(krate::MidiCapture *capture) { m_capture = capture; }
    bool playing() const { return m_playing; }
    bool following() const { return m_following; }

    Q_INVOKABLE void setLoop(int from, int to);  // bars [from, to), either way round
    Q_INVOKABLE void setPunch(int from, int to);
    // A MIDI lane's clips: [{startBar, bars, pattern, midi}] in order.
    Q_INVOKABLE QVariantList lane(int track) const;
    Q_INVOKABLE int clipAt(int track, int bar) const; // the clip's index, or -1
    Q_INVOKABLE int patternBars(int pattern) const;   // its length, in whole bars
    // A pattern clip at `bar` for `bars` (0: the pattern's own length),
    // shortened to end before the next clip; false if the bar is taken.
    Q_INVOKABLE bool placePattern(int track, int bar, int pattern, int bars = 0);
    Q_INVOKABLE void removeClip(int track, int index);
    // Editing a MIDI lane's clips (D4), in whole bars; clips on a lane never
    // overlap, so each refuses (false) what would.
    Q_INVOKABLE bool moveClip(int track, int index, int toTrack, int startBar);
    Q_INVOKABLE bool trimClipStart(int track, int index, int startBar); // later into its content, or back
    Q_INVOKABLE bool trimClipEnd(int track, int index, int endBar);
    Q_INVOKABLE bool splitClip(int track, int index, int bar);
    Q_INVOKABLE bool duplicateClip(int track, int index); // right after it
    // Undo and redo every lane edit (MIDI and audio): up to 100 steps, back
    // to when this sequence opened.
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Automation lanes: [{device, deviceName, param, label, points: [{beat, value}]}].
    Q_INVOKABLE QVariantList automationLanes() const;
    // What can be automated: [{device, name}] (devices with knobs), and a device's knobs' labels.
    Q_INVOKABLE QVariantList automatableDevices() const;
    Q_INVOKABLE QStringList paramLabels(int device) const;
    Q_INVOKABLE int addAutomationLane(int device, int param); // its index (an existing one if there is)
    Q_INVOKABLE void removeAutomationLane(int lane);
    Q_INVOKABLE int addPoint(int lane, double beat, double value);  // the point's index
    Q_INVOKABLE int movePoint(int lane, int point, double beat, double value); // its new index
    Q_INVOKABLE void removePoint(int lane, int point);
    Q_INVOKABLE QString valueText(int lane, double value) const; // as the knob shows it

    // A MIDI lane armed for recording (not kept).
    Q_INVOKABLE bool armed(int track) const;
    Q_INVOKABLE void setArmed(int track, bool on);
    // A clip's notes for drawing: [{x, w, pitch}] (beats from its start), the pattern repeated.
    Q_INVOKABLE QVariantList clipNotes(int track, int index) const;

    // The transport's start and loop, the tracks' punch, put back from the
    // sequence (after an export used the transport its own way).
    void reapply() { apply(); }
    // A device's knobs changed (a plugin loaded into a slot): its lanes found again.
    void devicesChanged()
    {
        handOverAutomation();
        ++m_revision;
        emit changed();
    }
    // The beat the song ends on: its last clip, MIDI or audio (at the tempo).
    double songEndBeat(double bpm) const;

    // The screen timer's tick: where the transport is (public for tests).
    void poll();

signals:
    void changed();
    void positionChanged();
    void historyChanged();

private:
    struct TakeNote
    {
        double on, off;
        int pitch, velocity;
    };
    struct Held
    {
        bool down = false;
        double beat = 0.0;
        int velocity = 0;
    };
    struct Snapshot
    {
        krate::seq::Arrangement song;
        AudioTracksController::Lanes audio;
        std::vector<AutomationData> automation;
    };
    Snapshot snapshot() const;
    void restore(const Snapshot &s);
    void beforeEdit(); // the lanes as they are, onto the undo history
    void handOverAutomation(); // the lanes, by device and parameter index, to the player
    int deviceOf(const QString &id) const;
    int paramOf(int device, const QString &key) const;
    void writeTick();      // knobs someone turned, while recording automation
    void finishWriting();  // what was recorded into the lanes
    void takeCaptured(); // the keyboard's notes since the last tick
    void finishTake();   // the notes into a clip on each armed lane
    void apply(); // the transport's start and loop, the tracks' punch, from the state
    void edited(bool arrangement);

    static inline ArrangeController *s_instance = nullptr;
    SequencerController &m_sequencer;
    krate::Transport &m_transport;
    AudioTracksController *m_tracks;
    int m_revision = 0;
    qreal m_position = 0.0;
    bool m_playing = false;
    bool m_following = false;
    bool m_recording = false;
    krate::MidiCapture *m_capture = nullptr;
    std::array<bool, 8> m_armed{};
    bool m_taking = false;
    std::vector<TakeNote> m_take;
    std::array<Held, 128> m_held{};
    double m_lastBeat = 0.0;
    std::vector<Snapshot> m_undo, m_redo;
    // Automation.
    krate::AutomationPlayer *m_player = nullptr;
    QStringList m_deviceIds, m_deviceNames;
    std::vector<krate::IInstrument *> m_instruments;
    std::vector<AutomationData> m_handed; // the lanes the player has
    bool m_autoWrite = false;
    bool m_writing = false;
    std::vector<std::vector<float>> m_seen;        // per device, per knob: as last seen
    struct Recorded
    {
        int device = -1, param = -1;
        std::vector<std::vector<krate::AutomationPoint>> segments; // a new one each time the beat goes back (a loop)
    };
    std::vector<Recorded> m_recorded;
    QTimer m_writeTimer;
    bool m_restoring = false;
    QTimer m_pollTimer;
};
