#include "kalliope/dsp/Filters.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace kalliope {

namespace {
// The feedback's soft ceiling: below about half of it the path is linear.
constexpr double kFeedbackCeiling = 1.6;

// A cheap tanh (Pade 3/2), exact at 0 and smooth to +-1 at +-3.
inline double softClip(double x) noexcept
{
    if (x <= -3.0)
        return -1.0;
    if (x >= 3.0)
        return 1.0;
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}
} // namespace

void Ladder::set(double cutoffHz, double k, double sampleRate, double drive) noexcept
{
    const double fc = std::clamp(cutoffHz, 5.0, 0.42 * sampleRate);
    const double g = std::tan(std::numbers::pi * fc / sampleRate);
    m_G = g / (1.0 + g);
    m_invOne = 1.0 / (1.0 + g);
    m_k = std::max(k, 0.0);
    m_makeup = std::sqrt(1.0 + m_k); // part of the passband lost to feedback comes back
    if (drive != m_drive) {
        m_drive = std::max(drive, 1.0);
        m_trim = 1.0 / std::sqrt(m_drive);
    }
}

float Ladder::process(float in, bool slope24) noexcept
{
    const double G = m_G;
    // Each stage: y = G*x + s/(1+g). The 4th stage's output as a function of the input:
    const double S = G * G * G * m_s[0] * m_invOne + G * G * m_s[1] * m_invOne + G * m_s[2] * m_invOne
                     + m_s[3] * m_invOne;
    const double G4 = G * G * G * G;
    const double x = double(in) * m_makeup * m_drive;
    const double y4 = (G4 * x + S) / (1.0 + m_k * G4);
    const double u = std::tanh(x - m_k * kFeedbackCeiling * softClip(y4 / kFeedbackCeiling));

    double stageIn = u;
    double out2 = 0.0, out4 = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double v = (stageIn - m_s[i]) * G;
        const double y = v + m_s[i];
        m_s[i] = y + v;
        stageIn = y;
        if (i == 1)
            out2 = y;
        out4 = y;
    }
    return float((slope24 ? out4 : out2) * m_trim);
}

void HighPass::set(double cutoffHz, double sampleRate) noexcept
{
    m_bypass = cutoffHz <= 0.0;
    if (m_bypass)
        return;
    const double g = std::tan(std::numbers::pi * std::min(cutoffHz, 0.42 * sampleRate) / sampleRate);
    m_G = g / (1.0 + g);
}

float HighPass::process(float in) noexcept
{
    if (m_bypass)
        return in;
    const double v = (double(in) - m_s) * m_G;
    const double lp = v + m_s;
    m_s = lp + v;
    return float(double(in) - lp);
}

} // namespace kalliope
