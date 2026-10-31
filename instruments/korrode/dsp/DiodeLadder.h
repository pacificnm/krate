#pragma once

namespace korrode {

// Four-stage diode ladder lowpass in zero-delay-feedback (TPT) form, after the
// virtual-analog diode ladder structure published by Will Pirkle. Unlike a
// transistor ladder, neighbouring stages load each other, which gives the
// gentler slope and the characteristic resonance. Linear core; the caller
// applies input saturation. Run it oversampled (see Voice).
//
// Measured behaviour (tests/tst_dsp.cpp): passband gain is 1 / (1 + k), and
// the resonant peak sits at about 0.67 x cutoffHz.
class DiodeLadder
{
public:
    static constexpr double kInputCeiling = 1.2; // the ladder input's soft ceiling (docs/DSP.md#filter-diode-ladder)
    // Feedback at which this structure self-oscillates.
    static constexpr double kSelfOscillation = 17.0;

    void reset() noexcept;

    // cutoffHz must stay below 0.5 * sampleRate. k: feedback 0..kSelfOscillation.
    // Cheap enough to call once per base-rate sample.
    void setCoefficients(double cutoffHz, double sampleRate, double k) noexcept;

    double process(double x) noexcept;

private:
    struct Stage
    {
        double alpha = 0.0;
        double beta = 0.0;
        double gamma = 1.0;
        double delta = 0.0;
        double epsilon = 0.0;
        double a0 = 1.0;
        double z1 = 0.0;
        double feedback = 0.0;

        double feedbackOutput() const noexcept { return beta * (z1 + feedback * delta); }

        double process(double x) noexcept
        {
            x = x * gamma + feedback + epsilon * feedbackOutput();
            const double v = (a0 * x - z1) * alpha;
            const double lp = v + z1;
            z1 = v + lp;
            return lp;
        }
    };

    Stage m_stage[4];
    double m_gamma = 0.0; // instantaneous gain of the whole ladder
    double m_sg1 = 0.0;   // per-stage contributions to the feedback sum
    double m_sg2 = 0.0;
    double m_sg3 = 0.0;
    double m_k = 0.0;
};

} // namespace Korrode
