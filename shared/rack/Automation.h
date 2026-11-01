#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/Instrument.h"
#include "core/SpscQueue.h"

namespace krate {

// Automation (docs/DAW.md, D5): a knob's movements over the song, as points
// (beat, normalized value) joined by straight lines; before the first point
// its value, after the last the last.
struct AutomationPoint
{
    double beat = 0.0;
    float value = 0.0f;
    bool operator==(const AutomationPoint &) const = default;
};

struct AutomationLane
{
    int device = -1; // rack device
    int param = -1;  // index into its parameters()
    std::vector<AutomationPoint> points; // by beat
    bool operator==(const AutomationLane &) const = default;

    float valueAt(double beat) const noexcept
    {
        if (points.empty())
            return 0.0f;
        if (beat <= points.front().beat)
            return points.front().value;
        if (beat >= points.back().beat)
            return points.back().value;
        std::size_t lo = 0, hi = points.size() - 1; // points[lo].beat < beat < points[hi].beat
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            (points[mid].beat <= beat ? lo : hi) = mid;
        }
        const AutomationPoint &a = points[lo], &b = points[hi];
        const double t = b.beat > a.beat ? (beat - a.beat) / (b.beat - a.beat) : 1.0;
        return float(double(a.value) + t * double(b.value - a.value));
    }
};

struct Automation
{
    std::vector<AutomationLane> lanes;
};

// Plays automation into the rack's instruments on the audio thread, once per
// block (or part of one, around a loop's end), at the block's beat, while the
// transport plays and it's on (SONG mode). A value is set as a MIDI-learned
// knob's is (setParamFromHost: the panel follows), only when it changes. A
// lane can be held off (the knob being recorded over). The GUI hands over
// whole Automation snapshots; old ones come back for deletion.
class AutomationPlayer
{
public:
    static constexpr int kMaxLanes = 128;

    AutomationPlayer();
    ~AutomationPlayer();
    AutomationPlayer(const AutomationPlayer &) = delete;
    AutomationPlayer &operator=(const AutomationPlayer &) = delete;

    // --- GUI thread, before the host starts: the rack's instruments by device.
    void setInstruments(std::vector<IInstrument *> instruments) { m_instruments = std::move(instruments); }

    // --- GUI thread
    void setAutomation(std::unique_ptr<Automation> automation);
    void collectGarbage();
    void setActive(bool on) noexcept { m_active.store(on, std::memory_order_relaxed); }
    // Lane `lane` of the current automation held off (true) or played (false).
    void setHeld(int lane, bool held) noexcept;
    void releaseAll() noexcept;
    // What the player last set lane `lane` to (NaN: nothing yet), to tell a
    // knob it moved from one someone turned.
    float lastValue(int lane) const noexcept;

    // --- Audio thread
    void apply(const TransportState &transport) noexcept;

private:
    std::vector<IInstrument *> m_instruments;
    std::atomic<bool> m_active{false};
    std::array<std::atomic<bool>, kMaxLanes> m_held{};
    std::array<std::atomic<float>, kMaxLanes> m_last{};

    SpscQueue<Automation *, 16> m_incoming;
    SpscQueue<Automation *, 64> m_retired;
    Automation *m_pending = nullptr; // GUI side
    Automation *m_current = nullptr; // audio side
    Automation *m_backlog = nullptr; // audio side, waiting for room in m_retired
};

} // namespace krate
