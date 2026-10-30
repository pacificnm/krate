#include "kobalt/dsp/Filter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace kobalt {

namespace {
// The feedback's soft ceiling: below about half of it the path is linear.
constexpr double kFeedbackCeiling = 1.4;
// How hard a stage's input is before its soft limit: 1/kStageHeadroom is the
// level where it starts to bend.
constexpr double kStageHeadroom = 0.6;

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

void Filter::set(double cutoffHz, double k, double sampleRate) noexcept
{
    const double fc = std::clamp(cutoffHz, 5.0, 0.42 * sampleRate);
    const double g = std::tan(std::numbers::pi * fc / sampleRate);
    m_G = g / (1.0 + g);
    m_invOne = 1.0 / (1.0 + g);
    m_k = std::max(k, 0.0);
    m_makeup = std::sqrt(1.0 + 0.6 * m_k); // most of the passband lost to feedback comes back
}

float Filter::process(float in) noexcept
{
    const double G = m_G;
    // Each stage: y = G*x + s/(1+g). The 4th stage's output as a function of the input:
    const double S = G * G * G * m_s[0] * m_invOne + G * G * m_s[1] * m_invOne + G * m_s[2] * m_invOne
                     + m_s[3] * m_invOne;
    const double G4 = G * G * G * G;
    const double x = double(in) * m_makeup;
    const double y4 = (G4 * x + S) / (1.0 + m_k * G4);
    double stageIn = x - m_k * kFeedbackCeiling * softClip(y4 / kFeedbackCeiling);

    for (double &s : m_s) {
        const double limited = softClip(stageIn * kStageHeadroom) / kStageHeadroom;
        const double v = (limited - s) * G;
        const double y = v + s;
        s = y + v;
        stageIn = y;
    }
    return float(stageIn);
}

} // namespace kobalt
