#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
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
//
// An external transport (JACK's, when a timebase master such as a DAW
// publishes tempo and position) takes over while present: the host hands it
// in before each block (setExternal), the state follows it, and play/stop
// from the GUI become requests the host passes on (takeRequest) so the
// external transport, and everything following it, starts and stops together.
//
// The Krate arranger (docs/DAW.md, D3) sets where play starts and a loop
// range in bars: reaching the loop's end, the transport is back at its start
// (restarted), on the exact sample when the host splits its block there
// (framesUntilLoop). While an external transport leads, position and looping
// are its own.
class Transport
{
public:
    // What the external transport says at the start of a block.
    struct External
    {
        bool rolling = false;
        double bpm = 120.0; // quarter notes per minute
        double beat = 0.0;  // quarter notes since the start of the song
    };
    enum Request { None = 0, Start = 1, Stop = 2 };

    // An external transport from bars, beats and ticks (JACK's BBT: bar and
    // beat count from 1, beats are the time signature's, `bpm` counts them);
    // Krate counts quarter notes.
    static External fromBbt(bool rolling, int bar, int beat, double tick, double ticksPerBeat, double beatsPerBar,
                            double beatType, double bpm) noexcept
    {
        const double quarter = 4.0 / beatType;
        External e;
        e.rolling = rolling;
        e.bpm = bpm * quarter;
        e.beat = (double(bar - 1) * beatsPerBar + double(beat - 1) + tick / ticksPerBeat) * quarter;
        return e;
    }

    // GUI thread. While following an external transport, play/stop is a
    // request to it (and the tempo is its, whatever is set here).
    void setPlaying(bool playing) noexcept
    {
        if (m_following.load(std::memory_order_relaxed))
            m_request.store(playing ? Start : Stop, std::memory_order_relaxed);
        else
            m_playing.store(playing, std::memory_order_relaxed);
    }
    // True while an external transport leads (tempo, position, play/stop).
    bool following() const noexcept { return m_following.load(std::memory_order_relaxed); }
    bool playing() const noexcept { return m_playing.load(std::memory_order_relaxed); }
    void setBpm(double bpm) noexcept { m_bpm.store(bpm, std::memory_order_relaxed); }
    double bpm() const noexcept { return m_bpm.load(std::memory_order_relaxed); }
    // Beat position at the start of the last block (for display: beat LEDs, counters).
    double beatPosition() const noexcept { return m_lastBeat.load(std::memory_order_relaxed); }

    // GUI thread: where play starts (beats; 0 by default).
    void setStartBeat(double beat) noexcept { m_startBeat.store(std::max(beat, 0.0), std::memory_order_relaxed); }
    double startBeat() const noexcept { return m_startBeat.load(std::memory_order_relaxed); }
    // GUI thread: loop bars [fromBar, toBar) while on (toBar > fromBar, bars of 4 beats).
    void setLoop(bool on, int fromBar, int toBar) noexcept
    {
        const bool valid = on && fromBar >= 0 && toBar > fromBar;
        m_loop.store(valid ? std::uint64_t(std::uint32_t(fromBar)) << 32 | std::uint32_t(toBar) : 0,
                     std::memory_order_relaxed);
    }
    bool looping() const noexcept { return m_loop.load(std::memory_order_relaxed) != 0; }
    int loopFromBar() const noexcept { return int(m_loop.load(std::memory_order_relaxed) >> 32); }
    int loopToBar() const noexcept { return int(m_loop.load(std::memory_order_relaxed) & 0xFFFFFFFFu); }

    // Audio thread, before advance(): of the next `frames`, how many come
    // before the loop's end (a block split there wraps on the sample), or
    // `frames` when it isn't in them.
    std::uint32_t framesUntilLoop(std::uint32_t frames, double sampleRate) const noexcept
    {
        const std::uint64_t loop = m_loop.load(std::memory_order_relaxed);
        if (!loop || m_hasExternal || !m_playing.load(std::memory_order_relaxed))
            return frames;
        const double end = double(loop & 0xFFFFFFFFu) * kBeatsPerBar;
        const double bpm = m_bpm.load(std::memory_order_relaxed);
        const double from = m_wasPlaying ? beatNow() : m_startBeat.load(std::memory_order_relaxed);
        if (from >= end - kLoopEpsilon)
            return frames;
        const double k = std::ceil((end - from) * 60.0 / bpm * sampleRate - 1e-6);
        return k >= double(frames) ? frames : std::uint32_t(std::max(k, 1.0));
    }

    // Audio thread, before advance(): the external transport for this block,
    // or nullptr when there is none (this transport keeps its own time).
    void setExternal(const External *external) noexcept
    {
        m_hasExternal = external != nullptr;
        if (external)
            m_external = *external;
    }
    // Audio thread (the host): a play/stop pressed while following, to pass on.
    Request takeRequest() noexcept { return Request(m_request.exchange(None, std::memory_order_relaxed)); }

