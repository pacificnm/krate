#include "dsp/YellowDrive.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace krunch {

namespace {

constexpr double kDriveMinOhms = 33e3;   // in series with the Drive pot
constexpr double kDrivePotOhms = 1e6;
constexpr double kLegOhms = 4.7e3;       // the ground leg: gain = 1 + R / 4.7k
constexpr double kFeedbackCap = 51e-12;
constexpr double kLegHz = 720.0;         // 4.7k with 47 nF
constexpr double kToneHz = 1000.0;
// The clipped part's ceilings, in full-scale units: two diodes in series one
// way, one the other.
constexpr double kUpVolts = 1.2;
constexpr double kDownVolts = 0.6;

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string levelText(float v)
{
    const double db = std::round((-24.0 + 36.0 * double(v)) * 10.0) / 10.0 + 0.0;
    return std::format("{:+.1f} dB", db);
}

// The Drive pot: audio taper.
double driveOhms(float drive)
{
    const double t = std::clamp(double(drive), 0.0, 1.0);
    return kDriveMinOhms + kDrivePotOhms * (std::exp(4.0 * t) - 1.0) / (std::exp(4.0) - 1.0);
}

// Soft clipping, harder on the way down (one diode) than up (two).
double clip(double x)
{
    return x >= 0.0 ? kUpVolts * std::tanh(x / kUpVolts) : kDownVolts * std::tanh(x / kDownVolts);
}

} // namespace

const PedalInfo YellowDrive::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"drive", "DRIVE", 0.5f, &tenth},
        {"tone", "TONE", 0.5f, &tenth},
        {"level", "LEVEL", 0.5f, &levelText},
    };
    return PedalInfo{"yellow_drive", "Yellow Drive", "#E3B81E", knobs, false};
}();

void YellowDrive::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    const double up = sampleRate * Oversampler4x::kFactor;
    m_input.setCutoff(sampleRate, 20.0);
    m_legHighpass.setCutoff(up, kLegHz);
    m_toneLowpass.setCutoff(sampleRate, kToneHz);
    m_output.setCutoff(sampleRate, 15.0);
    m_drive = -1.0f;
}

void YellowDrive::resetEffect() noexcept
{
    m_input.reset();
    m_legHighpass.reset();
    m_feedbackLowpass.reset();
    m_oversampler.reset();
    m_toneLowpass.reset();
    m_output.reset();
    m_level = -1.0f;
}

void YellowDrive::retune(float drive) noexcept
{
    m_drive = drive;
    const double r = driveOhms(drive);
    m_gain = r / kLegOhms;
    const double rolloff = 1.0 / (2.0 * std::numbers::pi * r * kFeedbackCap);
    m_feedbackLowpass.setCutoff(m_rate * Oversampler4x::kFactor, rolloff);
}

void YellowDrive::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float drive = knob(Drive); drive != m_drive)
        retune(drive);
    // Tone: how much of what the 1 kHz low-pass took comes back: dark at 0,
    // about -4 dB of treble at noon, bright (+4 dB) at 10.
    const float treble = 0.1f + 1.5f * std::pow(knob(Tone), 1.5f);
    const float target = float(std::pow(10.0, (-24.0 + 36.0 * double(knob(Level))) / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);

    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float x = m_input.highpass(buffer[n]);
        m_oversampler.up(x, up);
        for (float &u : up) {
            const double extra = m_feedbackLowpass.lowpass(float(m_gain * m_legHighpass.highpass(u)));
            u = float(u + clip(extra));
        }
        const float driven = m_output.highpass(m_oversampler.down(up));
        const float low = m_toneLowpass.lowpass(driven);
        m_level += step;
        buffer[n] = (low + treble * (driven - low)) * m_level;
    }
    m_level = target;
}

} // namespace krunch
