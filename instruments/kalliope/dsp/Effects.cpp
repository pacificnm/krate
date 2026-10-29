#include "kalliope/dsp/Effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace kalliope {

namespace {
struct ChorusMode
{
    double rateHz, centreMs, depthMs;
};
constexpr ChorusMode kModes[] = {{0.0, 0.0, 0.0}, {0.5, 7.0, 2.0}, {0.85, 7.0, 3.0}};
constexpr double kChorusMix = 0.7071; // dry + wet at equal parts, about the dry level overall
constexpr double kFeedbackLowpassHz = 4000.0;
constexpr double kDelaySmoothingSeconds = 0.05; // tempo and time changes glide instead of clicking
} // namespace

void Chorus::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_line.fill(0.0f);
    m_write = 0;
    m_phase = 0.0;
}

float Chorus::read(double delaySamples) const noexcept
{
    const double pos = double(m_write) - delaySamples;
    const double floorPos = std::floor(pos);
    const double frac = pos - floorPos;
    const auto i0 = std::size_t(std::int64_t(floorPos) & std::int64_t(kSize - 1));
    const auto i1 = (i0 + 1) & (kSize - 1);
    return float(m_line[i0] * (1.0 - frac) + m_line[i1] * frac);
}

void Chorus::process(const float *in, float *left, float *right, std::uint32_t frames, int mode) noexcept
{
    mode = std::clamp(mode, 0, 2);
    const ChorusMode &m = kModes[mode];
    const double msToSamples = m_rate / 1000.0;
    for (std::uint32_t i = 0; i < frames; ++i) {
        m_line[m_write] = in[i];
        if (mode == 0) {
            left[i] = in[i];
            right[i] = in[i];
        } else {
            const double a = 2.0 * std::numbers::pi * m_phase;
            const double dl = (m.centreMs + m.depthMs * std::sin(a)) * msToSamples;
            const double dr = (m.centreMs + m.depthMs * std::cos(a)) * msToSamples; // a quarter cycle on
            left[i] = float(kChorusMix * (double(in[i]) + read(dl)));
            right[i] = float(kChorusMix * (double(in[i]) + read(dr)));
            m_phase += m.rateHz / m_rate;
            m_phase -= std::floor(m_phase);
        }
        m_write = (m_write + 1) & (kSize - 1);
    }
}

Delay::Delay(double maxSeconds, double maxSampleRate)
    : m_left(std::size_t(maxSeconds * maxSampleRate) + 4)
    , m_right(m_left.size())
{
    prepare(48000.0);
}

void Delay::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_smoothing = 1.0 - std::exp(-1.0 / (kDelaySmoothingSeconds * sampleRate));
    m_lowpass = 1.0 - std::exp(-2.0 * std::numbers::pi * kFeedbackLowpassHz / sampleRate);
    clear();
}

void Delay::clear() noexcept
{
    std::fill(m_left.begin(), m_left.end(), 0.0f);
    std::fill(m_right.begin(), m_right.end(), 0.0f);
    m_write = 0;
    m_delay = -1.0;
    m_lpLeft = m_lpRight = 0.0;
}

void Delay::process(float *left, float *right, std::uint32_t frames, double seconds, double level,
                    double feedback) noexcept
{
    const std::size_t size = m_left.size();
    const double target = std::clamp(seconds * m_rate, 1.0, double(size - 2));
    if (m_delay < 0.0)
        m_delay = target;
    feedback = std::clamp(feedback, 0.0, 0.9);
    for (std::uint32_t i = 0; i < frames; ++i) {
        m_delay += (target - m_delay) * m_smoothing;
        // Read both lines m_delay samples back (linear interpolation).
        const double pos = double(m_write) + double(size) - m_delay;
        const double floorPos = std::floor(pos);
        const double frac = pos - floorPos;
        const auto i0 = std::size_t(floorPos) % size;
        const auto i1 = (i0 + 1) % size;
        const double yl = m_left[i0] * (1.0 - frac) + m_left[i1] * frac;
        const double yr = m_right[i0] * (1.0 - frac) + m_right[i1] * frac;

        // Ping-pong: the input enters on the left; each line feeds the other.
        m_lpLeft += (yl - m_lpLeft) * m_lowpass;
        m_lpRight += (yr - m_lpRight) * m_lowpass;
        const double input = 0.5 * (double(left[i]) + double(right[i]));
        m_left[m_write] = float(input + feedback * m_lpRight);
        m_right[m_write] = float(feedback * m_lpLeft);
        m_write = (m_write + 1) % size;

        left[i] = float(double(left[i]) + level * yl);
        right[i] = float(double(right[i]) + level * yr);
    }
}

} // namespace kalliope
