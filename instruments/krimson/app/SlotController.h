#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class KrimsonController;

namespace krimson {
class Engine;
}

// QML singleton "Slots": the four slots' setup (docs/PLAY.md#slots): which are
// lit (the keyboard plays them, the panel edits the pressed one), each slot's
// MIDI channel, on/off and keyboard zone, and the split key. Saves every slot's
// sound and setup to slots.json (2 s after a change and on destruction), since
// panel.json only holds the edited slot.
class SlotController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Slots)
    QML_SINGLETON

    // Bumped on every change; bind like `Slots.revision >= 0 && Slots.isLit(i)`.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int splitNote READ splitNote WRITE setSplitNote NOTIFY changed) // -1 = no split

public:
    // Not default-constructible on purpose (see k303's SynthController). Loads
    // slots.json into the engine: construct it before processing starts.
    SlotController(KrimsonController &panel, krimson::Engine &engine, const QString &filePath, QObject *parent);
    ~SlotController() override;

    static SlotController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }

    // A slot button: plain = play and edit only that slot; shift = add it to (or
    // take it from) the lit slots, editing it when it lights.
    Q_INVOKABLE void pressSlot(int slot, bool shift);
    Q_INVOKABLE bool isLit(int slot) const;
    Q_INVOKABLE int channel(int slot) const; // 1..16, 0 = none
    Q_INVOKABLE void setChannel(int slot, int channel);
    Q_INVOKABLE bool isOn(int slot) const;
    Q_INVOKABLE void setOn(int slot, bool on);
    Q_INVOKABLE int zone(int slot) const;    // 0 whole, 1 lower, 2 upper
    Q_INVOKABLE void setZone(int slot, int zone);
    int splitNote() const;
    void setSplitNote(int note);

    void save();

signals:
    void changed();

private:
    void load();
    void edited(); // notify and save in 2 s

    static inline SlotController *s_instance = nullptr;

    KrimsonController &m_panel;
    krimson::Engine &m_engine;
    QString m_filePath;
    int m_revision = 0;
    QTimer m_saveTimer;
};
