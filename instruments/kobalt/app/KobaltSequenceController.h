#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "kobalt/app/SequenceFile.h"

class QQmlEngine;
class QJSEngine;
class KobaltController;

namespace kobalt {
class Engine;
}

// QML singleton "Sequencer": Kobalt's step sequencer, 8 patterns of up to 64
// steps (docs/BRIEF.md#step-sequencer). Loading from the keys: pick a step,
// then play notes one at a time; each goes into the step and the next is
// picked. A key going down while the one before is still held makes the step
// before legato (it runs into this one). REST, TIE, LEGATO and CLEAR; shift
// on a step makes it the last. Every edit goes to the engine as a whole
// pattern; sequences.json is saved 2 s after an edit and on destruction.
class KobaltSequenceController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sequencer)
    QML_SINGLETON

    Q_PROPERTY(int patternIndex READ patternIndex WRITE setPatternIndex NOTIFY changed) // 0..7
    Q_PROPERTY(int length READ length NOTIFY changed)                                   // 1..64
    Q_PROPERTY(int cursor READ cursor NOTIFY changed)              // the step loading goes to; -1 none
    Q_PROPERTY(int playingStep READ playingStep NOTIFY playingStepChanged) // -1 when not playing
    Q_PROPERTY(int revision READ revision NOTIFY changed)          // bumped on every edit
    Q_PROPERTY(bool active READ active NOTIFY changed)             // Play Mode is Sequencer

public:
    // Not default-constructible on purpose (see k303's SynthController). Loads
    // the patterns and hands the current one to the engine, so call it before
    // processing starts.
    KobaltSequenceController(KobaltController &panel, kobalt::Engine &engine, const QString &filePath, QObject *parent);
    ~KobaltSequenceController() override;

    static KobaltSequenceController *create(QQmlEngine *, QJSEngine *);

    int patternIndex() const { return m_bank.current; }
    void setPatternIndex(int index);
    int length() const { return current().length; }
    int cursor() const { return m_cursor; }
    int playingStep() const { return m_playingStep; }
    int revision() const { return m_revision; }
    bool active() const;
    const kobalt::persistence::SequenceBank &bank() const { return m_bank; }
    const kobalt::SeqPattern &current() const { return m_bank.patterns[std::size_t(m_bank.current)]; }

    // 0 = rest, 1 = note, 2 = tie.
    Q_INVOKABLE int stepState(int step) const;
    Q_INVOKABLE bool stepLegato(int step) const;
    Q_INVOKABLE QString stepText(int step) const; // e.g. "C3", "C3 legato", "tie", "rest"
    // Click a step: load from it (again: stop loading); shift: make it the last step.
    Q_INVOKABLE void pressStep(int step, bool shift);
    Q_INVOKABLE void rest();
    Q_INVOKABLE void tie();
    Q_INVOKABLE void legato(); // the step just loaded (before the cursor) runs into the next, or no longer
    Q_INVOKABLE void clear();
    // A new random pattern into the current one, at its length: from the held
    // keys, else the pattern's notes, else C minor pentatonic; over Arp Range's
    // octaves, Density the share of steps that play. Each press a different one.
    Q_INVOKABLE void generate();

    // Load `note` into the cursor's step and move on (what playing a key does).
    // `slur`: the key before is still held, so the step before runs into this one.
    void loadNote(int note, bool slur);
    void save();

signals:
    void changed();
    void playingStepChanged();

private:
    kobalt::SeqPattern &current() { return m_bank.patterns[std::size_t(m_bank.current)]; }
    void edited();      // to the engine, notify, save in 2 s
    void advance();     // the cursor to the next step (wrapping at the length)
    int previous() const; // the step before the cursor (wrapping), -1 if not loading
    void poll();        // playing step, loading from the keys

    static inline KobaltSequenceController *s_instance = nullptr;

    KobaltController &m_panel;
    kobalt::Engine &m_engine;
    QString m_filePath;
    kobalt::persistence::SequenceBank m_bank;
    int m_cursor = -1;
    int m_playingStep = -1;
    int m_revision = 0;
    QList<int> m_held;    // keys down at the last poll
    int m_lastLoaded = -1; // the key that loaded the step before the cursor
    std::uint32_t m_seed = 0x5EED1234u;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
