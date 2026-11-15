#include "dsp/TaigaFuzz.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {

constexpr double kBoost = 4.0;        // the input booster
constexpr double kBoostHeadroom = 2.0; // where it starts to round off (diode volts: 0.6)
constexpr double kMinSustain = 1.0;   // the first clipping stage's gain, SUSTAIN at 0 ...
constexpr double kMaxSustain = 200.0; // ... and at 10 (the pot's log taper between)
constexpr double kSecondStage = 12.0; // the second clipping stage's fixed gain
constexpr double kDiode = 0.6;        // two silicon diodes, back to back
constexpr double kCoupleHz = 40.0;    // the green voicing's large coupling capacitors
constexpr double kRolloffHz = 3200.0; // 100k with 500 pF in each stage's feedback
constexpr double kToneLowHz = 300.0;
constexpr double kToneHighHz = 1500.0;
constexpr float kFlat = 0.10f;        // the part of the signal past the tone stack: a shallower scoop
constexpr double kMakeup = 1.5;       // the recovery stage: VOLUME at noon about 3 dB over bypass

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
double volumeDb(float v) { return -30.0 + 36.0 * double(v); }
std::string volumeText(float v) { return std::format("{:+.1f} dB", std::round(volumeDb(v) * 10.0) / 10.0 + 0.0); }

// Two diodes each way: soft, symmetric, settling at kDiode.
double diodes(double x)
{
    constexpr double p = 2.5;
    return x / std::pow(1.0 + std::pow(std::fabs(x) / kDiode, p), 1.0 / p);
}

} // namespace

const PedalInfo TaigaFuzz::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"volume", "VOLUME", 0.5f, &volumeText},
        {"tone", "TONE", 0.5f, &tenth},
        {"sustain", "SUSTAIN", 0.6f, &tenth},
    };
    return PedalInfo{"taiga_fuzz", "Taiga Fuzz", "#4E5B31", knobs, false};
}();

void TaigaFuzz::prepareEffect(double sampleRate) noexcept
{
    const double up = sampleRate * Oversampler4x::kFactor;
    m_input.setCutoff(sampleRate, 20.0);
    m_couple1.setCutoff(up, kCoupleHz);
    m_couple2.setCutoff(up, kCoupleHz);
    m_rolloff1.setCutoff(up, kRolloffHz);
    m_rolloff2.setCutoff(up, kRolloffHz);
    m_toneLow.setCutoff(sampleRate, kToneLowHz);
    m_toneHigh.setCutoff(sampleRate, kToneHighHz);
    m_output.setCutoff(sampleRate, 20.0);
}

void TaigaFuzz::resetEffect() noexcept
{
    for (OnePole *f : {&m_input, &m_couple1, &m_couple2, &m_rolloff1, &m_rolloff2, &m_toneLow, &m_toneHigh, &m_output})
        f->reset();
    m_oversampler.reset();
    m_level = -1.0f;
}

void TaigaFuzz::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    const double sustain = kMinSustain * std::pow(kMaxSustain / kMinSustain, double(knob(Sustain)));
    const float tone = knob(Tone); // 0: the low-pass alone (dark), 1: the high-pass alone (bright)
    const float target = float(kMakeup * std::pow(10.0, volumeDb(knob(Volume)) / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);

    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_oversampler.up(m_input.highpass(buffer[n]), up);
        for (float &u : up) {
            double x = kBoostHeadroom * std::tanh(kBoost * u / kBoostHeadroom);
            x = diodes(m_rolloff1.lowpass(float(sustain * m_couple1.highpass(float(x)))));
            x = diodes(m_rolloff2.lowpass(float(kSecondStage * m_couple2.highpass(float(x)))));
            u = float(x);
        }
        const float fuzz = m_oversampler.down(up);
        const float low = m_toneLow.lowpass(fuzz);
        const float high = fuzz - m_toneHigh.lowpass(fuzz);
        const float stack = kFlat * fuzz + (1.0f - kFlat) * ((1.0f - tone) * low + tone * high);
        m_level += step;
        buffer[n] = m_output.highpass(stack) * m_level;
    }
    m_level = target;
}

} // namespace krunch
