#pragma once

#include <atomic>
#include <cstdint>

#include "dsp/ParamStore.h"
#include "dsp/Sequencer.h"
#include "dsp/SpscQueue.h"
#include "dsp/Voice.h"

namespace k303 {

// GUI -> audio pattern change, applied at the start of the next audio block.
// Edits to the playing pattern take effect when the sequencer next reads that step.
struct Command
{
    enum class Kind : std::uint8_t {
        SetStep,
        SetLength,
        LoadPattern,
        LiveNoteOn,   // play the voice directly; ignored while the sequencer runs
        LiveNoteOff,
    };

    Kind kind = Kind::SetStep;
    std::uint8_t index = 0;    // SetStep: step 0..15; SetLength: 1..16; LiveNoteOn: MIDI note
    Step step{};               // SetStep
    Pattern pattern{};         // LoadPattern
};

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

    // GUI thread (single producer). Returns false if the queue is full.
    bool post(const Command &command) noexcept { return m_commands.push(command); }

    // For tests and offline rendering only: not safe while the audio thread runs.
    const Sequencer &sequencer() const noexcept { return m_sequencer; }

    // GUI thread: peak absolute output since the last call.
    float takePeak() noexcept { return m_peak.exchange(0.0f, std::memory_order_relaxed); }

private:
    void handle(const SequencerEvent &event) noexcept;
    void applyCommands() noexcept;

    ParamStore m_params;
    Sequencer m_sequencer;
    Voice m_voice;
    double m_sampleRate = 48000.0;

    float m_volume = 0.0f;      // smoothed (realtime thread only)
    float m_volumeCoeff = 0.0f;

    SpscQueue<Command, 256> m_commands;

    std::atomic<bool> m_running{false};
    std::atomic<int> m_currentStep{-1};
    std::atomic<float> m_peak{0.0f};
};

} // namespace k303
