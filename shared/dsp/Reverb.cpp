#include "dsp/Reverb.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace krate {

namespace {
// Line lengths in milliseconds: mutually prime-ish, 30..75 ms.
constexpr std::array<double, 8> kLengthsMs{29.7, 37.1, 41.1, 43.7, 53.9, 59.3, 67.9, 73.1};
constexpr double kDampHz = 6000.0;   // the tail darkens as it decays
constexpr double kInputGain = 0.35;
constexpr double kOutputGain = 0.6;
} // namespace

Reverb::Reverb(double maxSampleRate)
{
    for (int i = 0; i < kLines; ++i)
        m_lines[std::size_t(i)].assign(std::size_t(kLengthsMs[std::size_t(i)] / 1000.0 * maxSampleRate) + 2, 0.0f);
    prepare(48000.0);
}

double Reverb::decaySeconds(double size) noexcept
{
    return 0.4 * std::pow(20.0, std::clamp(size, 0.0, 1.0));
}

void Reverb::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    for (int i = 0; i < kLines; ++i) {
        const auto wanted = std::size_t(kLengthsMs[std::size_t(i)] / 1000.0 * sampleRate);
        m_length[std::size_t(i)] = std::clamp<std::size_t>(wanted, 1, m_lines[std::size_t(i)].size() - 1);
    }
    m_dampCoefficient = 1.0 - std::exp(-2.0 * std::numbers::pi * kDampHz / sampleRate);
    m_lastSize = -1.0;
    clear();
}

void Reverb::clear() noexcept
{
    for (auto &line : m_lines)
        std::fill(line.begin(), line.end(), 0.0f);
    m_pos.fill(0);
    m_damp.fill(0.0);
}

void Reverb::process(float *left, float *right, std::uint32_t frames, double size, double level) noexcept
{
    if (size != m_lastSize) {
        // Each line loses 60 dB over the decay time: gain = 10^(-3 * length / (RT60 * rate)).
        const double rt60 = decaySeconds(size);
        for (int i = 0; i < kLines; ++i)
            m_gain[std::size_t(i)] = std::pow(10.0, -3.0 * double(m_length[std::size_t(i)]) / (rt60 * m_rate));
        m_lastSize = size;
    }
    for (std::uint32_t n = 0; n < frames; ++n) {
        std::array<double, kLines> out{};
        for (int i = 0; i < kLines; ++i) {
            const auto line = std::size_t(i);
            out[line] = m_lines[line][m_pos[line]];
        }
        // An 8x8 Hadamard mix (fast Walsh-Hadamard), normalized.
        std::array<double, kLines> mixed = out;
        for (int h = 1; h < kLines; h *= 2)
            for (int i = 0; i < kLines; i += 2 * h)
                for (int j = i; j < i + h; ++j) {
                    const double a = mixed[std::size_t(j)], b = mixed[std::size_t(j + h)];
                    mixed[std::size_t(j)] = a + b;
                    mixed[std::size_t(j + h)] = a - b;
                }
        const double input = kInputGain * 0.5 * (double(left[n]) + double(right[n]));
        for (int i = 0; i < kLines; ++i) {
            const auto line = std::size_t(i);
            double feedback = mixed[line] * (1.0 / std::sqrt(double(kLines))) * m_gain[line];
            m_damp[line] += (feedback - m_damp[line]) * m_dampCoefficient;
            feedback = m_damp[line];
            m_lines[line][m_pos[line]] = float(input + feedback);
            if (++m_pos[line] >= m_length[line])
                m_pos[line] = 0;
        }
        const double wetL = kOutputGain * (out[0] - out[2] + out[4] - out[6]) * 0.5;
        const double wetR = kOutputGain * (out[1] - out[3] + out[5] - out[7]) * 0.5;
        left[n] = float(double(left[n]) + level * wetL);
        right[n] = float(double(right[n]) + level * wetR);
    }
}

} // namespace krate
