#pragma once

#include <cstdint>

namespace krate {

enum class Wave : std::uint8_t { Triangle, Saw, Pulse, Square, Sine, Noise };

// Band-limited oscillator (Kalliope's docs/DSP.md#building-blocks; Krimson uses it too). Every discontinuity
// (a saw's wrap, a pulse's edges, a hard-sync reset) is corrected with a
// two-sample polynomial band-limited step of the jump's actual height, placed
// at its exact sub-sample time: the sample before the jump gets h*t^2/2, the
// sample after gets -h*(1-t)^2/2, where t is the time from the jump to the
// next sample. Triangle and sine have no jumps of their own (only on sync).
// No latency, realtime-safe.
class Oscillator
{
public:
    void setWave(Wave wave) noexcept { m_wave = wave; }
    Wave wave() const noexcept { return m_wave; }
    void reset(double phase = 0.0) noexcept;

    // One sample: returns the value at the current phase, then advances by
    // `increment` (cycles per sample, 0..0.45). `width`: pulse width 0..1.
    // `syncT` >= 0: a master oscillator wrapped this step, syncT samples before
    // the next sample (in 0..1); this oscillator resets to phase 0 at that instant.
    float next(double increment, double width, double syncT = -1.0) noexcept;

    // After next(): did the phase wrap in this step, and how long before the
    // next sample (the syncT to pass to a slave).
    bool wrapped() const noexcept { return m_wrapped; }
    double wrapT() const noexcept { return m_wrapT; }

    double phase() const noexcept { return m_phase; }

private:
    double naive(double phase, double width) const noexcept;
    // Natural jumps of the waveform for phases in (from, to], `to` - `from` < 1,
    // `to` possibly beyond 1. `after`: time from `to` to the next sample.
    void addJumps(double from, double to, double increment, double width, double after) noexcept;
    void jump(double height, double t) noexcept; // t: time from the jump to the next sample

    Wave m_wave = Wave::Saw;
    double m_phase = 0.0;
    double m_value = 0.0;   // the sample being produced
    double m_pending = 0.0; // correction for the next sample
    bool m_wrapped = false;
    double m_wrapT = 0.0;
    std::uint32_t m_noise = 0x2545F491u;
};

} // namespace krate
