#pragma once

#include <atomic>
#include <cstdint>

#include "dsp/Oscillator.h"
#include "dsp/ParamStore.h"

namespace k303 {

// Everything the audio callback renders. Host-agnostic: JACK today, LV2 later.
// process() runs on the realtime thread and must not allocate, lock or block.
//
// M2: plays a test saw while running; Tuning, Waveform and Volume act on it.
class Engine
{
public:
    Engine();

    // Realtime thread (or before processing starts).
    void setSampleRate(double sampleRate) noexcept;
    void process(float *outL, float *outR, std::uint32_t frames) noexcept;

    // Any thread.
    ParamStore &params() noexcept { return m_params; }
    void setRunning(bool running) noexcept { m_running.store(running, std::memory_order_relaxed); }
    bool isRunning() const noexcept { return m_running.load(std::memory_order_relaxed); }

    // GUI thread: peak absolute output since the last call.
    float takePeak() noexcept { return m_peak.exchange(0.0f, std::memory_order_relaxed); }

private:
    ParamStore m_params;
    Oscillator m_osc;
    double m_sampleRate = 48000.0;

    // Smoothed values (realtime thread only)
    float m_runGain = 0.0f;
    float m_runCoeff = 0.0f;
    double m_semitones = 0.0;
    float m_volume = 0.0f;
    float m_paramCoeff = 0.0f;

    std::atomic<bool> m_running{false};
    std::atomic<float> m_peak{0.0f};
};

} // namespace k303
