#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/PatternFile.h"

namespace k303 {
class Engine;
}
class QQmlEngine;
class QJSEngine;

// QML singleton "Pattern": the 16 steps of the current pattern as a list
// model, the selected step, and the editing modes. The GUI-side copy of all
// 64 patterns (8 banks of 8) is the source of truth for editing and saving; every change
// is sent to the audio thread as an Engine command (docs/ARCHITECTURE.md).
class PatternModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Pattern)
    QML_SINGLETON

    Q_PROPERTY(int index READ patternIndex WRITE setPatternIndex NOTIFY indexChanged) // pattern 0..63 (bank * 8 + number)
    Q_PROPERTY(int bank READ bank NOTIFY indexChanged)              // 0..7 = A..H, of the current pattern
    Q_PROPERTY(int number READ number NOTIFY indexChanged)          // 0..7, in its bank
    Q_PROPERTY(int pendingBank READ pendingBank NOTIFY indexChanged) // a BANK pressed, waiting for a number; -1 none
    Q_PROPERTY(QString name READ name NOTIFY indexChanged)          // "A1" .. "H8"
    Q_PROPERTY(int length READ length WRITE setLength NOTIFY lengthChanged)
    Q_PROPERTY(int selectedStep READ selectedStep WRITE setSelectedStep NOTIFY selectedStepChanged)

    // The selected step, for the note keys and function-button LEDs.
    Q_PROPERTY(int selectedNote READ selectedNote NOTIFY selectionChanged)     // 0..12, -1 on a rest
    Q_PROPERTY(int selectedOctave READ selectedOctave NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedAccent READ selectedAccent NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedSlide READ selectedSlide NOTIFY selectionChanged)
    Q_PROPERTY(int selectedGate READ selectedGate NOTIFY selectionChanged)     // 0 note, 1 tie, 2 rest

    // Keyboard on: note keys play the voice. Off: they set the selected step's note.
    Q_PROPERTY(bool keyboardMode READ keyboardMode WRITE setKeyboardMode NOTIFY modesChanged)
    // Time mode: DOWN = Note, UP = Tie, ACCENT = Rest.
    Q_PROPERTY(bool timeMode READ timeMode WRITE setTimeMode NOTIFY modesChanged)

public:
    enum Role {
        NoteRole = Qt::UserRole + 1,
        OctaveRole,
        AccentRole,
        SlideRole,
        GateRole,
        ActiveRole,   // step index < length
        LabelRole,    // step button text: "C", "D#↑" (octave up), "G↓" (down), "–" (tie), "" (rest)
    };
    Q_ENUM(Role)

    // Not default-constructible on purpose: see SynthController.
    PatternModel(k303::Engine &engine, const QString &filePath, QObject *parent);
    ~PatternModel() override;

    static PatternModel *create(QQmlEngine *, QJSEngine *);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Not index(): that would hide QAbstractListModel::index(row).
    int patternIndex() const { return m_bank.current; }
    void setPatternIndex(int index);
    int bank() const { return m_bank.current / k303::persistence::kPatternsPerBank; }
    int number() const { return m_bank.current % k303::persistence::kPatternsPerBank; }
    int pendingBank() const { return m_pendingBank; }
    QString name() const;
    // A BANK button: it waits (blinking) for a number, so a live change is one change.
    Q_INVOKABLE void pressBank(int bank);
    // A pattern button 1..8: that pattern in the waiting bank, else the current one.
    Q_INVOKABLE void pressNumber(int number);
    int length() const { return current().length; }
    void setLength(int length);
    int selectedStep() const { return m_selected; }
    void setSelectedStep(int step);

    int selectedNote() const;
    int selectedOctave() const { return selected().octave; }
    bool selectedAccent() const { return selected().accent; }
    bool selectedSlide() const { return selected().slide; }
    int selectedGate() const { return int(selected().gate); }

    bool keyboardMode() const { return m_keyboardMode; }
    void setKeyboardMode(bool on);
    bool timeMode() const { return m_timeMode; }
    void setTimeMode(bool on);

    // Edits to the selected step.
    Q_INVOKABLE void setNote(int semitone);   // also turns a rest into a note
    Q_INVOKABLE void octaveDown();
    Q_INVOKABLE void octaveUp();
    Q_INVOKABLE void toggleAccent();
    Q_INVOKABLE void toggleSlide();
    Q_INVOKABLE void setGate(int gate);       // 0 note, 1 tie, 2 rest

    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();
    Q_INVOKABLE void clear();                 // all rests, length kept
    Q_INVOKABLE void randomize();             // new random riff, length kept

    // Keyboard mode: play the voice directly while the sequencer is stopped.
    Q_INVOKABLE void auditionOn(int semitone);
    Q_INVOKABLE void auditionOff();

    Q_INVOKABLE void save();

signals:
    void indexChanged();
    void lengthChanged();
    void selectedStepChanged();
    void selectionChanged();
    void modesChanged();

private:
    const k303::Pattern &current() const { return m_bank.patterns[std::size_t(m_bank.current)]; }
    k303::Pattern &current() { return m_bank.patterns[std::size_t(m_bank.current)]; }
    const k303::Step &selected() const { return current().steps[std::size_t(m_selected)]; }

    template<typename Edit>
    void editSelected(Edit edit);
    void sendCurrentPattern();
    void scheduleSave();

    static inline PatternModel *s_instance = nullptr;

    k303::Engine &m_engine;
    QString m_filePath;
    k303::persistence::PatternBank m_bank;
    int m_pendingBank = -1;
    int m_selected = 0;
    bool m_keyboardMode = false;
    bool m_timeMode = false;
    bool m_dirty = false;
    QTimer m_saveTimer;
};
