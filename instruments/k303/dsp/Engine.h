#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/SpscQueue.h"
#include "dsp/ParamStore.h"
#include "dsp/Sequencer.h"
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

using krate::MidiEvent;

// Audio -> GUI notification. (MIDI Learn notices go through krate::MidiLearnMap.)
struct EngineEvent
{
    enum class Kind : std::uint8_t { NoteOn };

    Kind kind = Kind::NoteOn;
    std::uint8_t a = 0;   // NoteOn: MIDI note (for step record, later)
    std::uint8_t b = 0;   // NoteOn: velocity
};

// Everything the audio callback renders. Host-agnostic: JACK today, LV2 later.
// process() runs on the realtime thread and must not allocate, lock or block.
//
// Sequencer -> Voice -> volume. No limiting: the host's master stage soft-clips
// (docs/SHARED_FOUNDATIONS.MD). Time comes from the block's
// transport: while it plays, the sequencer plays its pattern; while stopped
// (or with no transport), MIDI keys play the voice. The Tempo parameter is not
// read here: the host's transport carries the tempo. MIDI CCs move
// mapped parameters (docs/MIDI.md).
class Engine final : public krate::IInstrument
{
public:
    Engine();

    // krate::IInstrument
    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    float param(int index) const noexcept override;
    void setParam(int index, float normalized) noexcept override;
    void setParamFromHost(int index, float normalized) noexcept override;
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChangedByAudio(); }
    void process(const krate::AudioBlock &block) noexcept override;
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }

    // k303-specific, any thread.
    ParamStore &params() noexcept { return m_params; }
    int currentStep() const noexcept { return m_currentStep.load(std::memory_order_relaxed); }
    // True while the playing step starts a beat (beat LED).
    bool onBeat() const noexcept { return m_onBeat.load(std::memory_order_relaxed); }

    // GUI thread (single producer). Returns false if the queue is full.
    bool post(const Command &command) noexcept { return m_commands.push(command); }

    // Any thread. Off: the sequencer doesn't follow the transport and MIDI keys
    // play the voice while it plays too (a song track playing k303). On by default.
    void setSequencerEnabled(bool on) noexcept { m_sequencerEnabled.store(on, std::memory_order_relaxed); }
    bool sequencerEnabled() const noexcept { return m_sequencerEnabled.load(std::memory_order_relaxed); }

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

    krate::SpscQueue<Command, 256> m_commands;
    krate::SpscQueue<EngineEvent, 256> m_events;
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values

    // MIDI keys held down, oldest first (realtime thread only). Last-note priority.
    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;

    std::atomic<int> m_currentStep{-1};
    std::atomic<bool> m_onBeat{false};
    std::atomic<bool> m_sequencerEnabled{true};
    std::atomic<float> m_peak{0.0f};
};

} // namespace k303
