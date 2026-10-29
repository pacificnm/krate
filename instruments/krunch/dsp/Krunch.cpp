#include "dsp/Krunch.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace units {
int input(float v) { return v >= 0.5f ? 1 : 0; }
double levelDb(float v) { return kLevelMinDb + double(v) * (kLevelMaxDb - kLevelMinDb); }
double levelGain(float v) { return v <= 0.0f ? 0.0 : std::pow(10.0, levelDb(v) / 20.0); }
} // namespace units

namespace format {
std::string input(float v) { return std::format("In {}", units::input(v) + 1); }
std::string level(float v)
{
    if (v <= 0.0f)
        return "-inf dB";
    const double db = std::round(units::levelDb(v) * 10.0) / 10.0 + 0.0; // + 0.0: never "-0.0"
    return std::format("{:+.1f} dB", db);
}
} // namespace format

namespace {
void holdPeak(std::atomic<float> &held, float peak) noexcept
{
    if (peak > held.load(std::memory_order_relaxed))
        held.store(peak, std::memory_order_relaxed);
}
} // namespace

void Krunch::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
}

void Krunch::process(const krate::AudioBlock &block) noexcept
{
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    const std::uint32_t frames = block.frames;
    const float *in = block.inputs ? block.inputs[units::input(m_params.get(int(ParamId::Input)))] : nullptr;
    if (!in || frames == 0) {
        std::fill_n(left, frames, 0.0f);
        std::fill_n(right, frames, 0.0f);
        return;
    }

    // Ramp the level across the block so knob moves don't click.
    const float target = float(units::levelGain(m_params.get(int(ParamId::Level))));
    if (m_gain < 0.0f)
        m_gain = target;
    const float step = (target - m_gain) / float(frames);
    float inPeak = 0.0f, outPeak = 0.0f;
    for (std::uint32_t s = 0; s < frames; ++s) {
        m_gain += step;
        const float x = in[s];
        const float y = x * m_gain;
        left[s] = y;
        right[s] = y;
        inPeak = std::max(inPeak, std::fabs(x));
        outPeak = std::max(outPeak, std::fabs(y));
    }
    m_gain = target;
    holdPeak(m_inputPeak, inPeak);
    holdPeak(m_outputPeak, outPeak);
}

} // namespace krunch
