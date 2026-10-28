#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class Transport;
}
namespace kalliope {
class Engine;
}

// QML singleton "Kalliope": the synth's sliders, selectors and switches
// (krate::ParamController by key), plus Tempo and run/stop. Standalone, those
// drive the transport it is given (the arpeggiator follows it); in the Krate
// rack it is given none, because the rack's transport bar owns them.
class KalliopeController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Kalliope)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // False in the rack: the panel hides Tempo then.
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    // Bumped when the held keys change (MIDI or the on-screen keyboard); bind
    //     Kalliope.notesRevision >= 0 && Kalliope.noteHeld(n)
    Q_PROPERTY(int notesRevision READ notesRevision NOTIFY notesChanged)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    KalliopeController(kalliope::Engine &engine, const QString &panelFilePath, krate::Transport *transport,
                       QObject *parent);
    ~KalliopeController() override;

    static KalliopeController *create(QQmlEngine *, QJSEngine *);

    bool ownsTransport() const { return m_transport != nullptr; }
    bool playing() const;
    void setPlaying(bool playing);
    int notesRevision() const { return m_notesRevision; }

    // On-screen keyboard.
    Q_INVOKABLE void noteOn(int note);
    Q_INVOKABLE void noteOff(int note);
    Q_INVOKABLE bool noteHeld(int note) const;

signals:
    void playingChanged();
    void notesChanged();

private:
    void syncTempo(); // the Tempo knob drives the transport, if we have one
    void poll();

    static inline KalliopeController *s_instance = nullptr;

    kalliope::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
    QTimer m_pollTimer;
    bool m_playing = false;
    int m_notesRevision = 0;
    std::uint64_t m_heldSeen[2] = {0, 0};
};
