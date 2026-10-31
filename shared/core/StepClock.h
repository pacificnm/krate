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
    static constexpr int kMaxLength = 64;
    void setLength(int length) noexcept;          // 1..kMaxLength steps in the pattern
    void setGateLength(double fraction) noexcept { m_gateLength = fraction; }

    void start() noexcept;  // the next tick starts the first step
    // Running, but the first step waits for this beat (joining a transport
    // that's already playing, on the next bar).
    void startAt(double beat) noexcept;
    // The start of the next bar (of `beatsPerBar` quarter notes) at or after `beat`.
    static double nextBar(double beat, int beatsPerBar = 4) noexcept
    {
        const double bars = beat / beatsPerBar;
        const double whole = double(static_cast<long long>(bars));
        return (bars - whole < 1e-9 ? whole : whole + 1.0) * beatsPerBar;
    }
    void stop() noexcept;
    bool running() const noexcept { return m_running; }

    // Once per sample while running, with that sample's beat position.
    Tick tick(double beat) noexcept;

    // Right after a tick that started a step: make that step the first of the
    // current order and length instead (a pattern change at a pass boundary).
    void restartPass() noexcept;

    int step() const noexcept { return m_running ? m_step : -1; }  // pattern index
    int previousStep() const noexcept { return m_prevStep; }        // -1 at the start
    int nextStep() const noexcept { return m_next < m_length ? m_next : 0; } // already chosen
    int stepsPerBeat() const noexcept { return m_stepsPerBeat; }
    // Steps played since start(): 0 for the first. Its parity is the step's
    // place in time (on or off the beat), whatever the play order (Kessel's shuffle).
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
    bool m_waiting = false;       // startAt(): nothing before m_startBeat
    double m_startBeat = 0.0;
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
