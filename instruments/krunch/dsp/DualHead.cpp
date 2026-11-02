#include "dsp/DualHead.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace krunch {

namespace {
std::string delayText(float v) { return std::format("{:.1f} ms", DualHead::delayMs(v)); }
std::string toneText(float v)
{
    const double t = DualHead::tilt(v);
    return std::fabs(t) < 0.03 ? std::string("Same") : t < 0.0 ? std::format("Darker {:.0f}", -t * 10.0 / 0.7) : std::format("Brighter {:.0f}", t * 10.0 / 0.7);
}
std::string balanceText(float v) { return std::format("{:+.1f} dB", DualHead::balanceDb(v) + 0.0); }
} // namespace

const PedalInfo DualHead::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"delay", "DELAY", 0.3f, &delayText},
        {"tone", "TONE", 0.5f, &toneText},
        {"balance", "BALANCE", 1.0f, &balanceText},
    };
    return PedalInfo{"dual_head", "Dual Head", "#3B4A6B", knobs, true};
}();

double DualHead::delayMs(float v) { return 40.0 * std::clamp(double(v), 0.0, 1.0); }
double DualHead::tilt(float v) { return 1.4 * (std::clamp(double(v), 0.0, 1.0) - 0.5); }
double DualHead::balanceDb(float v) { return -12.0 * (1.0 - std::clamp(double(v), 0.0, 1.0)); }

void DualHeadSplit::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_glide = 1.0 - std::exp(-1.0 / (0.05 * sampleRate)); // the delay moves over ~50 ms: no zipper
    m_lowA = 1.0 - std::exp(-2.0 * std::numbers::pi * 1200.0 / sampleRate);
    m_mixStep = 1.0 / (0.01 * sampleRate); // on and off over 10 ms
    reset();
}

void DualHeadSplit::reset() noexcept
{
    std::fill(std::begin(m_buffer), std::end(m_buffer), 0.0f);
    m_write = 0;
    m_delay = -1.0;
    m_low = 0.0;
    m_mix = 0.0;
}

void DualHeadSplit::process(const DualHead *pedal, float *left, float *right, std::uint32_t frames) noexcept
{
    (void)left; // the first amp: as it is
    if (!pedal && m_mix <= 0.0)
        return;
    const double target = pedal ? std::min(DualHead::delayMs(pedal->knob(DualHead::Delay)) * 0.001 * m_rate, double(kMaxDelay - 4)) : m_delay;
    if (m_delay < 0.0)
        m_delay = target;
    const double tilt = pedal ? DualHead::tilt(pedal->knob(DualHead::Tone)) : 0.0;
    const double gain = pedal ? std::pow(10.0, DualHead::balanceDb(pedal->knob(DualHead::Balance)) / 20.0) : 1.0;
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float in = right[n];
        m_buffer[m_write] = in;
        m_delay += m_glide * (target - m_delay);
        // Read m_delay samples back, linearly between two.
        const double pos = double(m_write) - m_delay;
        const double wrapped = pos < 0.0 ? pos + kMaxDelay : pos;
        const int i = int(wrapped);
        const double f = wrapped - double(i);
        const double delayed = double(m_buffer[i % kMaxDelay]) + f * double(m_buffer[(i + 1) % kMaxDelay] - m_buffer[i % kMaxDelay]);
        if (++m_write == kMaxDelay)
            m_write = 0;
        // The second amp's voice: tilted around 1.2 kHz.
        m_low += m_lowA * (delayed - m_low);
        const double second = (delayed + tilt * (delayed - m_low)) * gain;
        m_mix = pedal ? std::min(1.0, m_mix + m_mixStep) : std::max(0.0, m_mix - m_mixStep);
        right[n] = float(double(in) + m_mix * (second - double(in)));
    }
}

} // namespace krunch
