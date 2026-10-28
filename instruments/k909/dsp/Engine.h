#pragma once

#include <atomic>
#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "k909/dsp/Params.h"
#include "k909/dsp/Sequencer.h"
#include "k909/dsp/Voices.h"

namespace k909 {

// GUI -> audio thread (docs/SEQUENCER.md#editing-gui--audio).
struct Command
{
    enum class Kind : std::uint8_t {
        Trigger,        // play `drum` now (audition)
        SetStep,         // pattern `slot`: drum's track, `step` = `state`
        SetLength,       // pattern `slot`: `value` steps
        SetStepsPerBeat, // pattern `slot`: `value`
        SetOrder,        // pattern `slot`: `value` (krate::StepClock::Order)
    };
    Kind kind = Kind::Trigger;
    std::uint8_t slot = 0;     // pattern bank slot 0..7 (A..H)
    std::uint8_t drum = 0;     // DrumId
    bool accent = false;
    float velocity = 1.0f;
    std::uint8_t step = 0;
    StepState state = StepState::Off;
    std::uint8_t value = 0;
};

// K909's realtime core (docs/DSP.md, docs/SEQUENCER.md): eleven voices played
// by the sequencer (following the transport) and by MIDI notes (General MIDI
// drum numbers, docs/BRIEF.md#midi-notes), plus MIDI Learn for the knobs.
class Engine final : public krate::IInstrument
{
public:
    Engine() = default;

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float normalized) noexcept override { m_params.set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { m_params.setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }
    void process(const krate::AudioBlock &block) noexcept override;

    float param(ParamId id) const noexcept { return m_params.get(int(id)); }

    // Audio thread (or offline, between blocks): play a voice now, with the
    // current knob settings. velocity 0..1; accent adds Total Accent.
    void trigger(DrumId drum, float velocity, bool accent) noexcept;
    bool sounding(DrumId drum) const noexcept { return m_voices.sounding(drum); }

    // Any thread: the playing step 0..15, or -1 when stopped (step LEDs); the
    // bank slot playing, and the one waiting for the next pass (-1 = none).
    int currentStep() const noexcept { return m_currentStep.load(std::memory_order_relaxed); }
    int playingSlot() const noexcept { return m_playingSlot.load(std::memory_order_relaxed); }
    int queuedSlot() const noexcept { return m_queuedSlot.load(std::memory_order_relaxed); }

    // Setup, before processing starts (and offline tools): the starting pattern.
    // While running, use postPattern() and post().
    void setPattern(const Pattern &pattern, int slot = 0) noexcept;
    const Pattern &pattern() const noexcept { return m_sequencer.pattern(); }

    // GUI thread: play bank slot `slot` (its whole pattern) from the next pass,
    // or at once while stopped. False if the queue is full.
    bool postPattern(const Pattern &pattern, int slot) noexcept { return m_patternLoads.push({pattern, slot, false}); }
    // GUI thread: new contents for slot `slot` if it is playing or queued (Clear).
    bool postReplace(const Pattern &pattern, int slot) noexcept { return m_patternLoads.push({pattern, slot, true}); }

    // GUI thread: queue a command for the next block. False if the queue is full.
    bool post(const Command &command) noexcept { return m_commands.push(command); }

    // The voice a MIDI note plays, or DrumId::Count for none.
    static DrumId drumForNote(int note) noexcept;

private:
    void handleMidi(const krate::MidiEvent &event) noexcept;
    void apply(const Command &command) noexcept;
    void play(const Hits &hits) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values
    Voices m_voices;
    struct PatternLoad
    {
        Pattern pattern;
        int slot;
        bool replace; // contents only; false = select (queuePattern)
    };
    krate::SpscQueue<Command, 256> m_commands;
    krate::SpscQueue<PatternLoad, 16> m_patternLoads;
    Sequencer m_sequencer;
    std::atomic<int> m_currentStep{-1};
    std::atomic<int> m_playingSlot{0};
    std::atomic<int> m_queuedSlot{-1};
    double m_sampleRate = 48000.0;
    double m_volume = -1.0;          // smoothed output gain; < 0 = not yet primed
    double m_volumeSmoothing = 0.0;  // per-sample one-pole coefficient
};

} // namespace k909
