#include "dsp/DiodeLadder.h"

#include <cmath>
#include <numbers>

namespace korrode {

void DiodeLadder::reset() noexcept
{
    for (auto &s : m_stage) {
        s.z1 = 0.0;
        s.feedback = 0.0;
    }
}

void DiodeLadder::setCoefficients(double cutoffHz, double sampleRate, double k) noexcept
{
    // Bilinear prewarp: one-pole TPT integrator gain.
    const double g = std::tan(std::numbers::pi * cutoffHz / sampleRate);

    // Stage coupling terms, solved from the last stage backwards.
    const double G4 = 0.5 * g / (1.0 + g);
    const double G3 = 0.5 * g / (1.0 + g - 0.5 * g * G4);
    const double G2 = 0.5 * g / (1.0 + g - 0.5 * g * G3);
    const double G1 = g / (1.0 + g - g * G2);

    m_gamma = G4 * G3 * G2 * G1;
    m_sg1 = G4 * G3 * G2;
    m_sg2 = G4 * G3;
    m_sg3 = G4;

    const double alpha = g / (1.0 + g);
    for (auto &s : m_stage)
        s.alpha = alpha;

    m_stage[0].beta = 1.0 / (1.0 + g - g * G2);
    m_stage[1].beta = 1.0 / (1.0 + g - 0.5 * g * G3);
    m_stage[2].beta = 1.0 / (1.0 + g - 0.5 * g * G4);
    m_stage[3].beta = 1.0 / (1.0 + g);

    m_stage[0].gamma = 1.0 + G1 * G2;
    m_stage[1].gamma = 1.0 + G2 * G3;
    m_stage[2].gamma = 1.0 + G3 * G4;
    m_stage[3].gamma = 1.0;

    m_stage[0].delta = g;
    m_stage[1].delta = 0.5 * g;
    m_stage[2].delta = 0.5 * g;
    m_stage[3].delta = 0.0;

    m_stage[0].epsilon = G2;
    m_stage[1].epsilon = G3;
    m_stage[2].epsilon = G4;
    m_stage[3].epsilon = 0.0;

    m_stage[0].a0 = 1.0;
    m_stage[1].a0 = 0.5;
    m_stage[2].a0 = 0.5;
    m_stage[3].a0 = 0.5;

    m_k = k;
}

double DiodeLadder::process(double x) noexcept
{
    // Each stage sees the next one's state (the diodes load their neighbours).
    m_stage[3].feedback = 0.0;
    m_stage[2].feedback = m_stage[3].feedbackOutput();
    m_stage[1].feedback = m_stage[2].feedbackOutput();
    m_stage[0].feedback = m_stage[1].feedbackOutput();

    const double sigma = m_sg1 * m_stage[0].feedbackOutput() + m_sg2 * m_stage[1].feedbackOutput()
                       + m_sg3 * m_stage[2].feedbackOutput() + m_stage[3].feedbackOutput();

    // Zero-delay feedback: solve the loop for the ladder input instantaneously,
    // then saturate it, as the diodes and transistors at the ladder's input do:
    // a loud resonant peak compresses and growls instead of growing.
    const double u = kInputCeiling * std::tanh((x - m_k * sigma) / (1.0 + m_k * m_gamma) / kInputCeiling);

    double y = m_stage[0].process(u);
    y = m_stage[1].process(y);
    y = m_stage[2].process(y);
    return m_stage[3].process(y);
}

} // namespace Korrode
