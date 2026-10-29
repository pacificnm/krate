#pragma once

#include <QList>
#include <QSet>
#include <QStringList>

#include "app/RunControl.h"

namespace krate {
class Rack;
class Transport;
}

// The rack's RunControl: each instrument's own RUN / STOP runs just its
// sequencer on the rack's transport, while the rack's PLAY / STOP runs them all.
// - An instrument started while the rack is stopped plays alone (the others
//   are switched off, then the transport starts).
// - Started while the rack plays: it joins, in time, from the next bar.
// - Stopping the last one running stops the transport.
// - The rack's PLAY switches every sequencer on (playAll()).
// Devices are the rack's, in the order of `ids`. GUI thread.
class RackRunControl final : public krate::RunControl
{
    Q_OBJECT

public:
    RackRunControl(krate::Rack &rack, krate::Transport &transport, QStringList ids, QObject *parent = nullptr);

    bool running(const QString &id) const override;
    void setRunning(const QString &id, bool running) override;
    void addRunner(const QString &id) override;

    // The rack's PLAY: every sequencer on (the transport is the caller's).
    void playAll();
    // The transport started or stopped elsewhere: tell the panels.
    void transportChanged() { emit changed(); }

private:
    void apply(int device);

    krate::Rack &m_rack;
    krate::Transport &m_transport;
    QStringList m_ids;
    QList<bool> m_enabled;
    QSet<QString> m_runners;
};
