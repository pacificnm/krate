#include "dsp/ModPedals.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace krunch {

namespace {

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }

constexpr double kMinDelayMs = 0.25;
constexpr double kMaxDelayMs = 10.0;
constexpr double kMaxFeedback = 0.85;

double flangerHz(float v) { return 0.05 * std::pow(60.0, double(v)); } // 0.05 .. 3 Hz: slow, as the original
std::string flangerRate(float v) { return std::format("{:.2f} Hz", flangerHz(v)); }
std::string rangeText(float v) { return std::format("{:.1f} ms", Flanger::delayMs(v, 1.0)); }
std::string matrixText(float v) { return v >= 0.5f ? "Matrix" : "Sweep"; }
std::string phaserSpeed(float v) { return std::format("{:.2f} Hz", Phaser::speedHz(v)); }
std::string phaserMode(float v) { return v >= 0.5f ? "Deep" : "Smooth"; }

// A Hermite (4-point) read between samples: smooth enough for a short,
// fast-moving delay, where straight lines would dull and buzz the top.
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

// --- Flanger ----------------------------------------------------------------------

const PedalInfo Flanger::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"rate", "RATE", 0.35f, &flangerRate},
        {"range", "RANGE", 0.6f, &rangeText},
        {"color", "COLOR", 0.55f, &tenth},
        {"matrix", "MATRIX", 0.0f, &matrixText, 2},
    };
    return PedalInfo{"flanger", "Flanger", "#B8B6B0", knobs, false};
}();

double Flanger::delayMs(float range, double sweep)
{
    // Range sets the top of the sweep (1 .. 10 ms); the sweep runs from the
    // shortest delay up to it, evenly in pitch.
    const double top = 1.0 * std::pow(kMaxDelayMs / 1.0, std::clamp(double(range), 0.0, 1.0));
    return kMinDelayMs * std::pow(top / kMinDelayMs, std::clamp(sweep, 0.0, 1.0));
}

void Flanger::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_smoothing = 1.0 - std::exp(-1.0 / (0.02 * sampleRate)); // hand moves glide over ~20 ms
    m_chip.setLowpass(sampleRate, 8000.0, 0.6);
}

void Flanger::resetEffect() noexcept
{
    m_line.fill(0.0f);
    m_write = 0;
    m_position = -1.0;
    m_feedback = 0.0f;
    m_chip.reset();
}

void Flanger::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    const bool matrix = knob(Matrix) >= 0.5f;
    const float range = knob(Range);
    const double increment = flangerHz(knob(Rate)) / m_rate;
    const float feedback = float(kMaxFeedback * double(knob(Color)));
    for (std::uint32_t n = 0; n < frames; ++n) {
        // Where in the sweep: the triangle, or (Filter Matrix) Range by hand,
        // over the whole 0.25 .. 10 ms.
        double target;
        float top = range;
        if (matrix) {
            target = std::clamp(double(range), 0.0, 1.0);
            top = 1.0f;
        } else {
            target = m_phase < 0.5 ? 2.0 * m_phase : 2.0 - 2.0 * m_phase;
            m_phase += increment;
            if (m_phase >= 1.0)
                m_phase -= 1.0;
        }
        if (m_position < 0.0)
            m_position = target;
        m_position += (target - m_position) * m_smoothing;

        const double delay = delayMs(top, m_position) * 0.001 * m_rate;
        const double back = double(m_write) - delay + double(kSize);
        const auto i = std::size_t(back);
        const float t = float(back - double(i));
        const float wet = m_chip.process(hermite(m_line[(i - 1) & (kSize - 1)], m_line[i & (kSize - 1)],
                                                 m_line[(i + 1) & (kSize - 1)], m_line[(i + 2) & (kSize - 1)], t));
        const float x = buffer[n];
        m_line[m_write] = x + feedback * wet;
        m_write = (m_write + 1) & (kSize - 1);
        // Half and half: the comb's notches cancel fully; scaled so the peaks don't jump out.
        buffer[n] = 0.7f * (x + wet);
    }
}

// --- Phaser -----------------------------------------------------------------------

const PedalInfo Phaser::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"speed", "SPEED", 0.35f, &phaserSpeed},
        {"mode", "MODE", 0.0f, &phaserMode, 2},
    };
    return PedalInfo{"phaser", "Phaser", "#D9741F", knobs, false};
}();

double Phaser::speedHz(float speed) { return 0.1 * std::pow(80.0, std::clamp(double(speed), 0.0, 1.0)); }

void Phaser::prepareEffect(double sampleRate) noexcept { m_rate = sampleRate; }

void Phaser::resetEffect() noexcept
{
    m_state.fill(0.0);
    m_last = 0.0;
}

void Phaser::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    const double increment = speedHz(knob(Speed)) / m_rate;
    const double feedback = knob(Mode) >= 0.5f ? 0.6 : 0.0;
    constexpr double kLowHz = 140.0, kHighHz = 1600.0;
    for (std::uint32_t n = 0; n < frames; ++n) {
        // The LFO: a sine, bent (to the power 1.5) so it lingers low, as a FET's sweep does.
        const double lfo = std::pow(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * m_phase), 1.5);
        m_phase += increment;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
        const double fc = kLowHz * std::pow(kHighHz / kLowHz, lfo);
        const double k = std::tan(std::numbers::pi * std::min(fc, 0.45 * m_rate) / m_rate);
        const double a = (k - 1.0) / (k + 1.0); // first-order all-pass coefficient

        const double x = buffer[n];
        double y = x + feedback * m_last;
        for (double &z : m_state) { // y = a x + x[n-1] - a y[n-1], as transposed form
            const double out = a * y + z;
            z = y - a * out;
            y = out;
        }
        m_last = y;
        buffer[n] = float(0.5 * (x + y) * (feedback > 0.0 ? 0.8 : 1.0)); // deep: trimmed a little so it isn't louder overall
    }
}

} // namespace krunch
