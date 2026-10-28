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
    enum class Type : std::uint8_t { NoteOn, NoteOff, ControlChange, ProgramChange, Other };

    Type type = Type::Other;
    std::uint8_t channel = 0;  // 0..15
    std::uint8_t data1 = 0;    // note, CC number or program
    std::uint8_t data2 = 0;    // velocity or CC value
};

MidiMessage decode(const MidiEvent &event) noexcept;

// CC -> target table plus MIDI Learn, shared by every instrument
// (docs/SHARED_FOUNDATIONS.MD#midi-and-midi-learn). A target is either a
// parameter (index into the instrument's descriptor table, >= 0) or an action
// (a button the app defines, e.g. "select pattern 1": actionTarget(n), <= -2).
// Parameters follow the CC value; actions fire on a press (value > 0) and are
// handed to the GUI via popAction(). Mapping and learn-target writes are
// atomic, so the GUI may change them while the audio thread reads them.
class MidiLearnMap
{
public:
    static constexpr int kUnmapped = -1;
    static constexpr int actionTarget(int action) noexcept { return -2 - action; }
    static constexpr bool isAction(int target) noexcept { return target <= -2; }
    static constexpr int actionIndex(int target) noexcept { return -2 - target; }

    struct Learned
    {
        std::uint8_t cc = 0;
        std::int16_t target = kUnmapped; // parameter index or actionTarget(n)
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

    // Setup: how many actions the app defines (0 = parameters only).
    void setActionCount(int count) noexcept { m_actionCount = count; }
    int actionCount() const noexcept { return m_actionCount; }

    // Any thread.
    void setMapping(int cc, int target) noexcept;
    int mapping(int cc) const noexcept;
    void clearAll() noexcept;
    void setLearnTarget(int target) noexcept; // kUnmapped = not learning
    int learnTarget() const noexcept { return m_learnTarget.load(std::memory_order_relaxed); }

    // Audio thread: a Control Change arrived. If a target is armed, the CC is
    // bound to it first and a Learned notice is queued for the GUI: a learned
    // knob applies this value at once, a learned button does not fire on the
    // press that taught it. A mapped action press is queued for popAction().
    Result controlChange(int cc, int value) noexcept;

    // GUI thread (single consumers).
    std::optional<Learned> popLearned() noexcept { return m_learned.pop(); }
    std::optional<int> popAction() noexcept { return m_actions.pop(); } // action index

private:
    bool isParam(int target) const noexcept { return target >= 0 && target < int(m_params.size()); }
    bool valid(int target) const noexcept
    {
        return isParam(target) || (isAction(target) && actionIndex(target) < m_actionCount);
    }

    std::span<const ParamDescriptor> m_params;
    int m_actionCount = 0;
    std::array<std::atomic<std::int16_t>, 128> m_map;
    std::atomic<std::int16_t> m_learnTarget{kUnmapped};
    SpscQueue<Learned, 64> m_learned;
    SpscQueue<int, 64> m_actions;
};

} // namespace krate
