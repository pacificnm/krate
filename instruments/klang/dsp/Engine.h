#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "dsp/Effects.h"
#include "dsp/Reverb.h"
#include "klang/dsp/Params.h"
#include "klang/dsp/Sample.h"
#include "klang/dsp/Voice.h"

namespace klang {

// Klang's realtime core (docs/BRIEF.md): 16 pads, each with two voices (a new
// hit fades the one before out while it starts), choke groups, the trance
// gate, the delay and reverb on the pads' sends, then Volume. Notes 36..51 hit
// pads 1..16; with Keys on Chromatic every key plays the edited pad at its
// pitch (C4 as recorded). The delay's and the reverb's buffers are allocated
// here, at construction; samples are handed over by pointer (setSample).
class Engine final : public krate::IInstrument
{
public:
    static constexpr int kPads = 16;
    static constexpr int kFirstPadNote = 36; // C2: pad 1
    static constexpr int kChromaticRoot = 60; // C4: a sample's own pitch

    Engine() = default;

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    // The edited pad's value for pad parameters, Klang's for the rest.
    float param(int index) const noexcept override;
    void setParam(int index, float normalized) noexcept override;
    void setParamFromHost(int index, float normalized) noexcept override;
    std::uint64_t takeParamsChangedByAudio() noexcept override;
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }
    void process(const krate::AudioBlock &block) noexcept override;

    float param(ParamId id) const noexcept { return param(int(id)); }
    void setParam(ParamId id, float v) noexcept { setParam(int(id), v); }

    // Any thread: which pad (0..15) the parameter table shows. Changing it
    // reports every pad parameter as changed, so the panel follows.
    void setEditedPad(int pad) noexcept;
    int editedPad() const noexcept { return m_edited.load(std::memory_order_relaxed); }
    // Any thread: a given pad's parameter (tests, pads.json).
    float padParam(int pad, ParamId id) const noexcept { return m_pads[std::size_t(pad)].get(int(id)); }
    void setPadParam(int pad, ParamId id, float v) noexcept { m_pads[std::size_t(pad)].set(int(id), v); }
    // Any thread: report every parameter as changed (after loading pads from a file).
    void reportAllChanged() noexcept { m_forced.fetch_or(~std::uint64_t(0), std::memory_order_relaxed); }

    // GUI thread: the sample a pad plays (nullptr: none), taken at the next
    // block. The caller keeps the old one alive until blocksProcessed() has
    // moved on by two (its voices are stopped when the swap is taken).
    void setSample(int pad, const Sample *sample) noexcept
    {
        m_requested[std::size_t(pad)].store(sample, std::memory_order_release);
    }
    std::uint64_t blocksProcessed() const noexcept { return m_blocks.load(std::memory_order_acquire); }

    // GUI thread: hit a pad (applied at the next block), or let it go.
    bool postHit(int pad, int velocity) noexcept { return m_hits.push({std::uint8_t(pad), std::uint8_t(velocity)}); }
    bool postRelease(int pad) noexcept { return m_hits.push({std::uint8_t(pad), 0}); }

    // The trance gate's 16 steps (bit n = step n on). Any thread.
    void setGatePattern(std::uint16_t steps) noexcept { m_gatePattern.store(steps, std::memory_order_relaxed); }
    std::uint16_t gatePattern() const noexcept { return m_gatePattern.load(std::memory_order_relaxed); }

    // Any thread: is the pad sounding (for its light)? Updated every block.
    bool padSounding(int pad) const noexcept { return (m_soundingMask.load(std::memory_order_relaxed) >> pad) & 1u; }

    // GUI thread: Program Changes received, for the pattern controller.
    std::optional<int> popProgram() noexcept
    {
        if (const auto p = m_programs.pop())
            return int(*p);
        return std::nullopt;
    }

    // The pads' settings in DSP units (tests).
    PadSettings settings(int pad) const noexcept;
    const PadVoice &voice(int pad, int which) const noexcept { return m_voices[std::size_t(pad)][std::size_t(which)]; }

    // Audio thread (the sequencer and the keys): hit or release a pad.
    void hit(int pad, float velocity, double pitchSemis = 0.0, bool flip = false, int note = -1) noexcept;
    void releasePad(int pad, int note = -1) noexcept;

private:
    static constexpr std::uint32_t kChunk = 128;
    using Store = krate::ParamValues<ParamCount>;

    void handleMidi(const krate::MidiEvent &event) noexcept;
    void takeSamples() noexcept;
    void render(float *left, float *right, std::uint32_t frames, double bpm, double beat, double beatsPerFrame) noexcept;
    float global(ParamId id) const noexcept { return m_global.get(int(id)); }

    Store m_global{kParams};
    std::array<Store, kPads> m_pads{Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams},
                                    Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams},
                                    Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams},
                                    Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams}};
    std::atomic<int> m_edited{0};
    std::atomic<std::uint64_t> m_forced{0};
    krate::MidiLearnMap m_learn{kParams};
    double m_sampleRate = 48000.0;

    std::array<std::atomic<const Sample *>, kPads> m_requested{};
    std::array<const Sample *, kPads> m_samples{};
    std::array<std::array<PadVoice, 2>, kPads> m_voices;
    std::array<int, kPads> m_current{}; // the voice of each pad taking new hits
    std::array<PadSettings, kPads> m_settings;
    std::atomic<std::uint64_t> m_blocks{0};
    std::atomic<std::uint32_t> m_soundingMask{0};

    struct Hit
    {
        std::uint8_t pad, velocity; // velocity 0: release
    };
    krate::SpscQueue<Hit, 128> m_hits;
    krate::SpscQueue<std::uint8_t, 16> m_programs;

    // Trance gate
    std::atomic<std::uint16_t> m_gatePattern{0x7777};
    double m_gate = 1.0;          // smoothed
    double m_freeBeat = 0.0;      // the gate's clock while the transport is stopped
    std::array<float, kChunk> m_gateValues{};

    // Sends and effects
    std::array<float, kChunk> m_delayL{}, m_delayR{}, m_reverbL{}, m_reverbR{}, m_dryL{}, m_dryR{}, m_tmpL{}, m_tmpR{};
    krate::Delay m_delay{4.0};
    krate::Reverb m_reverb{192000.0};
    bool m_reverbOn = false;
    bool m_wasPlaying = false;
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;
};

} // namespace klang
