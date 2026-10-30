#include "krimson/dsp/Ensemble.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace krimson {

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi;

double triangle(double phase) { return 1.0 - 4.0 * std::fabs(phase - std::floor(phase + 0.5)); } // -1..1

float hermite(float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return ((a * t - b) * t + c) * t + x0;
}
} // namespace

void Ensemble::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_tone = std::exp(-kTwoPi * 8000.0 / sampleRate);
    reset();
}

void Ensemble::reset() noexcept
{
    m_left.fill(0.0f);
    m_right.fill(0.0f);
    m_write = 0;
    m_toneL = m_toneR = 0.0;
}

float Ensemble::read(const std::array<float, kSize> &line, double delaySamples) const noexcept
{
    const double back = double(m_write) - delaySamples + double(kSize);
    const auto i = std::size_t(back);
    const auto t = float(back - double(i));
    return hermite(line[(i - 1) & (kSize - 1)], line[i & (kSize - 1)], line[(i + 1) & (kSize - 1)],
                   line[(i + 2) & (kSize - 1)], t);
}

void Ensemble::process(float *left, float *right, std::uint32_t frames, int mode) noexcept
{
    if (mode != m_lastMode && m_lastMode == Off)
        reset(); // switched on: no stale copies from before
    m_lastMode = mode;
    if (mode == Off)
        return;
    const double ms = m_rate / 1000.0;
    const double slowStep = (mode == Full ? 0.62 : 0.45) / m_rate;
    const double fastStep = 5.8 / m_rate;
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float inL = left[n], inR = right[n];
        // Into the chips: each side its own line, with a touch of the other (they share a sweep).
        m_left[m_write] = inL + 0.15f * inR;
        m_right[m_write] = inR + 0.15f * inL;

        double wetL = 0.0, wetR = 0.0;
        if (mode == Chorus) {
            wetL = read(m_left, (7.0 + 2.6 * triangle(m_slow)) * ms);
            wetR = read(m_right, (7.0 + 2.6 * triangle(m_slow + 0.25)) * ms);
        } else {
            // Three copies per side, a third of a cycle apart, with the shimmer on top.
            for (int k = 0; k < 3; ++k) {
                const double third = double(k) / 3.0;
                const double shimmer = 0.22 * std::sin(kTwoPi * (m_fast + third));
                wetL += read(m_left, (9.0 + 1.8 * std::sin(kTwoPi * (m_slow + third)) + shimmer) * ms);
                wetR += read(m_right, (9.0 + 1.8 * std::sin(kTwoPi * (m_slow + third + 0.5)) + shimmer) * ms);
            }
            wetL /= 3.0;
            wetR /= 3.0;
        }
        m_slow += slowStep;
        m_fast += fastStep;
        if (m_slow >= 1.0)
            m_slow -= 1.0;
        if (m_fast >= 1.0)
            m_fast -= 1.0;
        // The chips: darker, gently saturated.
        m_toneL = wetL + m_tone * (m_toneL - wetL);
        m_toneR = wetR + m_tone * (m_toneR - wetR);
        const double copyL = std::tanh(m_toneL * 1.2) / 1.2, copyR = std::tanh(m_toneR * 1.2) / 1.2;
        // Dry and copies, the copies a little louder in Ensemble (the thick sound), the level kept.
        const double wet = mode == Full ? 0.75 : 0.6;
        left[n] = float((inL + wet * copyL) / (1.0 + 0.3 * wet));
        right[n] = float((inR + wet * copyR) / (1.0 + 0.3 * wet));
        m_write = (m_write + 1) & (kSize - 1);
    }
}

} // namespace krimson
