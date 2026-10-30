#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "NoteTargets.h"
#include "SequencerFile.h"
#include "seq/Sequencer.h"

class QQmlEngine;
class QJSEngine;

// QML singleton "Sequencer": the SEQ screen's view of the rack sequencer
// (docs/SEQUENCER.md). Owns the GUI copy of the bank; every edit hands the
// player a fresh copy and saves sequencer.json a moment later. The pattern
// shown is the one cued: while playing it plays from the end of the current
// one. Notes are in ticks (96 per beat). GUI thread.
class SequencerController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sequencer)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int pattern READ pattern WRITE setPattern NOTIFY changed)
    Q_PROPERTY(int track READ track WRITE setTrack NOTIFY changed)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY changed)
    Q_PROPERTY(int length READ length NOTIFY changed)       // ticks
    Q_PROPERTY(int grid READ grid WRITE setGrid NOTIFY changed)
    Q_PROPERTY(int gridTicks READ gridTicks NOTIFY changed)
    Q_PROPERTY(int ticksPerBeat READ ticksPerBeat CONSTANT)
    Q_PROPERTY(int maxTicks READ maxTicks CONSTANT)
    Q_PROPERTY(QStringList gridNames READ gridNames CONSTANT)
    Q_PROPERTY(QStringList targetChoices READ targetChoices CONSTANT) // "None", then the instruments
    // Per track: {choice, mute, notes (in this pattern), label, drums}.
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY changed)
    // The selected track's notes in this pattern: {start, length, pitch, velocity}.
    Q_PROPERTY(QVariantList notes READ notes NOTIFY changed)
    // Per pattern: has notes.
    Q_PROPERTY(QVariantList patternsUsed READ patternsUsed NOTIFY changed)
    // For a drum track (K909): its voices, top to bottom: {name, note}.
    Q_PROPERTY(QVariantList drumLanes READ drumLanes CONSTANT)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY changed)
    // Where the player is.
    Q_PROPERTY(bool playing READ playing NOTIFY positionChanged)
    Q_PROPERTY(int playingPattern READ playingPattern NOTIFY positionChanged)
    Q_PROPERTY(int tick READ tick NOTIFY positionChanged)

public:
    SequencerController(krate::seq::Sequencer &sequencer, std::vector<NoteTarget> targets, QString path,
                        QObject *parent = nullptr);
    ~SequencerController() override;

    static SequencerController *create(QQmlEngine *, QJSEngine *);

    // Reads sequencer.json (or starts empty) and hands it to the player.
    void load();
    // Writes now if an edit is waiting (at quit).
    void flush();

    int revision() const { return m_revision; }
    int pattern() const { return m_state.pattern; }
    void setPattern(int pattern);
    int track() const { return m_track; }
    void setTrack(int track);
    bool running() const { return m_state.running; }
    void setRunning(bool on);
    int length() const;
    int grid() const;
    void setGrid(int grid);
    int gridTicks() const;
    int ticksPerBeat() const { return krate::seq::kTicksPerBeat; }
    int maxTicks() const { return krate::seq::kMaxTicks; }
    QStringList gridNames() const;
    QStringList targetChoices() const;
    QVariantList tracks() const;
    QVariantList notes() const;
    QVariantList patternsUsed() const;
    QVariantList drumLanes() const;
    bool hasClipboard() const { return m_clipboard.has_value(); }
    bool playing() const { return m_playing; }
    int playingPattern() const { return m_playingPattern; }
    int tick() const { return m_tick; }

    Q_INVOKABLE void setTarget(int track, int choice); // 0 = none
    Q_INVOKABLE void setMute(int track, bool mute);
    Q_INVOKABLE void setLength(int ticks);             // rounded to the grid, 1 step .. 4 bars
    // Notes of the selected track in this pattern, by index into `notes`.
    Q_INVOKABLE int addNote(int start, int pitch, int length, int velocity); // -1 if it doesn't fit
    Q_INVOKABLE void moveNote(int index, int start, int pitch);
    Q_INVOKABLE void resizeNote(int index, int length);
    Q_INVOKABLE void setVelocity(int index, int velocity);
    Q_INVOKABLE void deleteNote(int index);
    Q_INVOKABLE void clearTrack();
    Q_INVOKABLE void clearPattern();
    Q_INVOKABLE void copyPattern();
    Q_INVOKABLE void pastePattern();

    // The GUI's copy (tests, the Song screen's import).
    const SequencerState &state() const { return m_state; }

signals:
    void changed();
    void positionChanged();

private:
    krate::seq::Pattern &current() { return m_state.bank->patterns[std::size_t(m_state.pattern)]; }
    const krate::seq::Pattern &current() const { return m_state.bank->patterns[std::size_t(m_state.pattern)]; }
    krate::seq::TrackNotes &currentNotes() { return current().tracks[std::size_t(m_track)]; }
    int choiceOf(int track) const;
    void applyTargets();
    void edited(); // hand the player the bank, tell the screen, save soon
    void save();
    void poll();

    static SequencerController *s_instance;
    krate::seq::Sequencer &m_sequencer;
    std::vector<NoteTarget> m_targets;
    QString m_path;
    SequencerState m_state;
    std::optional<krate::seq::Pattern> m_clipboard;
    int m_track = 0;
    int m_revision = 0;
    bool m_playing = false;
    int m_playingPattern = 0;
    int m_tick = 0;
    bool m_dirty = false;
    bool m_readOnly = false;
    QTimer m_saveTimer;
    QTimer m_pollTimer;
};
