#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace krunch {

// Noise gate on the guitar before the preamp (where hum and string noise get
// amplified): opens fast above the threshold, holds, then closes smoothly
// once the level is 6 dB under it.
class Gate
{
public:
    void prepare(double sampleRate) noexcept
    {
        m_detectRelease = std::exp(-1.0 / (0.020 * sampleRate));
        m_open = 1.0 / (0.0005 * sampleRate); // 0.5 ms
        m_close = 1.0 / (0.080 * sampleRate); // 80 ms
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

    // threshold: 0 = off, else 0..1 over -80..-20 dBFS.
    void process(float *buffer, std::uint32_t frames, float threshold) noexcept
    {
        if (threshold <= 0.0f && m_gain >= 1.0)
            return;
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
            m_gain = m_isOpen ? std::min(1.0, m_gain + m_open) : std::max(0.0, m_gain - m_close);
            buffer[n] = float(buffer[n] * m_gain);
        }
    }

    bool closed() const noexcept { return m_gain <= 0.0; }

private:
    double m_detectRelease = 0.0;
    double m_open = 0.0, m_close = 0.0;
    std::uint32_t m_holdFrames = 0;
    double m_level = 0.0;
    double m_gain = 1.0;
    std::uint32_t m_hold = 0;
    bool m_isOpen = true;
};

} // namespace krunch
