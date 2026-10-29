#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class Transport;
}
namespace krimson {
class Engine;
}

// QML singleton "Krimson": the edited slot's controls (krate::ParamController
// by key), which slot is edited, plus Tempo and run/stop. Standalone, those
// drive the transport it is given; in the Krate rack it is given none.
class KrimsonController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Krimson)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    // The slot the panel edits: 0..3 = A..D.
    Q_PROPERTY(int editedSlot READ editedSlot WRITE setEditedSlot NOTIFY editedSlotChanged)
    // Bumped when the held keys change (MIDI or the on-screen keyboard).
    Q_PROPERTY(int notesRevision READ notesRevision NOTIFY notesChanged)
    // MORPH lit: turning a knob sets its morph target (docs/PLAY.md#morph).
    Q_PROPERTY(bool morphEdit READ morphEdit WRITE setMorphEdit NOTIFY morphChanged)
    // Bumped when any morph target changes (or the edited slot does).
    Q_PROPERTY(int morphRevision READ morphRevision NOTIFY morphChanged)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    KrimsonController(krimson::Engine &engine, const QString &panelFilePath, krate::Transport *transport,
                      QObject *parent);
    ~KrimsonController() override;

    static KrimsonController *create(QQmlEngine *, QJSEngine *);

    bool ownsTransport() const { return m_transport != nullptr; }
    bool playing() const;
    void setPlaying(bool playing);
    int editedSlot() const;
    void setEditedSlot(int slot);
    int notesRevision() const { return m_notesRevision; }

    // On-screen keyboard and wheels: played as the keyboard (the lit slots).
    Q_INVOKABLE void noteOn(int note);
    Q_INVOKABLE void noteOff(int note);
    Q_INVOKABLE bool noteHeld(int note) const;
    Q_INVOKABLE void pitchBend(qreal amount); // -1..1
    Q_INVOKABLE void modWheel(qreal amount);  // 0..1

    // The edited slot's morph targets, by key: -1 = none.
    bool morphEdit() const { return m_morphEdit; }
    void setMorphEdit(bool on);
    int morphRevision() const { return m_morphRevision; }
    Q_INVOKABLE qreal morph(const QString &key) const;
    Q_INVOKABLE void setMorph(const QString &key, qreal target);
    Q_INVOKABLE void clearMorph(const QString &key);
    Q_INVOKABLE void clearAllMorph(); // the edited slot's
    Q_INVOKABLE bool morphable(const QString &key) const;
    // After morph targets were set behind the panel's back (a program, a performance).
    void morphReloaded();

signals:
    void playingChanged();
    void editedSlotChanged();
    void notesChanged();
    void morphChanged();

private:
    void syncTempo();
    void syncArp(); // standalone: the edited slot's ARP on starts the transport, off stops what it started
    void poll();

    static inline KrimsonController *s_instance = nullptr;

    krimson::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
    QTimer m_pollTimer;
    bool m_playing = false;
    int m_notesRevision = 0;
    bool m_morphEdit = false;
    bool m_arpOn = false;
    int m_arpSlot = 0;
    bool m_arpStartedTransport = false;
    int m_morphRevision = 0;
    std::uint64_t m_heldSeen[2] = {0, 0};
};
