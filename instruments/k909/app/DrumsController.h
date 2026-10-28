#pragma once

#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class Transport;
}
namespace k909 {
class Engine;
}

// QML singleton "Drums": K909's knobs (krate::ParamController by key), plus
// START/STOP and Tempo. Standalone, those drive the transport it is given; in
// the Krate rack it is given none, because the rack's transport bar owns them.
class DrumsController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Drums)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // False in the rack: the panel hides START/STOP and Tempo then.
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    DrumsController(k909::Engine &engine, const QString &panelFilePath, krate::Transport *transport,
                    QObject *parent);
    ~DrumsController() override;

    static DrumsController *create(QQmlEngine *, QJSEngine *);

    bool ownsTransport() const { return m_transport != nullptr; }
    bool playing() const;
    void setPlaying(bool playing);

    // Play one voice now (0 = bass drum .. 10 = ride, track order), unaccented.
    Q_INVOKABLE void audition(int track);

signals:
    void playingChanged();

private:
    void syncTempo(); // the Tempo knob drives the transport, if we have one

    static inline DrumsController *s_instance = nullptr;

    k909::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
};
