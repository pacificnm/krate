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
#include "dsp/Lfo.h"
#include "kalliope/dsp/Params.h"
#include "kalliope/dsp/Sequence.h"
#include "kalliope/dsp/Voice.h"

namespace kalliope {

using krate::Chorus;
using krate::Delay;
using krate::Lfo;

// Kalliope's realtime core (docs/DSP.md): eight voices at 2x, decimated once,
// then the stereo chorus and the tempo-synced ping-pong delay; Poly, Unison and
// Solo key modes (docs/PLAY.md#key-modes), the arpeggiator and the step
// sequencer on the transport (docs/PLAY.md#arpeggiator, docs/SEQUENCER.md),
// sustain pedal, pitch bend, mod wheel, MIDI Learn. The delay's buffer is
// allocated here, at construction.
//
// Keys (what is held) and voice notes (what sounds) are separate. Play Mode
// Note: keys play voices. Arpeggio: keys feed the arpeggiator, which plays the
// voices. Sequencer: the pattern plays the voices while the transport runs and
// keys transpose it; stopped, keys play voices (to hear what you record).
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

    float param(ParamId id) const noexcept { return m_params.get(int(id)); }
    void setParam(ParamId id, float normalized) noexcept { m_params.set(int(id), normalized); }

    // The panel's settings in DSP units (what the voices play with).
    VoiceSettings settings() const noexcept;

    static constexpr int kVoices = 8;
    enum class KeyMode { Poly, Unison, Solo };

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

    // Inspection (tests): what each voice is doing.
    bool voiceActive(int v) const noexcept { return m_voices[std::size_t(v)].active(); }
    bool voiceGate(int v) const noexcept { return m_voices[std::size_t(v)].gate(); }
    int voiceNote(int v) const noexcept { return m_voices[std::size_t(v)].note(); }
    double voiceLevel(int v) const noexcept { return m_voices[std::size_t(v)].level(); }
    int activeVoices() const noexcept;
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
    KeyMode keyMode() const noexcept;
    void applyKeyMode(KeyMode mode) noexcept;
    int pickVoice(int note) noexcept; // Poly: which voice plays `note`
    void playMono(bool retrigger) noexcept; // Unison/Solo: the voices follow the top held key
    void releaseVoice(int v) noexcept;      // key up: release, or hold for the pedal
    void releaseAllVoices() noexcept;       // every voice to release (keys stay held)
    void voiceOn(int note) noexcept;        // Poly: a voice for `note` (stealing if needed)
    void voiceOff(int note) noexcept;       // Poly: release `note`'s voice
    void arpUpdate(const krate::TransportState &transport) noexcept; // once per block: play mode, clock
    void arpStep() noexcept;                // a step starts: the next note
    void arpRelease() noexcept;             // the gate ends (or the arpeggio stops)
    void seqStep(int step) noexcept;        // a sequencer step starts
    void seqRelease() noexcept;             // the sequencer's notes end
    void playNotes(const int *notes, int count) noexcept; // through the key mode
    void releaseNotes(const int *notes, int count) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values
    double m_sampleRate = 48000.0;

    std::array<Voice, kVoices> m_voices;
    std::array<std::uint64_t, kVoices> m_started{};  // note-on order, for stealing
    std::array<bool, kVoices> m_sustained{};         // key up, held by the pedal
    std::uint64_t m_counter = 0;
    KeyMode m_mode = KeyMode::Poly;
    bool m_sustain = false;
    std::uint32_t m_phaseSeed = 0x9E3779B9u; // start phases of new Poly notes (deterministic)
    Lfo m_lfo;
    Chorus m_chorus;
    Delay m_delay;
    krate::HalfbandDecimator<31> m_decimator;
    std::array<float, 2 * kChunk> m_buffer2x{};
    std::array<float, kChunk> m_lfoValues{};

    // Held keys, oldest first (last-note priority).
    struct GuiNote
    {
        std::uint8_t note, velocity;
    };
    krate::SpscQueue<GuiNote, 128> m_guiNotes;
    krate::SpscQueue<std::uint8_t, 16> m_programs; // audio -> GUI
    std::array<std::atomic<std::uint64_t>, 2> m_heldMask{};
    std::atomic<bool> m_sequencerEnabled{true}; // the step sequencer (the arpeggiator always runs)
    void publishHeld() noexcept;

    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;
    // Octave: each MIDI key's shift as it went down, so its note-off matches
    // even if Octave changed while it was held (-128 = not down).
    std::array<std::int8_t, 128> m_keyShift{};

    // Arpeggiator (docs/PLAY.md#arpeggiator).
    krate::StepClock m_arpClock; // the arpeggiator's and the sequencer's (never both at once)
    PlayMode m_playMode = PlayMode::Note;
    bool m_arpOn = false;        // m_playMode == Arpeggio
    bool m_hold = false;
    std::array<std::uint8_t, 16> m_latched{}; // keys kept by Hold
    int m_latchedCount = 0;
    int m_arpNote = -1;
    std::uint32_t m_arpPosition = 0;
    std::uint32_t m_arpRandom = 0x2F6B3A1Du;

    // Step sequencer (docs/SEQUENCER.md).
    krate::SpscQueue<SeqPattern, 8> m_sequences;
    SeqPattern m_sequence;
    std::array<int, 4> m_seqNotes{}; // sounding now
    int m_seqCount = 0;
    int m_transpose = 0;
    std::atomic<int> m_seqPlayingStep{-1};
    float m_lastVelocity = 1.0f;
    double m_bend = 0.0;  // -1..1
    double m_wheel = 0.0; // 0..1
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;
};

} // namespace kalliope
