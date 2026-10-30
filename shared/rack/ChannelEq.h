#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "dsp/Biquad.h"

namespace krate {

// A mixer channel's EQ (docs/SHARED_FOUNDATIONS.MD#mixer-eq): a low shelf, a
// fully parametric mid (frequency, gain, width) and a high shelf, on both
// sides. Flat (every gain 0 dB), it does nothing at all. Audio thread.
class ChannelEq
{
public:
    enum Band : int { LowGain, LowFreq, MidGain, MidFreq, MidQ, HighGain, HighFreq, Count };
    using Settings = std::array<float, Count>; // dB, Hz, Q

    static constexpr Settings kFlat{0.0f, 100.0f, 0.0f, 1000.0f, 0.7f, 0.0f, 8000.0f};
    static constexpr float kMaxGainDb = 15.0f;

    static bool flat(const Settings &s) noexcept
    {
        return std::fabs(s[LowGain]) < 0.01f && std::fabs(s[MidGain]) < 0.01f && std::fabs(s[HighGain]) < 0.01f;
    }

    void prepare(double sampleRate) noexcept
    {
        m_rate = sampleRate;
        m_applied = {};
        m_wasFlat = true;
        for (auto &side : m_filters)
            for (Biquad &f : side)
                f.reset();
    }

    // Before each chunk: retunes when the settings changed (the filters keep
    // their state, so a knob turning doesn't click), then filters `frames` of
    // both sides in place.
    void process(const Settings &settings, float *left, float *right, std::uint32_t frames) noexcept
    {
        const bool isFlat = flat(settings);
        if (isFlat) {
            m_wasFlat = true;
            return;
        }
        if (m_wasFlat) { // from flat: start clean
            for (auto &side : m_filters)
                for (Biquad &f : side)
                    f.reset();
            m_applied = {};
            m_wasFlat = false;
        }
        if (settings != m_applied) {
            for (auto &side : m_filters) {
                side[0].setLowShelf(m_rate, settings[LowFreq], settings[LowGain]);
                side[1].setPeak(m_rate, settings[MidFreq], settings[MidQ], settings[MidGain]);
                side[2].setHighShelf(m_rate, settings[HighFreq], settings[HighGain]);
            }
            m_applied = settings;
        }
        float *const sides[2] = {left, right};
        for (int c = 0; c < 2; ++c) {
            auto &f = m_filters[std::size_t(c)];
            float *x = sides[c];
            for (std::uint32_t i = 0; i < frames; ++i)
                x[i] = f[2].process(f[1].process(f[0].process(x[i])));
        }
    }

    // The response at a frequency (tests, a curve on screen).
    static double magnitude(const Settings &s, double sampleRate, double hz) noexcept
    {
        Biquad low, mid, high;
        low.setLowShelf(sampleRate, s[LowFreq], s[LowGain]);
        mid.setPeak(sampleRate, s[MidFreq], s[MidQ], s[MidGain]);
        high.setHighShelf(sampleRate, s[HighFreq], s[HighGain]);
        return low.magnitude(sampleRate, hz) * mid.magnitude(sampleRate, hz) * high.magnitude(sampleRate, hz);
    }

private:
    double m_rate = 48000.0;
    std::array<std::array<Biquad, 3>, 2> m_filters{};
    Settings m_applied{};
    bool m_wasFlat = true;
};

} // namespace krate
