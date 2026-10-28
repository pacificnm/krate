#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

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

// A raw MIDI message (1..3 bytes) at a frame offset inside the current block.
struct MidiEvent
{
    std::uint32_t frame = 0;
    std::uint8_t size = 0;
    std::uint8_t data[3] = {0, 0, 0};
};

// Audio -> GUI notification.
struct EngineEvent
{
    enum class Kind : std::uint8_t { CcLearned, NoteOn };

    Kind kind = Kind::CcLearned;
    std::uint8_t a = 0;   // CcLearned: CC number; NoteOn: MIDI note
    std::uint8_t b = 0;   // CcLearned: ParamId; NoteOn: velocity
};

// Everything the audio callback renders. Host-agnostic: JACK today, LV2 later.
// process() runs on the realtime thread and must not allocate, lock or block.
//
// Sequencer -> Voice -> volume -> soft clip. While running, the sequencer
// plays its pattern; while stopped, MIDI keys play the voice. MIDI CCs move
// mapped parameters (docs/MIDI.md).
class Engine
{
public:
    Engine();

    // Realtime thread (or before processing starts).
    void setSampleRate(double sampleRate) noexcept;
    // events: sorted by frame, each frame < frames.
    void process(float *outL, float *outR, std::uint32_t frames,
                 const MidiEvent *events = nullptr, std::uint32_t eventCount = 0) noexcept;

    // Any thread.
    ParamStore &params() noexcept { return m_params; }
    void setRunning(bool running) noexcept { m_running.store(running, std::memory_order_relaxed); }
    bool isRunning() const noexcept { return m_running.load(std::memory_order_relaxed); }
    int currentStep() const noexcept { return m_currentStep.load(std::memory_order_relaxed); }

    // GUI thread (single producer). Returns false if the queue is full.
    bool post(const Command &command) noexcept { return m_commands.push(command); }

    // MIDI Learn (any thread). Params are ParamId values; -1 = none.
    void setLearnTarget(int param) noexcept { m_learnTarget.store(std::int16_t(param), std::memory_order_relaxed); }
    int learnTarget() const noexcept { return m_learnTarget.load(std::memory_order_relaxed); }
    void setCcMapping(int cc, int param) noexcept;
    int ccMapping(int cc) const noexcept;

    // GUI thread (single consumer).
    std::optional<EngineEvent> popEvent() noexcept { return m_events.pop(); }

    // For tests and offline rendering only: not safe while the audio thread runs.
    const Sequencer &sequencer() const noexcept { return m_sequencer; }

    // GUI thread: peak absolute output since the last call.
    float takePeak() noexcept { return m_peak.exchange(0.0f, std::memory_order_relaxed); }

private:
    void handle(const SequencerEvent &event) noexcept;
    void applyCommands() noexcept;
    void handleMidi(const MidiEvent &event) noexcept;
    void midiNoteOn(int note, int velocity) noexcept;
    void midiNoteOff(int note) noexcept;

    ParamStore m_params;
    Sequencer m_sequencer;
    Voice m_voice;
    double m_sampleRate = 48000.0;

    float m_volume = 0.0f;      // smoothed (realtime thread only)
    float m_volumeCoeff = 0.0f;

    SpscQueue<Command, 256> m_commands;
    SpscQueue<EngineEvent, 256> m_events;

    // MIDI keys held down, oldest first (realtime thread only). Last-note priority.
    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;

    std::array<std::atomic<std::int16_t>, 128> m_ccMap; // CC -> ParamId, -1 = unmapped
    std::atomic<std::int16_t> m_learnTarget{-1};

    std::atomic<bool> m_running{false};
    std::atomic<int> m_currentStep{-1};
    std::atomic<float> m_peak{0.0f};
};

} // namespace k303
