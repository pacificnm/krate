#pragma once

#include <cstdint>

namespace krate {

// Step timing and step order for a step sequencer, driven by the transport's
// beat position (docs/SHARED_FOUNDATIONS.MD#instrument-notes). Each instrument
// keeps its own step data and rules; this only answers "did a step start on
// this sample, which one, and which comes next". Realtime-safe.
class StepClock
{
public:
    enum class Order : std::uint8_t { Forward, Reverse, PingPong, Random };

    struct Tick
    {
        bool stepStarted = false; // a new step begins on this sample
        bool gatePoint = false;   // the current step reached its gate length
    };

    // Changing steps-per-beat mid-step keeps the fraction of the current step done.
    void setStepsPerBeat(int steps) noexcept;
    void setOrder(Order order) noexcept { m_order = order; }
    void setLength(int length) noexcept;          // 1..16 steps in the pattern
    void setGateLength(double fraction) noexcept { m_gateLength = fraction; }

    void start() noexcept;  // the next tick starts the first step
    void stop() noexcept;
    bool running() const noexcept { return m_running; }

    // Once per sample while running, with that sample's beat position.
    Tick tick(double beat) noexcept;

    int step() const noexcept { return m_running ? m_step : -1; }  // pattern index
    int previousStep() const noexcept { return m_prevStep; }        // -1 at the start
    int nextStep() const noexcept { return m_next < m_length ? m_next : 0; } // already chosen
    int stepsPerBeat() const noexcept { return m_stepsPerBeat; }
    // Steps played since start(): 0 for the first. Its parity is the step's
    // place in time (on or off the beat), whatever the play order (K909's shuffle).
    std::int64_t stepCount() const noexcept { return m_count; }

private:
    int firstStep() noexcept;
    void chooseNext() noexcept;
    std::uint32_t random() noexcept; // xorshift32

    int m_stepsPerBeat = 4;
    int m_length = 16;
    double m_gateLength = 0.5;
    Order m_order = Order::Forward;

    bool m_running = false;
    bool m_anchored = false;
    double m_anchorBeat = 0.0;    // step position = m_anchorPos + (beat - m_anchorBeat) * stepsPerBeat
    double m_anchorPos = 0.0;
    double m_lastBeat = 0.0;
    std::int64_t m_count = -1;    // absolute step number since start
    bool m_gateChecked = false;

    int m_step = -1;
    int m_prevStep = -1;
    int m_next = 0;
    int m_direction = 1;          // ping-pong
    int m_nextDirection = 1;
    std::uint32_t m_rng = 0x9E3779B9u;
};

} // namespace krate
