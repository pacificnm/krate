#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "dsp/HalfbandDecimator.h"
#include "krimson/dsp/Params.h"
#include "krimson/dsp/Voice.h"

namespace krimson {

// Krimson's realtime core (docs/DSP.md). Four slots, each a full set of sound
// parameters and its own key state (held keys, sustain, bend, wheel, voice
// mode, glide), sharing 16 voices at 2x. The IInstrument parameter table shows
// the slot being edited (docs/BRIEF.md#parameters-one-slot-at-a-time).
//
// MIDI routing (docs/PLAY.md#slots): a channel that belongs to a slot (and the
// slot is on) goes to that slot; any other channel is the keyboard, whose notes
// go to the lit slots, split by the split key when one is set.
class Engine final : public krate::IInstrument
{
public:
    Engine();

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    // The edited slot's value for slot parameters, the instrument's for the rest.
    float param(int index) const noexcept override { return store(index).get(index); }
    void setParam(int index, float normalized) noexcept override { store(index).set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { store(index).setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override;
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }
    void process(const krate::AudioBlock &block) noexcept override;

    // Any thread: which slot (0..3 = A..D) the parameter table shows. Changing
    // it reports every slot parameter as changed, so the panel follows.
    void setEditedSlot(int slot) noexcept;
    int editedSlot() const noexcept { return m_edited.load(std::memory_order_relaxed); }
    // Any thread: report every parameter as changed (after loading slots from a file).
    void markAllChanged() noexcept { m_forcedChanges.store(~std::uint64_t(0), std::memory_order_relaxed); }

    // Any thread: a given slot's parameter (tests, slot files, the voices).
    float slotParam(int slot, ParamId id) const noexcept { return m_slots[std::size_t(slot)].get(int(id)); }
    void setSlotParam(int slot, ParamId id, float v) noexcept { m_slots[std::size_t(slot)].set(int(id), v); }
    float param(ParamId id) const noexcept { return param(int(id)); }
    void setParam(ParamId id, float v) noexcept { setParam(int(id), v); }

    // Slot setup (any thread; read by the audio thread every event).
    enum class Zone : std::uint8_t { Whole, Lower, Upper }; // which part of a split keyboard a slot plays
    void setSlotChannel(int slot, int channel) noexcept;  // 0..15, or -1 for none
    int slotChannel(int slot) const noexcept { return m_channel[std::size_t(slot)].load(std::memory_order_relaxed); }
    void setSlotOn(int slot, bool on) noexcept { m_on[std::size_t(slot)].store(on, std::memory_order_relaxed); }
    bool slotOn(int slot) const noexcept { return m_on[std::size_t(slot)].load(std::memory_order_relaxed); }
    void setSlotZone(int slot, Zone zone) noexcept { m_zone[std::size_t(slot)].store(zone, std::memory_order_relaxed); }
    Zone slotZone(int slot) const noexcept { return m_zone[std::size_t(slot)].load(std::memory_order_relaxed); }
    void setLitSlots(unsigned mask) noexcept { m_lit.store(mask & 0xFu, std::memory_order_relaxed); }
    unsigned litSlots() const noexcept { return m_lit.load(std::memory_order_relaxed); }
    void setSplitNote(int note) noexcept { m_split.store(note, std::memory_order_relaxed); } // -1 = no split
    int splitNote() const noexcept { return m_split.load(std::memory_order_relaxed); }

    // A slot's sound in DSP units: at its panel values, or (atMorph) with every
    // morph target in place of its parameter.
    SlotSettings settings(int slot, bool atMorph = false) const noexcept;

    // Morph targets (docs/DSP.md#morph), any thread: a continuous slot
    // parameter's value at full morph, or -1 for none. Detented parameters
    // (waves, types, switches) don't morph: setting one is ignored.
    void setMorph(int slot, ParamId id, float target) noexcept;
    float morph(int slot, ParamId id) const noexcept
    {
        return m_morph[std::size_t(slot)][std::size_t(id)].load(std::memory_order_relaxed);
    }
    void clearMorph(int slot) noexcept;
    bool hasMorph(int slot) const noexcept;
    static bool morphable(ParamId id) noexcept { return isSlotParam(int(id)) && kParams[std::size_t(id)].detents == 0; }

    // GUI thread: the on-screen keyboard and wheels, played as the keyboard
    // (the lit slots), at the next block. status: 0x90 note on, 0x80 note off,
    // 0xB0 control change, 0xE0 pitch bend. False if the queue is full.
    bool postKeyboard(std::uint8_t status, std::uint8_t d1, std::uint8_t d2) noexcept
    {
        return m_gui.push({0, 3, {status, d1, d2}});
    }
    // Any thread: is `note` held in any slot? Updated every block.
    bool noteHeld(int note) const noexcept
    {
        if (note < 0 || note > 127)
            return false;
        return (m_heldMask[std::size_t(note / 64)].load(std::memory_order_relaxed) >> (note % 64)) & 1u;
    }

    static constexpr int kVoices = 16;
    static constexpr int kUnisonVoices = 3; // voices stacked per note in Unison
    enum class VoiceMode { Poly, Mono, Legato, Unison };

    // Inspection (tests).
    int activeVoices() const noexcept;
    bool voiceActive(int v) const noexcept { return m_voices[std::size_t(v)].active(); }
    bool voiceGate(int v) const noexcept { return m_voices[std::size_t(v)].gate(); }
    int voiceNote(int v) const noexcept { return m_voices[std::size_t(v)].note(); }
    int voiceSlot(int v) const noexcept { return m_voiceSlot[std::size_t(v)]; }
    double voiceLevel(int v) const noexcept { return m_voices[std::size_t(v)].level(); }

private:
    using Store = krate::ParamValues<ParamCount>;
    Store &store(int index) noexcept { return isSlotParam(index) ? m_slots[std::size_t(editedSlot())] : m_global; }
    const Store &store(int index) const noexcept
    {
        return isSlotParam(index) ? m_slots[std::size_t(editedSlot())] : m_global;
    }

    // One slot's key state (audio thread only).
    struct SlotState
    {
        std::array<std::uint8_t, 16> held{}; // keys down, oldest first
        int heldCount = 0;
        bool sustain = false;
        double bend = 0.0;  // -1..1
        double wheel = 0.0; // 0..1
        int lastNote = -1;  // for glide
        float velocity = 1.0f;
        VoiceMode mode = VoiceMode::Poly;
        std::array<int, kUnisonVoices> monoVoices{-1, -1, -1}; // Mono / Legato / Unison: the voices it plays
    };

    // keyboard: from the on-screen keyboard (the lit slots, whatever the channel; no Octave shift).
    void handleMidi(const krate::MidiEvent &event, bool keyboard = false) noexcept;
    void publishHeld() noexcept;
    // The slots a message on `channel` (about `note`, or -1) reaches, as a bit mask
    // (channel -1: the keyboard).
    unsigned slotsFor(int channel, int note) const noexcept;
    void noteOn(int slot, int note, int velocity) noexcept;
    void noteOff(int slot, int note) noexcept;
    void setSustain(int slot, bool down) noexcept;
    void allNotesOff(int slot) noexcept;
    void render(float *left, float *right, std::uint32_t frames) noexcept;
    VoiceMode voiceMode(int slot) const noexcept;
    int pickVoice(int slot, int note) noexcept; // a voice for the note (stealing if needed)
    void startVoice(int slot, int v, int note, double glideFrom) noexcept;
    void playMono(int slot, bool retrigger) noexcept; // Mono / Legato / Unison: follow the top held key
    void releaseVoice(int slot, int v) noexcept;      // key up: release, or hold for the pedal

    static constexpr std::uint32_t kChunk = 128;

    std::array<Store, SlotCount> m_slots{Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams}};
    Store m_global{kParams};
    std::atomic<int> m_edited{0};
    std::atomic<std::uint64_t> m_forcedChanges{0};
    krate::MidiLearnMap m_learn{kParams};
    double m_sampleRate = 48000.0;

