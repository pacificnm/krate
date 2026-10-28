#pragma once

#include <atomic>
#include <cstdint>

#include "core/Instrument.h"

namespace krate {

// A transport controlled from the GUI (play/stop, tempo) and advanced by the
// host once per block: a standalone app gives one to its single instrument,
// the Krate rack shares one across all its devices, the offline renderers use
// one too. Instruments only ever see the TransportState it produces
// (docs/SHARED_FOUNDATIONS.MD#transport).
//
// Beat position is recomputed from an anchor (re-set at play and at every
// tempo change) rather than summed per block, so it never drifts.
class Transport
{
public:
    // GUI thread.
    void setPlaying(bool playing) noexcept { m_playing.store(playing, std::memory_order_relaxed); }
    bool playing() const noexcept { return m_playing.load(std::memory_order_relaxed); }
    void setBpm(double bpm) noexcept { m_bpm.store(bpm, std::memory_order_relaxed); }
    double bpm() const noexcept { return m_bpm.load(std::memory_order_relaxed); }

    // Audio thread, once per block: the state for this block, then advance past it.
    TransportState advance(std::uint32_t frames, double sampleRate) noexcept
    {
        TransportState state;
        state.playing = m_playing.load(std::memory_order_relaxed);
        state.bpm = m_bpm.load(std::memory_order_relaxed);

        if (state.playing && !m_wasPlaying) {
            state.restarted = true;
            anchor(0.0, state.bpm, sampleRate);
        } else if (state.bpm != m_anchorBpm || sampleRate != m_anchorRate) {
            anchor(beatNow(), state.bpm, sampleRate);
        }
        m_wasPlaying = state.playing;

        if (state.playing) {
            state.beatAtBlockStart = beatNow();
            m_samplesSinceAnchor += frames;
        }
        return state;
    }

private:
    double beatNow() const noexcept
    {
        return m_anchorBeat + double(m_samplesSinceAnchor) * m_anchorBpm / 60.0 / m_anchorRate;
    }

    void anchor(double beat, double bpm, double sampleRate) noexcept
    {
        m_anchorBeat = beat;
        m_anchorBpm = bpm;
        m_anchorRate = sampleRate;
        m_samplesSinceAnchor = 0;
    }

    std::atomic<bool> m_playing{false};
    std::atomic<double> m_bpm{120.0};

    // Audio thread only.
    bool m_wasPlaying = false;
    double m_anchorBeat = 0.0;
    double m_anchorBpm = 120.0;
    double m_anchorRate = 48000.0;
    std::uint64_t m_samplesSinceAnchor = 0;
};

} // namespace krate
