#pragma once

#include <array>
#include <cstdint>

#include "core/StepClock.h"
#include "kessel/dsp/Voices.h"

namespace kessel {

// One step of one track (docs/SEQUENCER.md#pattern-data). Flam: a soft grace
// hit on the step, the main hit just after.
enum class StepState : std::uint8_t { Off, On, Accent, Flam };

// A step's details, per track and step (docs/SEQUENCER.md#step-details).
// 0 everywhere means none: the step plays as the pattern and the knobs say.
using StepBytes = std::array<std::array<std::uint8_t, 16>, DrumCount>;
inline constexpr int kLockSteps = 254; // a lock is 1 + round(value * 254)
inline constexpr std::uint8_t lockOf(double value)
{
    return std::uint8_t(1 + int((value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value) * kLockSteps + 0.5));
}
inline constexpr float lockValue(std::uint8_t lock) { return float(lock - 1) / float(kLockSteps); }

struct Pattern
{
    std::array<std::array<StepState, 16>, DrumCount> tracks{}; // in DrumId order
    StepBytes velocity{}; // 0 = the pattern's (0.8, accent 1.0), else 1..127
    StepBytes tune{};     // 0 = the knob, else a lock (lockOf)
    StepBytes decay{};    // 0 = the knob, else a lock
    std::uint8_t length = 16;      // 1..16
    std::uint8_t stepsPerBeat = 4; // 4 = 1/16, 8 = 1/32, 3 = 1/8T, 6 = 1/16T
    std::uint8_t order = 0;        // krate::StepClock::Order: Forward, Reverse, PingPong, Random

    bool operator==(const Pattern &) const = default;
};

// Built-in grooves for a new install (docs/SEQUENCER.md#factory-patterns):
// 0-3 the classic ones (slots A-D), 4-11 the industrial ones (slots I-P).
// Factory pattern 0 also plays at startup and in kessel-render --pattern.
inline constexpr int kFactoryPatternCount = 12;
inline constexpr int kClassicPatternCount = 4;
Pattern factoryPattern(int index); // 0..kFactoryPatternCount-1

// The voices to play on one sample: bit n = DrumId n, with each one's step details.
struct Hits
{
    std::uint16_t on = 0;
    std::uint16_t accent = 0; // subset of `on`
    std::uint16_t flam = 0;   // subset of `on`
    std::array<std::uint8_t, DrumCount> velocity{}, tune{}, decay{}; // as Pattern's

    // `other`'s hits added (its details for the voices it plays).
    void merge(const Hits &other) noexcept
    {
        for (int d = 0; d < DrumCount; ++d)
            if (other.on & (1u << d)) {
                velocity[std::size_t(d)] = other.velocity[std::size_t(d)];
                tune[std::size_t(d)] = other.tune[std::size_t(d)];
                decay[std::size_t(d)] = other.decay[std::size_t(d)];
            }
        on = std::uint16_t(on | other.on);
        accent = std::uint16_t(accent | other.accent);
        flam = std::uint16_t(flam | other.flam);
    }
};

// Kessel's tracks x steps sequencer (docs/SEQUENCER.md#playback). Timing and step
// order come from krate::StepClock, driven by the transport's beat position.
// Call tick() once per sample while running. Realtime-safe.
//
// Patterns carry the bank slot they came from (0..7 = A..H). A pattern chosen
// while running waits as the "queued" one and takes over when the next pass
// begins; edits name their slot, so they reach whichever of the two it is.
class Sequencer
{
public:
    Sequencer();

    // Plays `pattern` (from bank slot `slot`) now; drops any queued pattern.
    void setPattern(const Pattern &pattern, int slot = 0) noexcept;
    // While running: plays it from the start of the next pass. Stopped: now.
    void queuePattern(const Pattern &pattern, int slot) noexcept;
    const Pattern &pattern() const noexcept { return m_pattern; }
    int slot() const noexcept { return m_slot; }
    int queuedSlot() const noexcept { return m_queuedSlot; } // -1 = none

    // Replaces slot `slot`'s pattern (playing or queued) in place, keeping the
    // play position; ignored for other slots.
    void replacePattern(const Pattern &pattern, int slot) noexcept;

    // Edits to slot `slot`'s pattern (the playing or the queued one; else ignored).
    void setStep(int slot, int track, int step, StepState state) noexcept; // ignored if out of range
    // A step's velocity (0 = default, else 1..127) and tune / decay locks (0 = none).
    void setStepDetail(int slot, int track, int step, std::uint8_t velocity, std::uint8_t tune,
                       std::uint8_t decay) noexcept;
    void setLength(int slot, int length) noexcept;                         // clamped to 1..16
    void setStepsPerBeat(int slot, int steps) noexcept;                    // 3, 4, 6 or 8
    void setOrder(int slot, int order) noexcept;                           // 0..3
    // 0 = straight .. 5 = every second step a third of a step late.
    void setShuffle(int amount) noexcept;

    void startAt(double beat) noexcept; // running, the first step on that beat (joining a playing transport)
    void start() noexcept;
    void stop() noexcept;
    bool running() const noexcept { return m_clock.running(); }
    int currentStep() const noexcept { return m_clock.step(); }
    int stepsPerBeat() const noexcept { return m_clock.stepsPerBeat(); }

    // Once per sample while running, with that sample's beat position.
    Hits tick(double beat) noexcept;

    static constexpr int kMaxShuffle = 5;
    static constexpr double kMaxShuffleDelay = 1.0 / 3.0; // of a step

private:
    Hits hitsFor(int step) const noexcept;
    Pattern *target(int slot) noexcept; // the playing or queued pattern for that slot
    void applyTimings() noexcept;       // length, steps per beat and order to the clock

    Pattern m_pattern = factoryPattern(0);
    int m_slot = 0;
    Pattern m_queued{};
    int m_queuedSlot = -1;
    krate::StepClock m_clock;
    int m_shuffle = 0;
    int m_pendingStep = -1; // a shuffled step waiting for its delayed trigger
};

} // namespace Kessel
