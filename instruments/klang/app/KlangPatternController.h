#pragma once

#include <array>
#include <optional>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "klang/dsp/Pattern.h"

class QQmlEngine;
class QJSEngine;

namespace klang {
class Engine;
}

// QML singleton "Patterns": Klang's 16 patterns (docs/BRIEF.md#scope-v1).
// Choosing a pattern shows it for editing and cues it (stopped: at once;
// playing: from the end of the pass). A click on a step does what the tool
// says: switch it on or off, accent it, roll it (1-4 hits), reverse it, or
// pitch it up or down. Every edit goes to the engine as a whole pattern;
// patterns.json is saved 2 s after an edit and on destruction.
class KlangPatternController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Patterns)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int current READ current WRITE select NOTIFY changed)      // 0..15, the one shown
    Q_PROPERTY(int length READ length WRITE setLength NOTIFY changed)     // 1..64
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY changed)           // 0..3: steps 1-16 .. 49-64
    Q_PROPERTY(int tool READ tool WRITE setTool NOTIFY changed)           // a Tool
    Q_PROPERTY(bool hasCopy READ hasCopy NOTIFY changed)
    Q_PROPERTY(int playing READ playing NOTIFY playingChanged)            // the pattern playing
    Q_PROPERTY(int playingStep READ playingStep NOTIFY playingChanged)    // -1 when stopped

public:
    enum Tool { Note, Accent, Roll, Reverse, PitchUp, PitchDown };
    Q_ENUM(Tool)

    KlangPatternController(klang::Engine &engine, const QString &filePath, QObject *parent);
    ~KlangPatternController() override;

    static KlangPatternController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    int current() const { return m_current; }
    void select(int index);
    int length() const { return pattern().length; }
    void setLength(int length);
    int page() const { return m_page; }
    void setPage(int page);
    int tool() const { return m_tool; }
    void setTool(int tool);
    bool hasCopy() const { return m_copy.has_value(); }
    int playing() const { return m_playing; }
    int playingStep() const { return m_playingStep; }
    const klang::Pattern &pattern() const { return m_bank[std::size_t(m_current)]; }
    const klang::Pattern &pattern(int index) const { return m_bank[std::size_t(index)]; }

    // A step: on, accent, repeat (1..4), reverse, pitch (-12..12).
    Q_INVOKABLE bool stepOn(int pad, int step) const;
    Q_INVOKABLE bool stepAccent(int pad, int step) const;
    Q_INVOKABLE int stepRepeat(int pad, int step) const;
    Q_INVOKABLE bool stepReverse(int pad, int step) const;
    Q_INVOKABLE int stepPitch(int pad, int step) const;
    Q_INVOKABLE void pressStep(int pad, int step); // what the tool does
    Q_INVOKABLE bool padUsed(int pad) const;       // any step on in this pattern
    Q_INVOKABLE bool used(int index) const;        // any step on in that pattern
    // The trance gate's 16 steps for this pattern.
    Q_INVOKABLE bool gateStep(int step) const;
    Q_INVOKABLE void toggleGateStep(int step);
    Q_INVOKABLE void clear();       // this pattern: no steps, 16 long, the gate as new
    Q_INVOKABLE void clearPad(int pad);
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();

    void save();
    static QJsonObject toJson(const std::array<klang::Pattern, klang::kPatterns> &bank, int current);
    static std::array<klang::Pattern, klang::kPatterns> fromJson(const QJsonObject &json, int *current);

signals:
    void changed();
    void playingChanged();

private:
    klang::Pattern &pattern() { return m_bank[std::size_t(m_current)]; }
    klang::Step *step(int pad, int step);
    const klang::Step *step(int pad, int step) const;
    void edited();
    void poll();

    static inline KlangPatternController *s_instance = nullptr;

    klang::Engine &m_engine;
    QString m_filePath;
    std::array<klang::Pattern, klang::kPatterns> m_bank{};
    int m_current = 0;
    int m_page = 0;
    int m_tool = Note;
    int m_revision = 0;
    int m_playing = 0;
    int m_playingStep = -1;
    std::optional<klang::Pattern> m_copy;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
