#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/Instrument.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "konnekt/dsp/Module.h"

namespace konnekt {

inline constexpr int kTracePoints = 64;    // a jack's trace: the last 1.5 s in 64 slices
inline constexpr int kProbeSamples = 4096; // the big scope's buffer

// What a trace shows: each slice's lowest and highest value.
struct Trace
{
    std::array<float, kTracePoints> low{}, high{}; // oldest first
};

// Konnekt's realtime core (docs/BRIEF.md): up to 64 modules run in signal
// order, 16 samples at a time, each per-voice module once per voice; cables
// carry one signal per voice. The patch changes by commands from the GUI
// thread (lock-free); modules are made and freed there, the engine only runs
// them. Notes go to voices (the oldest is taken when all are busy; with one
// voice, the newest key wins and letting it go returns to the one held).
class Engine final : public krate::IInstrument
{
public:
    Engine();

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override;
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float v) noexcept override { m_params.set(index, v); }
    void setParamFromHost(int index, float v) noexcept override { m_params.setFromAudio(index, v); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    void process(const krate::AudioBlock &block) noexcept override;

    // --- The patch (GUI thread). Each is applied at the next block; false if
    // the command queue is full. A module is prepared by the caller at
    // sampleRate() before it is added, and comes back through popRetired()
    // once the engine has let it go (removed, or replaced in its slot).
    double sampleRate() const noexcept { return m_rate.load(std::memory_order_relaxed); }
    bool addModule(int slot, Module *module) noexcept;
    bool removeModule(int slot) noexcept;
    bool connect(int toSlot, int toJack, int fromSlot, int fromJack) noexcept;
    bool disconnect(int toSlot, int toJack) noexcept;
    // The order the modules run in: sources first (see signalOrder()).
    bool setOrder(std::span<const int> slots) noexcept;
    Module *popRetired() noexcept;

    void setVoices(int voices) noexcept { m_voiceCount.store(std::clamp(voices, 1, kVoices), std::memory_order_relaxed); }
    int voices() const noexcept { return m_voiceCount.load(std::memory_order_relaxed); }

    // --- MIDI Learn (any thread): the next CC moves this knob from now on.
    void armLearn(int slot, int knob) noexcept;
    void cancelLearn() noexcept { m_armed.store(-1, std::memory_order_relaxed); }
    struct Learned
    {
        int cc, slot, knob;
    };
    std::optional<Learned> popLearned() noexcept;
    void setCcMapping(int cc, int slot, int knob) noexcept; // slot -1: none
    void forgetSlot(int slot) noexcept;                      // a module removed: its mappings go
    std::optional<std::pair<int, int>> ccMapping(int cc) const noexcept;

    // --- Seeing the signals (GUI thread reads, any time). The voice of the
    // newest note is the one shown.
    Trace trace(int slot, int jack) const noexcept;
    // The big scope: one output's last kProbeSamples samples, oldest first.
    void setProbe(int slot, int jack) noexcept;
    void probe(std::span<float> out) const noexcept;
    int shownVoice() const noexcept { return m_shownVoice.load(std::memory_order_relaxed); }
    int notesHeld() const noexcept { return m_notesHeld.load(std::memory_order_relaxed); }

    // The order to run modules in for these cables (from -> to): sources first;
    // modules in a loop keep their slot order (their loop cable is a block late).
    struct Cable
    {
        int fromSlot, fromJack, toSlot, toJack;
    };
    static std::vector<int> signalOrder(std::span<const int> slots, std::span<const Cable> cables);

private:
    struct Command
    {
        enum class Kind : std::uint8_t { Add, Remove, Connect, Disconnect, Order } kind;
        std::int8_t slot = -1, jack = -1, fromSlot = -1, fromJack = -1;
        Module *module = nullptr;
        std::array<std::int8_t, kMaxModules> order{};
        std::int8_t count = 0;
    };
    struct Source
    {
        std::int8_t slot = -1, jack = -1;
    };

    void apply(const Command &c) noexcept;
    void handleMidi(const krate::MidiEvent &e) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void releaseVoice(int v) noexcept;
    void runBlock(float *left, float *right, int frames) noexcept;
    float *buffer(int slot, int jack, int voice) noexcept
    {
        return m_buffers.data() + ((std::size_t(slot) * kMaxJacks + std::size_t(jack)) * kVoices + std::size_t(voice)) * kBlock;
    }
    float *sum(int slot, int jack) noexcept
    {
        return m_sums.data() + (std::size_t(slot) * kMaxJacks + std::size_t(jack)) * kBlock;
    }
    void tap(int slot, int jack, const float *x, int frames) noexcept;

    krate::ParamValues<1> m_params;
    std::atomic<double> m_rate{48000.0};
    krate::SpscQueue<Command, 512> m_commands;
    krate::SpscQueue<Module *, 128> m_retired;

    // Audio thread: the patch.
    std::array<Module *, kMaxModules> m_modules{};
    std::array<std::array<Source, kMaxJacks>, kMaxModules> m_inputs{};
    std::array<std::int8_t, kMaxModules> m_order{};
    int m_orderCount = 0;
    std::vector<float> m_buffers; // [slot][jack][voice][kBlock], allocated in the constructor
    std::vector<float> m_sums;    // per-voice cables into one-voice inputs, mixed
    std::vector<float> m_silence;

    // Voices
    std::atomic<int> m_voiceCount{4};
    std::array<MidiVoice, kVoices> m_voice{};
    std::array<int, kVoices> m_voiceNote{};       // -1: none
    std::array<std::uint64_t, kVoices> m_voiceAge{};
    std::array<bool, kVoices> m_sustained{};
    std::array<int, kVoices> m_pendingNote{};     // a stolen voice: its gate closes for a block, then this
    std::array<float, kVoices> m_pendingVelocity{};
    std::uint64_t m_counter = 0;
    std::array<std::uint8_t, 16> m_held{};        // one voice: the keys held, oldest first
    int m_heldCount = 0;
    bool m_sustain = false;
    float m_bend = 0.0f, m_modWheel = 0.0f;
    std::atomic<int> m_shownVoice{0};
    std::atomic<int> m_notesHeld{0};

    // MIDI Learn
    std::atomic<int> m_armed{-1}; // slot * kMaxKnobs + knob
    std::array<std::atomic<int>, 128> m_ccMap{}; // slot * kMaxKnobs + knob, or -1
    krate::SpscQueue<Learned, 16> m_learned;

    // Traces and the probe
    struct TraceState
    {
        std::array<std::atomic<float>, kTracePoints> low{}, high{};
        std::atomic<int> head{0};
        float curLow = 0.0f, curHigh = 0.0f;
        int count = 0;
    };
    std::vector<TraceState> m_traces; // [slot][jack]
    int m_traceBin = 1125;
    std::atomic<int> m_probe{-1};     // slot * kMaxJacks + jack
    std::array<std::atomic<float>, kProbeSamples> m_probeData{};
    std::atomic<int> m_probeHead{0};
    double m_volume = -1.0;
};

} // namespace konnekt
