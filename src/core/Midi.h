#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

#include "core/Instrument.h"
#include "core/SpscQueue.h"

namespace krate {

// A decoded channel message. Note On with velocity 0 decodes as NoteOff.
struct MidiMessage
{
    enum class Type : std::uint8_t { NoteOn, NoteOff, ControlChange, Other };

    Type type = Type::Other;
    std::uint8_t channel = 0;  // 0..15
    std::uint8_t data1 = 0;    // note or CC number
    std::uint8_t data2 = 0;    // velocity or CC value
};

MidiMessage decode(const MidiEvent &event) noexcept;

// CC -> parameter table plus MIDI Learn, shared by every instrument
// (docs/SHARED_FOUNDATIONS.MD#midi-and-midi-learn). Parameters are indices
// into the instrument's descriptor table. Mapping and learn-target writes are
// atomic, so the GUI may change them while the audio thread reads them.
class MidiLearnMap
{
public:
    static constexpr int kUnmapped = -1;

    struct Learned
    {
        std::uint8_t cc = 0;
        std::int16_t param = kUnmapped;
    };

    // What a CC should do: set `param` to `value` (normalized, snapped to the
    // parameter's detents), or nothing if param is kUnmapped.
    struct Result
    {
        int param = kUnmapped;
        float value = 0.0f;
    };

    explicit MidiLearnMap(std::span<const ParamDescriptor> params) noexcept;

    std::span<const ParamDescriptor> parameters() const noexcept { return m_params; }

    // Any thread.
    void setMapping(int cc, int param) noexcept;
    int mapping(int cc) const noexcept;
    void clearAll() noexcept;
    void setLearnTarget(int param) noexcept;  // kUnmapped = not learning
    int learnTarget() const noexcept { return m_learnTarget.load(std::memory_order_relaxed); }

    // Audio thread: a Control Change arrived. If a parameter is armed, the CC
    // is bound to it first (so the rest of the same knob turn already moves
    // it) and a Learned notice is queued for the GUI.
    Result controlChange(int cc, int value) noexcept;

    // GUI thread (single consumer).
    std::optional<Learned> popLearned() noexcept { return m_learned.pop(); }

private:
    bool valid(int param) const noexcept { return param >= 0 && param < int(m_params.size()); }

    std::span<const ParamDescriptor> m_params;
    std::array<std::atomic<std::int16_t>, 128> m_map;
    std::atomic<std::int16_t> m_learnTarget{kUnmapped};
    SpscQueue<Learned, 64> m_learned;
};

} // namespace krate
