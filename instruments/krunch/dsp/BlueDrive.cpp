#include "dsp/BlueDrive.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {

constexpr double kCouplingHz = 90.0;   // into each stage: full range, the boom kept out
constexpr double kRolloffHz = 7000.0;  // each stage's treble roll-off
constexpr double kStageMinDb = 2.0;    // each stage's gain, GAIN at 0 ...
constexpr double kStageMaxDb = 26.0;   // ... and at 10 (two stages: 4 ... 52 dB)
constexpr double kBias = 0.12;         // the transistors' lean: a few even harmonics
constexpr double kDiodeVolts = 0.55;   // the diodes to ground, in full-scale units

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string levelText(float v)
{
    const double db = std::round((-24.0 + 36.0 * double(v)) * 10.0) / 10.0 + 0.0;
    return std::format("{:+.1f} dB", db);
}

// Diodes to ground: firmer than the stage's own soft clipping, still rounded.
double diodes(double x)
{
    const double u = x / kDiodeVolts;
    return kDiodeVolts * u / std::pow(1.0 + std::pow(std::fabs(u), 2.5), 1.0 / 2.5);
}

} // namespace

const PedalInfo BlueDrive::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"level", "LEVEL", 0.5f, &levelText},
        {"tone", "TONE", 0.5f, &tenth},
        {"gain", "GAIN", 0.4f, &tenth},
    };
    return PedalInfo{"blue_drive", "Blue Drive", "#2D5FB4", knobs, false};
}();

float BlueDrive::Stage::process(float x, double gain, double bias) noexcept
{
    const double in = gain * coupling.highpass(x);
    // Soft clipping in the stage's feedback, leaning a little one way; the
    // lean's own offset taken back off, so silence stays silent.
    const double soft = std::tanh(in + bias) - std::tanh(bias);
    return float(diodes(rolloff.lowpass(float(soft))));
}

void BlueDrive::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    const double up = sampleRate * Oversampler4x::kFactor;
    m_input.setCutoff(sampleRate, 20.0);
    for (Stage *s : {&m_first, &m_second}) {
        s->coupling.setCutoff(up, kCouplingHz);
        s->rolloff.setCutoff(up, kRolloffHz);
    }
    m_output.setCutoff(sampleRate, 15.0);
    m_gainKnob = m_toneKnob = -1.0f;
}

void BlueDrive::resetEffect() noexcept
{
    m_input.reset();
    m_first.reset();
    m_second.reset();
    m_oversampler.reset();
    m_output.reset();
    m_toneLow.reset();
    m_toneHigh.reset();
    m_level = -1.0f;
}

void BlueDrive::retune(float gain, float tone) noexcept
{
    m_gainKnob = gain;
    m_toneKnob = tone;
    // GAIN: audio-ish taper, in dB per stage.
    const double t = std::clamp(double(gain), 0.0, 1.0);
    m_gain = std::pow(10.0, (kStageMinDb + (kStageMaxDb - kStageMinDb) * std::pow(t, 0.8)) / 20.0);
    // TONE tilts around noon: treble -12 ... +10 dB above about 1.2 kHz, the
    // lows the other way a little (+3 ... -3 dB below 250 Hz).
    const double u = std::clamp(double(tone), 0.0, 1.0) * 2.0 - 1.0;
    m_toneHigh.setHighShelf(m_rate, 1200.0, u < 0.0 ? 12.0 * u : 10.0 * u);
    m_toneLow.setLowShelf(m_rate, 250.0, -3.0 * u);
}

void BlueDrive::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float g = knob(Gain), t = knob(Tone); g != m_gainKnob || t != m_toneKnob)
        retune(g, t);
    // The diodes cap each stage near 0.55: bring that back to about a guitar's
    // level, so LEVEL at noon is about as loud as the guitar on its own.
    constexpr double kMakeup = 1.2;
    const float target = float(kMakeup * std::pow(10.0, (-24.0 + 36.0 * double(knob(Level))) / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);

    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_oversampler.up(m_input.highpass(buffer[n]), up);
        for (float &u : up)
            u = m_second.process(m_first.process(u, m_gain, kBias), m_gain, -kBias);
        const float driven = m_output.highpass(m_oversampler.down(up));
        m_level += step;
        buffer[n] = m_toneHigh.process(m_toneLow.process(driven)) * m_level;
    }
    m_level = target;
}

} // namespace krunch
