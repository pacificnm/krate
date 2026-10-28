#include "core/StepClock.h"

#include <algorithm>
#include <cmath>

namespace krate {

namespace {
// Absorbs rounding in the beat arithmetic so a step lands on the sample where
// it belongs instead of one late (1e-6 of a step is far below one sample).
constexpr double kEpsilon = 1e-6;
} // namespace

void StepClock::setStepsPerBeat(int steps) noexcept
{
    if (steps < 1 || steps == m_stepsPerBeat)
        return;
    if (m_anchored) {
        // Re-anchor at the last position so the step position stays continuous.
        m_anchorPos += (m_lastBeat - m_anchorBeat) * m_stepsPerBeat;
        m_anchorBeat = m_lastBeat;
    }
    m_stepsPerBeat = steps;
}

void StepClock::setLength(int length) noexcept
{
    m_length = std::clamp(length, 1, 16);
}

void StepClock::start() noexcept
{
    m_running = true;
    m_anchored = false;
    m_count = -1;
    m_step = -1;
    m_prevStep = -1;
    m_direction = m_nextDirection = 1;
    m_gateChecked = false;
}

void StepClock::stop() noexcept
{
    m_running = false;
    m_step = -1;
}

StepClock::Tick StepClock::tick(double beat) noexcept
{
    Tick t;
    if (!m_running)
        return t;
    if (!m_anchored) {
        // The first step starts on the first sample after start(), wherever the beat is.
        m_anchorBeat = beat;
        m_anchorPos = 0.0;
        m_anchored = true;
    }
    m_lastBeat = beat;

    const double pos = m_anchorPos + (beat - m_anchorBeat) * m_stepsPerBeat;
    const auto count = std::int64_t(std::floor(pos + kEpsilon));

    if (count > m_count) {
        m_count = count;
        m_prevStep = m_step;
        m_step = m_step < 0 ? firstStep() : nextStep();
        m_direction = m_nextDirection;
        chooseNext(); // decided now, so the caller can look ahead (slides, ties)
        m_gateChecked = false;
        t.stepStarted = true;
    } else if (!m_gateChecked && pos - double(m_count) + kEpsilon >= m_gateLength) {
        m_gateChecked = true;
        t.gatePoint = true;
    }
    return t;
}

int StepClock::firstStep() noexcept
{
    switch (m_order) {
    case Order::Reverse: return m_length - 1;
    case Order::Random: return int(random() % std::uint32_t(m_length));
    default: return 0;
    }
}

void StepClock::chooseNext() noexcept
{
    const int len = m_length;
    const int step = std::min(m_step, len - 1);
    m_nextDirection = m_direction;

    switch (m_order) {
    case Order::Forward:
        m_next = (step + 1) % len;
        break;
    case Order::Reverse:
        m_next = (step + len - 1) % len;
        break;
    case Order::PingPong:
        if (len == 1) {
            m_next = 0;
        } else if (step + m_direction >= len || step + m_direction < 0) {
            m_nextDirection = -m_direction; // turn around without repeating the end step
            m_next = step + m_nextDirection;
        } else {
            m_next = step + m_direction;
        }
        break;
    case Order::Random:
        m_next = int(random() % std::uint32_t(len));
        break;
    }
}

std::uint32_t StepClock::random() noexcept
{
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return m_rng;
}

} // namespace krate
