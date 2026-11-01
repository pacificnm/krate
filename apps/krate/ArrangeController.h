#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class SequencerController;
class AudioTracksController;
namespace krate {
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
    // A clip's notes for drawing: [{x, w, pitch}] (beats from its start), the pattern repeated.
    Q_INVOKABLE QVariantList clipNotes(int track, int index) const;

    // The screen timer's tick: where the transport is (public for tests).
    void poll();

signals:
    void changed();
    void positionChanged();

private:
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
    QTimer m_pollTimer;
};
