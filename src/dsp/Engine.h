#pragma once

#include <atomic>
#include <cstdint>

#include "dsp/ParamStore.h"
#include "dsp/Sequencer.h"
#include "dsp/Voice.h"

namespace k303 {

// Everything the audio callback renders. Host-agnostic: JACK today, LV2 later.
// process() runs on the realtime thread and must not allocate, lock or block.
//
// Sequencer -> Voice -> volume -> soft clip. While running, the sequencer
// plays its pattern (the demo riff until pattern editing lands in M5).
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
    int currentStep() const noexcept { return m_currentStep.load(std::memory_order_relaxed); }

    // GUI thread: peak absolute output since the last call.
    float takePeak() noexcept { return m_peak.exchange(0.0f, std::memory_order_relaxed); }

private:
    void handle(const SequencerEvent &event) noexcept;

    ParamStore m_params;
    Sequencer m_sequencer;
    Voice m_voice;
    double m_sampleRate = 48000.0;

    float m_volume = 0.0f;      // smoothed (realtime thread only)
    float m_volumeCoeff = 0.0f;

    std::atomic<bool> m_running{false};
    std::atomic<int> m_currentStep{-1};
    std::atomic<float> m_peak{0.0f};
};

} // namespace k303
