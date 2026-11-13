#include "dsp/Boost.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {
double boostDb(float v) { return Boost::kMaxDb * std::clamp(double(v), 0.0, 1.0); }
std::string boostText(float v) { return std::format("+{:.1f} dB", std::round(boostDb(v) * 10.0) / 10.0); }
std::string onOff(float v) { return v >= 0.5f ? "On" : "Off"; }
} // namespace

const PedalInfo Boost::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"boost", "BOOST", 0.3f, &boostText},     // +6 dB
        {"bright", "BRIGHT", 0.0f, &onOff, 2},
    };
    return PedalInfo{"boost", "Boost", "#C9CCD1", knobs, false};
}();

void Boost::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_input.setCutoff(sampleRate, 15.0);
    m_brightOn = -1.0f;
}

void Boost::resetEffect() noexcept
{
    m_input.reset();
    m_bright.reset();
    m_gain = -1.0f;
}

void Boost::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float bright = knob(Bright) >= 0.5f ? 1.0f : 0.0f; bright != m_brightOn) {
        m_brightOn = bright;
        m_bright.setHighShelf(m_rate, 3000.0, bright > 0.0f ? kBrightDb : 0.0);
    }
    const float target = float(std::pow(10.0, boostDb(knob(Amount)) / 20.0));
    if (m_gain < 0.0f)
        m_gain = target;
    const float step = (target - m_gain) / float(frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_gain += step;
        buffer[n] = m_bright.process(m_input.highpass(buffer[n])) * m_gain;
    }
    m_gain = target;
}

} // namespace krunch
