#pragma once

#include <algorithm>
#include <cmath>

namespace krate {

// ADSR with exponential (RC-like) segments (Kalliope's and Krimson's docs/DSP.md).
// The attack aims at 1.2 and stops at 1, so `attack` is the time to full level;
// decay and release are the times to fall 60 dB of the way to their target.
class Envelope
{
public:
    struct Times
    {
        double attack = 0.001, decay = 0.3, sustain = 1.0, release = 0.3; // seconds; sustain 0..1
    };

    void setSampleRate(double sampleRate) noexcept { m_rate = sampleRate; }
    // Times can change at any time (per block); `scale` shortens all of them (key follow).
    void setTimes(const Times &times, double scale = 1.0) noexcept;

    void gateOn() noexcept { m_stage = Stage::Attack; } // from the current level (no click)
    void gateOff() noexcept
    {
        if (m_stage != Stage::Idle)
            m_stage = Stage::Release;
    }
    void kill() noexcept { m_stage = Stage::Idle; m_level = 0.0; }
    bool active() const noexcept { return m_stage != Stage::Idle; }
    bool releasing() const noexcept { return m_stage == Stage::Release; }
    double level() const noexcept { return m_level; }

    double next() noexcept
    {
        switch (m_stage) {
        case Stage::Idle:
            break;
        case Stage::Attack:
            m_level += (1.2 - m_level) * m_attack;
            if (m_level >= 1.0) {
                m_level = 1.0;
                m_stage = Stage::Decay;
            }
            break;
        case Stage::Decay:
            m_level += (m_sustain - m_level) * m_decay;
            break;
        case Stage::Release:
            m_level += (0.0 - m_level) * m_release;
            if (m_level < 1e-5) {
                m_level = 0.0;
                m_stage = Stage::Idle;
            }
            break;
        }
        return m_level;
    }

private:
    enum class Stage { Idle, Attack, Decay, Release };

    double coefficient(double timeConstant) const noexcept
    {
        return 1.0 - std::exp(-1.0 / (std::max(timeConstant, 1e-5) * m_rate));
    }

    double m_rate = 48000.0;
    Stage m_stage = Stage::Idle;
    double m_level = 0.0;
    double m_attack = 1.0, m_decay = 1.0, m_release = 1.0, m_sustain = 1.0;
};

inline void Envelope::setTimes(const Times &t, double scale) noexcept
{
    constexpr double kLn6 = 1.791759469228055;   // 1.2 * (1 - e^-x) = 1 at x = ln 6
    constexpr double kLn1000 = 6.907755278982137; // 60 dB
    m_attack = coefficient(t.attack * scale / kLn6);
    m_decay = coefficient(t.decay * scale / kLn1000);
    m_release = coefficient(t.release * scale / kLn1000);
    m_sustain = t.sustain;
}

} // namespace krate
