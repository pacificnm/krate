#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace krunch {

// Noise gate on the guitar before the amp (where hum and string noise get
// amplified): opens fast above the threshold, holds, then once the level is
// 6 dB under it fades out over the decay (exponentially, to -60 dB: short for
// tight stops, long to let notes ring out).
class Gate
{
public:
    void prepare(double sampleRate) noexcept
    {
        m_detectRelease = std::exp(-1.0 / (0.020 * sampleRate));
        m_open = 1.0 / (0.0005 * sampleRate); // 0.5 ms
        m_rate = sampleRate;
        m_decay = -1.0;
        m_holdFrames = std::uint32_t(0.030 * sampleRate);
        reset();
    }
    void reset() noexcept
    {
        m_level = 0.0;
        m_gain = 1.0;
        m_hold = 0;
        m_isOpen = true;
    }

    static constexpr double kFloor = 0.001; // -60 dB: the fade ends, the gate is shut

    // threshold: 0 = off, else 0..1 over -80..-20 dBFS; decaySeconds: the fade out.
    void process(float *buffer, std::uint32_t frames, float threshold, double decaySeconds = 0.12) noexcept
    {
        if (threshold <= 0.0f && m_gain >= 1.0)
            return;
        if (decaySeconds != m_decay) {
            m_decay = decaySeconds;
            m_close = std::pow(kFloor, 1.0 / (std::max(0.001, decaySeconds) * m_rate));
        }
        const double open = threshold > 0.0f ? std::pow(10.0, (-80.0 + 60.0 * threshold) / 20.0) : 0.0;
        const double close = open * 0.5; // 6 dB under: no chatter around the threshold
        for (std::uint32_t n = 0; n < frames; ++n) {
            const double a = std::fabs(buffer[n]);
            m_level = a > m_level ? a : a + m_detectRelease * (m_level - a);
            if (m_level >= open) {
                m_isOpen = true;
                m_hold = m_holdFrames;
            } else if (m_hold > 0) {
                --m_hold;
            } else if (m_level < close) {
                m_isOpen = false;
            }
            if (m_isOpen)
                m_gain = std::min(1.0, m_gain + m_open);
            else
                m_gain = m_gain * m_close < kFloor ? 0.0 : m_gain * m_close;
            buffer[n] = float(buffer[n] * m_gain);
        }
    }

    bool closed() const noexcept { return m_gain <= 0.0; }

private:
    double m_detectRelease = 0.0;
    double m_open = 0.0, m_close = 0.0; // the open ramp's step; the fade's factor per sample
    double m_rate = 48000.0, m_decay = -1.0;
    std::uint32_t m_holdFrames = 0;
    double m_level = 0.0;
    double m_gain = 1.0;
    std::uint32_t m_hold = 0;
    bool m_isOpen = true;
};

} // namespace krunch
