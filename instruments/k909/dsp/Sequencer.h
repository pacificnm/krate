#pragma once

#include <array>
#include <cstdint>

#include "core/StepClock.h"
#include "dsp/Voices.h"

namespace k909 {

// One step of one track (docs/SEQUENCER.md#pattern-data).
enum class StepState : std::uint8_t { Off, On, Accent };

struct Pattern
{
    std::array<std::array<StepState, 16>, DrumCount> tracks{}; // in DrumId order
    std::uint8_t length = 16;      // 1..16
    std::uint8_t stepsPerBeat = 4; // 4 = 1/16, 8 = 1/32, 3 = 1/8T, 6 = 1/16T

    bool operator==(const Pattern &) const = default;
};

// Built-in grooves for a new install; factory pattern 0 also plays at startup
// and in k909-render --pattern.
inline constexpr int kFactoryPatternCount = 4;
Pattern factoryPattern(int index); // 0..kFactoryPatternCount-1

// The voices to play on one sample: bit n = DrumId n.
struct Hits
{
    std::uint16_t on = 0;
    std::uint16_t accent = 0; // subset of `on`
};

// K909's tracks x steps sequencer (docs/SEQUENCER.md#playback). Timing and step
// order come from krate::StepClock, driven by the transport's beat position.
// Call tick() once per sample while running. Realtime-safe.
class Sequencer
{
public:
    Sequencer();

    void setPattern(const Pattern &pattern) noexcept;
    void setStep(int track, int step, StepState state) noexcept; // ignored if out of range
    void setLength(int length) noexcept;                          // clamped to 1..16
    void setStepsPerBeat(int steps) noexcept;
    const Pattern &pattern() const noexcept { return m_pattern; }

    void setOrder(krate::StepClock::Order order) noexcept { m_clock.setOrder(order); }
    // 0 = straight .. 5 = every second step a third of a step late.
    void setShuffle(int amount) noexcept;

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

    Pattern m_pattern = factoryPattern(0);
    krate::StepClock m_clock;
    int m_shuffle = 0;
    int m_pendingStep = -1; // a shuffled step waiting for its delayed trigger
};

} // namespace k909
