#pragma once

#include <array>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "kalliope/app/SequenceFile.h"

class QQmlEngine;
class QJSEngine;
class KalliopeController;

namespace kalliope {
class Engine;
}

// QML singleton "Sequencer": the step sequencer's 8 patterns, step recording
// from the keys, REST / TIE / CLEAR and the pattern generator
// (docs/SEQUENCER.md). Every edit goes to the engine as a whole pattern;
// sequences.json is saved 2 s after an edit and on destruction.
class SequenceController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sequencer)
    QML_SINGLETON

    Q_PROPERTY(int patternIndex READ patternIndex WRITE setPatternIndex NOTIFY changed) // 0..7
    Q_PROPERTY(int length READ length NOTIFY changed)                                   // 1..16
    Q_PROPERTY(int cursor READ cursor NOTIFY changed)              // the step recording goes to; -1 none
    Q_PROPERTY(int playingStep READ playingStep NOTIFY playingStepChanged) // -1 when not playing
    Q_PROPERTY(int revision READ revision NOTIFY changed)          // bumped on every edit
    Q_PROPERTY(bool active READ active NOTIFY changed)             // Play Mode is Sequencer

public:
    // Not default-constructible on purpose (see k303's SynthController). Loads
    // the patterns and hands the current one to the engine, so call it before
    // processing starts.
    SequenceController(KalliopeController &panel, kalliope::Engine &engine, const QString &filePath, QObject *parent);
    ~SequenceController() override;

    static SequenceController *create(QQmlEngine *, QJSEngine *);

    int patternIndex() const { return m_bank.current; }
    void setPatternIndex(int index);
    int length() const { return current().length; }
    int cursor() const { return m_cursor; }
    int playingStep() const { return m_playingStep; }
    int revision() const { return m_revision; }
    bool active() const;
    const kalliope::persistence::SequenceBank &bank() const { return m_bank; }
    const kalliope::SeqPattern &current() const { return m_bank.patterns[std::size_t(m_bank.current)]; }

    // 0 = rest, 1 = notes, 2 = tie.
    Q_INVOKABLE int stepState(int step) const;
    Q_INVOKABLE QString stepText(int step) const; // e.g. "C3 D#3 G3", "tie", "rest"
    // Click a step: record into it (again: stop recording); shift: make it the last step.
    Q_INVOKABLE void pressStep(int step, bool shift);
    Q_INVOKABLE void rest();
    Q_INVOKABLE void tie();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void generate();

    // Write `notes` into the cursor's step and move on (also what recording from the keys does).
    void recordStep(const QList<int> &notes);
    void save();

signals:
    void changed();
    void playingStepChanged();

private:
    kalliope::SeqPattern &current() { return m_bank.patterns[std::size_t(m_bank.current)]; }
    void edited();      // to the engine, notify, save in 2 s
    void advance();     // the cursor to the next step (wrapping at the length)
    void poll();        // playing step, recording from held keys
    QList<int> heldNotes() const;

    static inline SequenceController *s_instance = nullptr;

    KalliopeController &m_panel;
    kalliope::Engine &m_engine;
    QString m_filePath;
    kalliope::persistence::SequenceBank m_bank;
    int m_cursor = -1;
    int m_playingStep = -1;
    int m_revision = 0;
    QList<int> m_chord;       // keys gathered while recording, until all are released
    std::uint32_t m_seed = 0x5EED1234u;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
