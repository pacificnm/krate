#include "kalliope/dsp/Chorus.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace kalliope {

namespace {
struct ChorusMode
{
    double rateHz, centreMs, depthMs, wet;
};
// I: slow and gentle; II: faster, deeper, a little more of the copy.
constexpr ChorusMode kModes[] = {{0.0, 0.0, 0.0, 0.0}, {0.5, 3.6, 1.6, 0.8}, {0.83, 3.6, 2.4, 0.9}};
constexpr double kChipHz = 7000.0;       // the bucket brigade's darkening
constexpr double kChipCeiling = 1.4;     // its soft saturation (gentle below ~0.7)

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

void Chorus::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_tone = std::exp(-2.0 * std::numbers::pi * kChipHz / sampleRate);
    reset();
}

void Chorus::reset() noexcept
{
    m_line.fill(0.0f);
    m_write = 0;
    m_phase = 0.0;
    m_inTone = m_toneL1 = m_toneL2 = m_toneR1 = m_toneR2 = 0.0;
}

float Chorus::read(double delaySamples) const noexcept
{
    const double back = double(m_write) - delaySamples + double(kSize);
    const auto i = std::size_t(back);
    const auto t = float(back - double(i));
    return hermite(m_line[(i - 1) & (kSize - 1)], m_line[i & (kSize - 1)], m_line[(i + 1) & (kSize - 1)],
                   m_line[(i + 2) & (kSize - 1)], t);
}

void Chorus::process(const float *in, float *left, float *right, std::uint32_t frames, int mode) noexcept
{
    mode = std::clamp(mode, 0, 2);
    if (mode != m_lastMode && m_lastMode == Off)
        reset(); // switched on: no stale copies from before
    m_lastMode = mode;
    if (mode == Off) {
        if (right != in)
            std::copy_n(in, frames, right);
        if (left != in)
            std::copy_n(in, frames, left);
        return;
    }
    const ChorusMode &m = kModes[mode];
    const double ms = m_rate / 1000.0;
    const double step = m.rateHz / m_rate;
    // Dry and copy summed, scaled back so the overall level stays about the dry's.
    const double norm = 1.0 / (1.0 + 0.2 * m.wet);
    for (std::uint32_t n = 0; n < frames; ++n) {
        const double dry = double(in[n]);
        // Into the chips: darkened (the anti-alias filter) and softly saturated.
        m_inTone = dry + m_tone * (m_inTone - dry);
        m_line[m_write] = float(kChipCeiling * std::tanh(m_inTone / kChipCeiling));

        const double sweep = triangle(m_phase);
        const double wetL = read((m.centreMs + m.depthMs * sweep) * ms);
        const double wetR = read((m.centreMs - m.depthMs * sweep) * ms);
        m_phase += step;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
        // Out of the chips: the reconstruction filter, two poles.
        m_toneL1 = wetL + m_tone * (m_toneL1 - wetL);
        m_toneL2 = m_toneL1 + m_tone * (m_toneL2 - m_toneL1);
        m_toneR1 = wetR + m_tone * (m_toneR1 - wetR);
        m_toneR2 = m_toneR1 + m_tone * (m_toneR2 - m_toneR1);

        left[n] = float((dry + m.wet * m_toneL2) * norm);
        right[n] = float((dry + m.wet * m_toneR2) * norm);
        m_write = (m_write + 1) & (kSize - 1);
    }
}

} // namespace kalliope