    // Audio thread, once per block: the state for this block, then advance past it.
    TransportState advance(std::uint32_t frames, double sampleRate) noexcept
    {
        if (m_hasExternal)
            return follow(frames, sampleRate);
        if (m_following.load(std::memory_order_relaxed)) {
            // The external transport went away: carry on from where it was.
            m_following.store(false, std::memory_order_relaxed);
            anchor(m_predicted, m_bpm.load(std::memory_order_relaxed), sampleRate);
        }

        TransportState state;
        state.playing = m_playing.load(std::memory_order_relaxed);
        state.bpm = m_bpm.load(std::memory_order_relaxed);

        const std::uint64_t loop = m_loop.load(std::memory_order_relaxed);
        if (state.playing && !m_wasPlaying) {
            state.restarted = true;
            anchor(m_startBeat.load(std::memory_order_relaxed), state.bpm, sampleRate);
        } else if (state.playing && loop && m_lastStart < double(loop & 0xFFFFFFFFu) * kBeatsPerBar - kLoopEpsilon
                   && beatNow() >= double(loop & 0xFFFFFFFFu) * kBeatsPerBar - kLoopEpsilon) {
            // The loop's end: back to its start, as if play were pressed there.
            state.restarted = true;
            anchor(double(loop >> 32) * kBeatsPerBar, state.bpm, sampleRate);
        } else if (state.bpm != m_anchorBpm || sampleRate != m_anchorRate) {
            anchor(beatNow(), state.bpm, sampleRate);
        }
        m_wasPlaying = state.playing;

        if (state.playing) {
            state.beatAtBlockStart = beatNow();
            m_samplesSinceAnchor += frames;
        }
        m_lastStart = state.beatAtBlockStart;
        m_lastBeat.store(state.beatAtBlockStart, std::memory_order_relaxed);
        return state;
    }

private:
    // Following the external transport. Its position is absolute (the song's),
    // so a start or a jump that lands on a bar line restarts the sequencers
    // there; anywhere else they stop for this block and join at the next bar,
    // as they do joining a playing rack (bars are 4 beats).
    TransportState follow(std::uint32_t frames, double sampleRate) noexcept
    {
        const External &e = m_external;
        m_following.store(true, std::memory_order_relaxed);
        m_bpm.store(e.bpm, std::memory_order_relaxed);
        m_playing.store(e.rolling, std::memory_order_relaxed);

        TransportState state;
        state.bpm = e.bpm;
        state.playing = e.rolling;
        const double blockBeats = double(frames) * e.bpm / 60.0 / sampleRate;
        const bool starting = e.rolling && !m_wasPlaying;
        const bool jumped = e.rolling && m_wasPlaying && std::fabs(e.beat - m_predicted) > kJumpBeats;
        if (starting || jumped) {
            const double inBar = e.beat - std::floor(e.beat / kBeatsPerBar) * kBeatsPerBar;
            const bool onBar = inBar <= blockBeats || kBeatsPerBar - inBar < 1e-6;
            if (onBar)
                state.restarted = true;
            else if (jumped)
                state.playing = false; // a gap of one block: then they join at the next bar
        }
        if (state.playing)
            state.beatAtBlockStart = e.beat;
        m_wasPlaying = state.playing;
        m_predicted = e.rolling ? e.beat + blockBeats : e.beat;
        anchor(m_predicted, e.bpm, sampleRate); // for carrying on if it goes away
        m_lastBeat.store(state.beatAtBlockStart, std::memory_order_relaxed);
        return state;
    }

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

    static constexpr double kBeatsPerBar = 4.0;
    static constexpr double kJumpBeats = 0.02; // further than this from where it should be: a locate
    static constexpr double kLoopEpsilon = 1e-6; // beats: a block split at the loop's end may stop this short of it

    std::atomic<bool> m_playing{false};
    std::atomic<double> m_bpm{120.0};
    std::atomic<double> m_lastBeat{0.0};
    std::atomic<bool> m_following{false};
    std::atomic<int> m_request{None};
    std::atomic<double> m_startBeat{0.0};
    std::atomic<std::uint64_t> m_loop{0}; // fromBar << 32 | toBar, 0 = off

    // Audio thread only.
    bool m_wasPlaying = false;
    double m_anchorBeat = 0.0;
    double m_anchorBpm = 120.0;
    double m_anchorRate = 48000.0;
    std::uint64_t m_samplesSinceAnchor = 0;
    bool m_hasExternal = false;
    External m_external;
    double m_predicted = 0.0; // where the external beat should be next block
    double m_lastStart = 0.0; // the last block's beat (a loop wraps once it's reached)
};

// A standalone panel's TEMPO knob while an external transport leads: the knob
// value (0..1) that shows its tempo, or -1 when the knob already does (or
// nothing external leads). `bpmAt0` and `bpmAt1`: the knob's BPM at 0 and 1
// (a linear knob).
inline double followingTempoValue(const Transport &transport, double knobBpm, double bpmAt0, double bpmAt1) noexcept
{
    if (!transport.following() || std::fabs(transport.bpm() - knobBpm) < 0.25 || bpmAt1 <= bpmAt0)
        return -1.0;
    const double v = (transport.bpm() - bpmAt0) / (bpmAt1 - bpmAt0);
    return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
}

} // namespace krate
