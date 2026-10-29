#include "dsp/Fuzz.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string volumeText(float v)
{
    return std::format("{:+.1f} dB", std::round((-24.0 + 36.0 * double(v)) * 10.0) / 10.0 + 0.0);
}
std::string biasText(float v) { return std::abs(v - 0.5f) < 0.04f ? "Centre" : std::format("{:+.1f}", (double(v) - 0.5) * 10.0); }
std::string typeText(float v) { return v >= 0.5f ? "Silicon" : "Germanium"; }

// The transistor: it saturates softly one way (up to 1) and cuts off sooner
// the other (down to -0.45), both with the same slope at rest. Germanium's
// knee is softer (x / (1 + |x|) rather than tanh).
double transistor(double u, bool silicon)
{
    const auto curve = [silicon](double v) { return silicon ? std::tanh(v) : v / (1.0 + std::fabs(v)); };
    return u >= 0.0 ? curve(u) : 0.45 * curve(u / 0.45);
}

} // namespace

const PedalInfo RoundFuzz::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"fuzz", "FUZZ", 0.7f, &tenth},
        {"volume", "VOLUME", 0.55f, &volumeText},
        {"bias", "BIAS", 0.5f, &biasText},
        {"type", "TYPE", 0.0f, &typeText, 2},
    };
    return PedalInfo{"round_fuzz", "Round Fuzz", "#7E1A22", knobs, false};
}();

void RoundFuzz::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_input.setCutoff(sampleRate, 25.0);
    m_output.setCutoff(sampleRate, 20.0);
    m_type = -1;
}

void RoundFuzz::resetEffect() noexcept
{
    m_input.reset();
    m_between.reset();
    m_rolloff.reset();
    m_oversampler.reset();
    m_output.reset();
    m_volume = -1.0f;
}

void RoundFuzz::retune(bool silicon) noexcept
{
    m_type = silicon ? 1 : 0;
    const double up = m_rate * Oversampler4x::kFactor;
    m_between.setCutoff(up, silicon ? 60.0 : 40.0);
    m_rolloff.setCutoff(up, silicon ? 7500.0 : 4500.0);
}

void RoundFuzz::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    const bool silicon = knob(Type) >= 0.5f;
    if (int(silicon) != m_type)
        retune(silicon);
    // FUZZ: the first stage's drive, +6 .. +40 dB (germanium a little less).
    const double drive = std::pow(10.0, ((silicon ? 6.0 : 4.0) + (silicon ? 34.0 : 30.0) * double(knob(Fuzz))) / 20.0);
    const double secondGain = silicon ? 4.0 : 3.0;
    // BIAS: the operating point, as a shift into each clipper (removed after, so only the shape changes).
    const double bias = (double(knob(Bias)) - 0.5) * 1.6;
    const double rest1 = transistor(bias, silicon), rest2 = transistor(0.5 * bias, silicon);
    // The output level: fuzz is loud; trimmed so the default Volume kicks in a
    // few dB over bypass with a guitar, silicon (hotter) brought down to match germanium.
    const float target = float(std::pow(10.0, (-24.0 + 36.0 * double(knob(Volume))) / 20.0) * (silicon ? 0.27 : 0.45));
    if (m_volume < 0.0f)
        m_volume = target;
    const float step = (target - m_volume) / float(frames);

    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_oversampler.up(m_input.highpass(buffer[n]), up);
        for (float &u : up) {
            const double first = transistor(drive * u + bias, silicon) - rest1;
            const double between = m_between.highpass(float(first));
            const double second = transistor(secondGain * between + 0.5 * bias, silicon) - rest2;
            u = m_rolloff.lowpass(float(second));
        }
        m_volume += step;
        buffer[n] = m_output.highpass(m_oversampler.down(up)) * m_volume;
    }
    m_volume = target;
}

} // namespace krunch
