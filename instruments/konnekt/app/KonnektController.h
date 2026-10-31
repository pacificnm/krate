#pragma once

#include <array>
#include <memory>
#include <vector>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "konnekt/dsp/Engine.h"

class QQmlEngine;
class QJSEngine;

// QML singleton "Konnekt": the patch on screen and in the engine
// (docs/BRIEF.md#scope-v1): modules (made and freed here, run by the engine),
// where they sit, their knobs, the cables, voices, MIDI Learn, the live traces
// and the scope, the help bar's text. The patch in use is saved to
// current.json 2 s after a change and on quitting.
class KonnektController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Konnekt)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)                 // modules or cables changed
    Q_PROPERTY(int moduleRevision READ moduleRevision NOTIFY modulesChanged) // modules added or removed
    Q_PROPERTY(int knobRevision READ knobRevision NOTIFY knobsChanged)    // a knob moved (here or by MIDI)
    Q_PROPERTY(int frame READ frame NOTIFY frameChanged)                  // ~20 Hz: traces and scope to redraw
    Q_PROPERTY(QVariantList moduleTypes READ moduleTypes CONSTANT)        // [{type, name, category, help}]
    Q_PROPERTY(int voices READ voices WRITE setVoices NOTIFY changed)
    Q_PROPERTY(QString help READ help WRITE setHelp NOTIFY helpChanged)   // the help bar
    Q_PROPERTY(bool learning READ learning WRITE setLearning NOTIFY learnChanged)
    Q_PROPERTY(int notesHeld READ notesHeld NOTIFY frameChanged)
    Q_PROPERTY(QString scopeName READ scopeName NOTIFY changed)          // what the scope shows, "" = nothing

public:
    KonnektController(konnekt::Engine &engine, const QString &dataDir, QObject *parent);
    ~KonnektController() override;

    static KonnektController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    int moduleRevision() const { return m_moduleRevision; }
    int knobRevision() const { return m_knobRevision; }
    int frame() const { return m_frame; }
    QVariantList moduleTypes() const;
    int voices() const { return m_engine.voices(); }
    void setVoices(int voices);
    QString help() const { return m_help; }
    void setHelp(const QString &help);
    bool learning() const { return m_learning; }
    void setLearning(bool on);
    int notesHeld() const { return m_engine.notesHeld(); }
    QString scopeName() const;

    // --- Modules
    Q_INVOKABLE int addModule(const QString &type, qreal x, qreal y); // its slot, -1 if the rack is full
    Q_INVOKABLE void removeModule(int slot);
    Q_INVOKABLE void moveModule(int slot, qreal x, qreal y);
    Q_INVOKABLE QVariantList modules() const;                         // [{slot, type, x, y}]
    Q_INVOKABLE QVariantMap moduleInfo(const QString &type) const;    // the module's table
    Q_INVOKABLE void clearPatch();

    // --- Knobs (positions 0..1)
    Q_INVOKABLE qreal knob(int slot, int knob) const;
    Q_INVOKABLE void setKnob(int slot, int knob, qreal position);
    Q_INVOKABLE qreal knobDefault(int slot, int knob) const;
    Q_INVOKABLE QString knobText(int slot, int knob, qreal position) const; // "1.20 kHz", "Saw up"

    // --- Cables
    Q_INVOKABLE QVariantList cables() const;                        // [{fromSlot, fromJack, toSlot, toJack, color}]
    Q_INVOKABLE bool plug(int outSlot, int outJack, int inSlot, int inJack); // replaces the input's cable
    Q_INVOKABLE void unplug(int inSlot, int inJack);
    Q_INVOKABLE QVariantMap cableInto(int inSlot, int inJack) const; // {fromSlot, fromJack} or empty

    // --- Seeing signals
    Q_INVOKABLE QVariantList trace(int slot, int jack) const;       // 64 lows then 64 highs, oldest first
    Q_INVOKABLE void scope(int slot, int jack);                      // the big scope shows this output
    Q_INVOKABLE QVariantList scopeSamples(int count) const;          // the last `count` samples
    Q_INVOKABLE int scopeSlot() const { return m_scopeSlot; }

    // --- MIDI Learn: while learning, a knob clicked waits for a controller's knob.
    Q_INVOKABLE void armLearn(int slot, int knob);
    Q_INVOKABLE int learnedCc(int slot, int knob) const; // -1: none
    Q_INVOKABLE void forget(int slot, int knob);
    Q_INVOKABLE bool armed(int slot, int knob) const { return m_armedSlot == slot && m_armedKnob == knob; }

    // --- Patches as JSON (docs/BRIEF.md#scope-v1)
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json); // replaces the patch; false if it isn't one
    void save();

signals:
    void changed();
    void modulesChanged();
    void knobsChanged();
    void frameChanged();
    void helpChanged();
    void learnChanged();

private:
    struct Placed
    {
        std::unique_ptr<konnekt::Module> module;
        qreal x = 0, y = 0;
    };
    struct Cable
    {
        int fromSlot, fromJack, toSlot, toJack, color;
    };

    void structureChanged(); // the order to the engine, notify, save soon
    void poll();
    const konnekt::Module *module(int slot) const;

    static inline KonnektController *s_instance = nullptr;

    konnekt::Engine &m_engine;
    QString m_path;
    std::array<Placed, konnekt::kMaxModules> m_slots;
    std::vector<std::unique_ptr<konnekt::Module>> m_leaving; // given back by the engine soon
    std::vector<Cable> m_cables;
    int m_nextColor = 0;
    int m_revision = 0, m_moduleRevision = 0, m_knobRevision = 0, m_frame = 0;
    QString m_help;
    bool m_learning = false;
    int m_armedSlot = -1, m_armedKnob = -1;
    int m_scopeSlot = -1, m_scopeJack = -1;
    std::array<float, konnekt::kMaxModules * konnekt::kMaxKnobs> m_seenKnobs{};
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
