#pragma once

#include <atomic>
#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "dsp/Effects.h"
#include "dsp/Reverb.h"
#include "kessel/dsp/Params.h"
#include "kessel/dsp/Sequencer.h"
#include "kessel/dsp/Voices.h"

namespace kessel {

// GUI -> audio thread (docs/SEQUENCER.md#editing-gui--audio).
struct Command
{
    enum class Kind : std::uint8_t {
        Trigger,        // play `drum` now (audition)
        SetStep,         // pattern `slot`: drum's track, `step` = `state`
        SetLength,       // pattern `slot`: `value` steps
        SetStepsPerBeat, // pattern `slot`: `value`
        SetOrder,        // pattern `slot`: `value` (krate::StepClock::Order)
        SetStepDetail,   // pattern `slot`: drum's `step` velocity `value`, `tune`, `decay` (Pattern's encoding)
    };
    Kind kind = Kind::Trigger;
    std::uint8_t slot = 0;     // pattern bank slot 0..7 (A..H)
    std::uint8_t drum = 0;     // DrumId
    bool accent = false;
    float velocity = 1.0f;
    std::uint8_t step = 0;
    StepState state = StepState::Off;
    std::uint8_t value = 0;
    std::uint8_t tune = 0, decay = 0;
};

// Kessel's realtime core (docs/DSP.md, docs/SEQUENCER.md): the voices played by
// the sequencer (following the transport) and by MIDI notes (General MIDI
// drum numbers, docs/BRIEF.md#midi-notes), plus MIDI Learn for the knobs.
// Each voice has a pan, a drive and two sends (docs/DSP.md#mixer-and-effects):
// the reverb (with a gate) and the delay; then the bus compressor and crusher,
// then Volume. The effects' buffers are allocated at construction.
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
    // current knob settings. velocity 0..1; accent adds Total Accent; a
    // tune or decay lock (0..1, < 0 = none) replaces that knob for this hit.
    void trigger(DrumId drum, float velocity, bool accent, float tuneLock = -1.0f, float decayLock = -1.0f) noexcept;

    static constexpr double kFlamSeconds = 0.018; // the main hit after a flam's grace hit
    static constexpr float kFlamGrace = 0.6f;     // the grace hit's velocity, of the main one's
    bool sounding(DrumId drum) const noexcept { return m_voices.sounding(drum); }

    // Any thread: the SAMPLE voice's sample (nullptr: none), taken at the next
    // block. The caller keeps it alive until blocksProcessed() has moved two
    // past the call that replaced it (docs/DSP.md#sample).
    void setSample(const Sample *sample) noexcept { m_pendingSample.store(sample, std::memory_order_release); }
    std::uint64_t blocksProcessed() const noexcept { return m_blocks.load(std::memory_order_acquire); }

    // Any thread: the playing step 0..15, or -1 when stopped (step LEDs); the
    // bank slot playing, and the one waiting for the next pass (-1 = none).
    int currentStep() const noexcept { return m_currentStep.load(std::memory_order_relaxed); }
    int playingSlot() const noexcept { return m_playingSlot.load(std::memory_order_relaxed); }
    int queuedSlot() const noexcept { return m_queuedSlot.load(std::memory_order_relaxed); }

    // Setup, before processing starts (and offline tools): the starting pattern.
    // While running, use postPattern() and post().
    void setSequencerEnabled(bool on) noexcept override { m_sequencerEnabled.store(on, std::memory_order_relaxed); }
    void setSequencerSuspended(bool on) noexcept override { m_sequencerSuspended.store(on, std::memory_order_relaxed); }
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
    void updateMix() noexcept;  // the voices' pan, drive and sends, once per block
    void renderVoices(float *left, float *right, std::uint32_t frames) noexcept; // and the send effects
    void processBus(float *left, float *right, std::uint32_t frames) noexcept;   // compressor, crusher

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
    std::atomic<bool> m_sequencerEnabled{true};
    std::atomic<bool> m_sequencerSuspended{false};
    std::atomic<int> m_playingSlot{0};
    std::atomic<int> m_queuedSlot{-1};
    double m_sampleRate = 48000.0;
    std::atomic<const Sample *> m_pendingSample{nullptr};
    std::atomic<std::uint64_t> m_blocks{0};

    // Send effects (docs/DSP.md#mixer-and-effects); buses rendered kChunk frames at a time.
    static constexpr std::uint32_t kChunk = 256;
    std::array<float, kChunk> m_revL{}, m_revR{}, m_dlyL{}, m_dlyR{}, m_dryL{}, m_dryR{};
    krate::Reverb m_reverb{192000.0};
    krate::Delay m_delay;
    bool m_reverbOn = false, m_delayOn = false;
    double m_delaySeconds = 0.5;
    double m_gateGain = 0.0;        // the reverb gate, 0..1
    int m_gateHold = 0;             // samples left open
    double m_gateAttack = 0.0, m_gateRelease = 0.0;
    // Bus compressor and crusher.
    double m_compEnvelope = 0.0;    // detected peak, linear
    double m_compGain = 1.0;        // applied gain, linear
    double m_compSmoothing = 0.0;
    double m_crushPhase = 1.0;
    float m_crushL = 0.0f, m_crushR = 0.0f;

    // Flams waiting for their main hit.
    struct PendingFlam
    {
        int frames = 0; // to go; 0 = none
        float velocity = 0.0f, tune = -1.0f, decay = -1.0f;
        bool accent = false;
    };
    std::array<PendingFlam, DrumCount> m_flams{};
    int m_flamsPending = 0;

    double m_volume = -1.0;          // smoothed output gain; < 0 = not yet primed
    double m_volumeSmoothing = 0.0;  // per-sample one-pole coefficient
};

} // namespace Kessel
