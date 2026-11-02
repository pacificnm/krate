#pragma once

#include <functional>

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
// - An instrument started while the rack is stopped plays alone (the others,
//   and the rack sequencer's RUN, are switched off, then the transport starts).
// - Started while the rack plays: it joins, in time, from the next bar.
// - Stopping the last one running stops the transport (unless the rack
//   sequencer still runs).
// - The rack's PLAY switches every sequencer on, the rack sequencer's too (playAll()),
//   except in the arranger's SONG mode: then the song plays alone (the rack
//   sequencer, which plays it; the instruments' own patterns off), an empty
//   song nothing.
// Devices are the rack's, in the order of `ids`. GUI thread.
class RackRunControl final : public krate::RunControl
{
    Q_OBJECT

public:
    RackRunControl(krate::Rack &rack, krate::Transport &transport, QStringList ids, QObject *parent = nullptr);

    bool running(const QString &id) const override;
    void setRunning(const QString &id, bool running) override;
    void addRunner(const QString &id) override;

    // The rack sequencer (the Seq screen's RUN, which plays the arranger's song
    // too): whether it runs, and switching it.
    void setRackSequencer(std::function<bool()> running, std::function<void(bool)> setRunning)
    {
        m_rackSequencerRunning = std::move(running);
        m_setRackSequencer = std::move(setRunning);
    }

    // The arranger's SONG mode (on: the rack's PLAY plays the song alone).
    void setSongMode(std::function<bool()> songMode) { m_songMode = std::move(songMode); }
    // SONG switched: on while playing, the instruments' own patterns stop.
    void songModeChanged();

    // The rack's PLAY: every sequencer on, or in SONG mode the song alone (the transport is the caller's).
    void playAll();
    // The rack sequencer's RUN from silence: the instruments' own sequencers
    // off, the transport on (the rack sequencer plays alone).
    void playWithoutInstruments();
    // Whether any instrument's own sequencer is running.
    bool anyInstrumentRunning() const;
    bool transportPlaying() const;
    void stopTransport();
    // The transport started or stopped elsewhere: tell the panels.
    void transportChanged() { emit changed(); }

private:
    void apply(int device);

    krate::Rack &m_rack;
    krate::Transport &m_transport;
    QStringList m_ids;
    QList<bool> m_enabled;
    QSet<QString> m_runners;
    std::function<bool()> m_rackSequencerRunning;
    std::function<void(bool)> m_setRackSequencer;
    std::function<bool()> m_songMode;
};
