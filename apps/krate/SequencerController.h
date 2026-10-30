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
#include "RackSounds.h"
#include "SequencerFile.h"
#include "SongImport.h"
#include "seq/Sequencer.h"

class QQmlEngine;
class RackRunControl;
class SongLibrary;
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
    // Saved sequences in the library (docs/SEQUENCER.md#saved-sequences): the
    // one open (0 = none), its name, whether it changed since; the list
    // ({id, name, saved}).
    Q_PROPERTY(int sequenceId READ sequenceId NOTIFY changed)
    Q_PROPERTY(QString sequenceName READ sequenceName NOTIFY changed)
    Q_PROPERTY(bool modified READ modified NOTIFY changed)
    Q_PROPERTY(QVariantList savedSequences READ savedSequences NOTIFY libraryChanged)
    // A copied track's notes, and where they came from ("Pattern 2, track 3"; "" = none).
    Q_PROPERTY(QString trackClipboard READ trackClipboard NOTIFY changed)
    // CHAIN mode and the chain: {pattern (0-based), repeats} per step.
    Q_PROPERTY(bool chainMode READ chainMode WRITE setChainMode NOTIFY changed)
    // What plays: 0 the chosen pattern (looped), 1 the range FROM..TO in order, 2 the chain.
    Q_PROPERTY(int playMode READ playMode WRITE setPlayMode NOTIFY changed)
    Q_PROPERTY(int rangeFrom READ rangeFrom WRITE setRangeFrom NOTIFY changed) // 0-based
    Q_PROPERTY(int rangeTo READ rangeTo WRITE setRangeTo NOTIFY changed)
    Q_PROPERTY(QVariantList chain READ chain NOTIFY changed)
    Q_PROPERTY(int chainStep READ chainStep NOTIFY positionChanged) // playing (-1: not chaining)
    Q_PROPERTY(int chainPass READ chainPass NOTIFY positionChanged)
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
    QString trackClipboard() const { return m_trackClipboardFrom; }
    bool chainMode() const { return m_state.chainMode; }
    void setChainMode(bool on);
    int playMode() const { return m_state.chainMode ? 2 : (m_state.rangeMode ? 1 : 0); }
    void setPlayMode(int mode);
    int rangeFrom() const { return m_state.rangeFrom; }
    void setRangeFrom(int pattern);
    int rangeTo() const { return m_state.rangeTo; }
    void setRangeTo(int pattern);
    QVariantList chain() const;
    int chainStep() const { return m_chainStep; }
    int chainPass() const { return m_chainPass; }
    bool playing() const { return m_playing; }
    int playingPattern() const { return m_playingPattern; }
    int tick() const { return m_tick; }

    // RUN, like an instrument's RUN / STOP: from silence it starts the rack
    // with the sequencer playing alone; while it plays, stops it (and the
    // rack, when nothing else is running); off while the rack plays, it joins.
    Q_INVOKABLE void toggleRun();
    void setRunControl(RackRunControl *run) { m_run = run; }
    // Where sequences are saved, and the rack's sounds and mixer to save with them.
    void setLibrary(SongLibrary *library, const RackSounds *sounds);

    int sequenceId() const { return int(m_state.sequenceId); }
    QString sequenceName() const { return m_state.sequenceName; }
    bool modified() const { return m_state.modified; }
    QVariantList savedSequences() const;
    // Saves over the open sequence (false if none is open).
    Q_INVOKABLE bool saveSequence();
    // Saves as a new sequence with this name, which becomes the open one.
    Q_INVOKABLE bool saveSequenceAs(const QString &name);
    // Opens a saved sequence: its patterns, tracks, chain and range, the
    // instruments' sounds and the mixer. What was there before (from before
    // the first one opened) comes back on close.
    Q_INVOKABLE bool openSequence(int id);
    Q_INVOKABLE void closeSequence();
    // A new, empty sequence: every pattern, the chain, the range and play
    // mode, and the mutes back to the start; the tracks keep their
    // instruments; no longer a saved sequence. RUN, the sounds and the mixer
    // stay. (The screen asks first when there are unsaved changes.)
    Q_INVOKABLE void newSequence();
    Q_INVOKABLE bool renameSequence(int id, const QString &name);
    Q_INVOKABLE bool deleteSequence(int id);

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
    // The selected track's notes in this pattern, copied; pasted over the
    // selected track's (in whichever pattern is shown). Its instrument and mute stay.
    Q_INVOKABLE void copyTrack();
    Q_INVOKABLE void pasteTrack();
    // The chain: add a pattern at the end (or after step `after`), remove a
    // step, set its repeats (1..16), empty it.
    Q_INVOKABLE void addToChain(int pattern, int after = -1);
    Q_INVOKABLE void removeChainStep(int step);
    Q_INVOKABLE void setChainRepeats(int step, int repeats);
    Q_INVOKABLE void clearChain();
    // Controller buttons (learnable rack actions): the next / previous pattern.
    Q_INVOKABLE void stepPattern(int delta);

    // Bars from a song (the Song screen): each chunk replaces track `track`'s
    // notes in a pattern, from `firstPattern` on, and sets that pattern's
    // length and grid; the track plays target `choice` (0 = leave it).
    // Returns how many patterns were written (they stop at pattern 16).
    int importChunks(int firstPattern, int track, const std::vector<songimport::Chunk> &chunks, int choice);
    // Of the `count` patterns from `firstPattern`, those whose track `track` has notes (0-based).
    QList<int> patternsWithNotes(int firstPattern, int count, int track) const;

    // The screen timer's tick: where the player is (public for tests).
    void poll();

    // The GUI's copy (tests, the Song screen's import).
    const SequencerState &state() const { return m_state; }

signals:
    void changed();
    void positionChanged();
    void libraryChanged();

private:
    krate::seq::Pattern &current() { return m_state.bank->patterns[std::size_t(m_state.pattern)]; }
    const krate::seq::Pattern &current() const { return m_state.bank->patterns[std::size_t(m_state.pattern)]; }
    krate::seq::TrackNotes &currentNotes() { return current().tracks[std::size_t(m_track)]; }
    int choiceOf(int track) const;
    void applyState(); // hand the player all of m_state
    void applyTargets();
    void edited(); // hand the player the bank, tell the screen, save soon
    void save();

    static SequencerController *s_instance;
    krate::seq::Sequencer &m_sequencer;
    RackRunControl *m_run = nullptr;
    SongLibrary *m_library = nullptr;
    const RackSounds *m_sounds = nullptr;
    // From before the first saved sequence opened: put back on close.
    struct Before
    {
        RackSounds::Snapshot sounds;
        QJsonObject sequencer;
    };
    std::optional<Before> m_before;
    std::vector<NoteTarget> m_targets;
    QString m_path;
    SequencerState m_state;
    std::optional<krate::seq::Pattern> m_clipboard;
    std::optional<krate::seq::TrackNotes> m_trackClipboard;
    QString m_trackClipboardFrom;
    int m_track = 0;
    int m_revision = 0;
    bool m_playing = false;
    int m_playingPattern = 0;
    int m_tick = 0;
    int m_chainStep = -1;
    int m_chainPass = 0;
    bool m_dirty = false;
    bool m_readOnly = false;
    QTimer m_saveTimer;
    QTimer m_pollTimer;
};
