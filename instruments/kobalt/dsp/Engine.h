#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "core/StepClock.h"
#include "dsp/Effects.h"
#include "dsp/HalfbandDecimator.h"
#include "dsp/Reverb.h"
#include "kobalt/dsp/Params.h"
#include "kobalt/dsp/Sequence.h"
#include "kobalt/dsp/Voice.h"

namespace kobalt {

// Kobalt's realtime core (docs/BRIEF.md): one voice at 2x, decimated, then
// the tempo-synced ping-pong delay and the reverb. One note at a time with
// last-note priority: a key going down plays legato if another is still
// held, and releasing the top key goes back to the newest key still held.
// Sustain pedal, pitch bend, mod wheel, MIDI Learn. The delay's and the
// reverb's buffers are allocated here, at construction.
//
// Keys (what is held) and the voice's notes (what sounds) are separate. Play
// Mode Note: keys play the voice. Arpeggio: keys feed the arpeggiator, which
// plays the voice. Sequencer: the pattern plays the voice while the transport
// runs and keys transpose it; stopped, keys play the voice (to hear what you load).
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
    void setSequencerEnabled(bool on) noexcept override { m_sequencerEnabled.store(on, std::memory_order_relaxed); }
    void setSequencerSuspended(bool on) noexcept override { m_sequencerSuspended.store(on, std::memory_order_relaxed); }

    float param(ParamId id) const noexcept { return m_params.get(int(id)); }
    void setParam(ParamId id, float normalized) noexcept { m_params.set(int(id), normalized); }

    // The panel's settings in DSP units (what the voice plays with), with the
    // bender and the lever as they are now.
    VoiceSettings settings() const noexcept;

    // GUI thread: play a note from the on-screen keyboard (applied at the next
    // block). velocity 0 = note off. False if the queue is full.
    bool postNote(int note, int velocity) noexcept
    {
        return m_guiNotes.push({std::uint8_t(note), std::uint8_t(velocity)});
    }
    // Any thread: is `note` held (by MIDI or the on-screen keyboard)? Updated every block.
    bool noteHeld(int note) const noexcept
    {
        if (note < 0 || note > 127)
            return false;
        return (m_heldMask[std::size_t(note / 64)].load(std::memory_order_relaxed) >> (note % 64)) & 1u;
    }

    // GUI thread: Program Changes received (0..127), for the patch controller.
    std::optional<int> popProgram() noexcept
    {
        if (const auto p = m_programs.pop())
            return int(*p);
        return std::nullopt;
    }

    // Inspection (tests).
    const Voice &voice() const noexcept { return m_voice; }
    // The note the arpeggiator is playing, or -1 (tests, display).
    int arpNote() const noexcept { return m_arpNote; }

    enum class PlayMode { Note, Arpeggio, Sequencer };

    // GUI thread: the sequencer's pattern (applied at the next block; a new
    // length takes effect from the next pass). False if the queue is full.
    bool postSequence(const SeqPattern &pattern) noexcept { return m_sequences.push(pattern); }
    // Setup (before processing) and tests.
    void setSequence(const SeqPattern &pattern) noexcept { m_sequence = pattern; }
    // Any thread: the sequencer step playing, or -1; the transposition (semitones).
    int sequencerStep() const noexcept { return m_seqPlayingStep.load(std::memory_order_relaxed); }
    int transpose() const noexcept { return m_transpose; }

private:
    static constexpr std::uint32_t kChunk = 128; // frames rendered at a time (fixed buffers)

    void handleMidi(const krate::MidiEvent &event) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void setSustain(bool down) noexcept;
    void allNotesOff() noexcept;
    void render(float *out, std::uint32_t frames, const VoiceSettings &s) noexcept;
    VoiceSettings baseSettings() const noexcept;     // settings() but for the bender and the lever
    void applyLive(VoiceSettings &s) const noexcept; // bend and wheel, as they are now
    void publishHeld() noexcept;
    void playUpdate(const krate::TransportState &transport) noexcept; // once per block: play mode, clock
    void arpStep() noexcept;      // an arpeggio step starts: the next note
    void arpRelease() noexcept;   // its gate ends (or the arpeggio stops)
    void seqStep(int step) noexcept;
    void seqRelease() noexcept;   // the sequencer's note ends
    void playKeys() noexcept;     // back to Note: the top key held sounds again

    krate::ParamValues<ParamCount> m_params{kParams};
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values
    double m_sampleRate = 48000.0;

    Voice m_voice;
    VoiceSettings m_settings; // this block's, with the bender as of the last event
    Lfo m_lfo;
    krate::Delay m_delay;
    krate::Reverb m_reverb{192000.0};
    bool m_reverbOn = false;
    krate::HalfbandDecimator<31> m_decimator;
    std::array<float, 2 * kChunk> m_buffer2x{};
    std::array<float, kChunk> m_lfoValues{};
    std::array<bool, kChunk> m_lfoWrapped{};

    struct GuiNote
    {
        std::uint8_t note, velocity;
    };
    krate::SpscQueue<GuiNote, 128> m_guiNotes;
    krate::SpscQueue<std::uint8_t, 16> m_programs; // audio -> GUI
    std::array<std::atomic<std::uint64_t>, 2> m_heldMask{};

    // Held keys, oldest first (last-note priority).
    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;
    // Octave: each MIDI key's shift as it went down, so its note-off matches
    // even if Octave changed while it was held (-128 = not down).
    std::array<std::int8_t, 128> m_keyShift{};
    bool m_sustain = false;
    bool m_sustained = false; // keys up, the note held by the pedal
    float m_lastVelocity = 1.0f;
    double m_bend = 0.0;  // -1..1
    double m_wheel = 0.0; // 0..1
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;

    // Arpeggiator and step sequencer, on one clock (never both at once).
    krate::StepClock m_clock;
    PlayMode m_playMode = PlayMode::Note;
    std::atomic<bool> m_sequencerEnabled{true}; // the step sequencer (the arpeggiator always runs)
    std::atomic<bool> m_sequencerSuspended{false};
    bool m_hold = false;
    std::array<std::uint8_t, 16> m_latched{}; // keys kept by Hold
    int m_latchedCount = 0;
    int m_arpNote = -1;
    std::uint32_t m_arpPosition = 0;
    krate::SpscQueue<SeqPattern, 8> m_sequences;
    SeqPattern m_sequence;
    int m_seqNote = -1;         // sounding now
    bool m_seqLegato = false;   // the step that played it runs into the next note
    int m_transpose = 0;
    std::atomic<int> m_seqPlayingStep{-1};
};

} // namespace kobalt