    // Slot setup.
    std::array<std::atomic<int>, SlotCount> m_channel{};
    std::array<std::atomic<bool>, SlotCount> m_on{};
    std::array<std::atomic<Zone>, SlotCount> m_zone{};
    std::atomic<unsigned> m_lit{1};
    std::atomic<int> m_split{-1};

    std::array<SlotState, SlotCount> m_state{};
    std::array<Voice, kVoices> m_voices;
    std::array<int, kVoices> m_voiceSlot{};          // which slot each voice plays
    std::array<std::uint64_t, kVoices> m_started{};  // note-on order, for stealing
    std::array<bool, kVoices> m_sustained{};         // key up, held by the pedal
    std::uint64_t m_counter = 0;
    std::array<Lfo, SlotCount> m_lfo2;   // per slot
    Lfo m_vibrato;                       // the mod wheel's vibrato
    krate::HalfbandDecimator<31> m_decimatorL, m_decimatorR;
    std::array<float, 2 * kChunk> m_bufferL{}, m_bufferR{};
    std::array<std::array<float, kChunk>, SlotCount> m_lfo2Values{};
    std::array<float, kChunk> m_vibratoValues{};
    std::array<SlotSettings, SlotCount> m_settings{};      // at the panel values
    std::array<SlotSettings, SlotCount> m_morphSettings{}; // at the morph targets
    std::array<bool, SlotCount> m_morphing{};              // the slot has morph targets
    std::array<bool, SlotCount> m_wheelMorph{};            // the wheel morphs (else velocity)
    std::array<std::array<std::atomic<float>, SlotParamCount>, SlotCount> m_morph{};
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;

    krate::SpscQueue<krate::MidiEvent, 256> m_gui;
    std::array<std::atomic<std::uint64_t>, 2> m_heldMask{};
    // Octave: the shift each MIDI key went down with, per channel and note, so
    // its note-off matches even if Octave changed while it was held (-128 = up).
    std::array<std::array<std::int8_t, 128>, 16> m_keyShift{};
};

} // namespace krimson
