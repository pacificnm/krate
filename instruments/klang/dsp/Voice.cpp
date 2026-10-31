#include "klang/dsp/Voice.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace klang {

namespace {
constexpr double kReleaseSeconds = 0.02;
constexpr double kChokeSeconds = 0.003;
constexpr double kMinAttackSeconds = 0.0005; // a region starting mid-wave doesn't click
constexpr double kEndFadeSeconds = 0.002;    // nor one ending mid-wave
constexpr double kSeamSeconds = 0.005;       // a loop's crossfade
constexpr int kFilterUpdate = 16;            // frames between filter retunes
constexpr double kLn1000 = 6.907755278982137;
} // namespace

void PadVoice::start(const Sample *sample, const PadSettings &s, float velocity, double pitchSemis, bool flip,
                     int note) noexcept
{
    m_sample = nullptr;
    if (!sample || sample->frames() < 2)
        return;
    const double frames = double(sample->frames());
    const double a = std::clamp(s.start, 0.0, 1.0) * frames;
    const double b = std::clamp(s.end, 0.0, 1.0) * frames;
    if (b - a < 2.0)
        return; // END at or before START: nothing to play
    m_sample = sample;
    m_regionStart = a;
    m_regionLength = b - a;
    m_pos = 0.0;
    m_reverse = s.reverse != flip;
    m_loop = s.mode == PadMode::Loop;
    m_oneShot = s.mode == PadMode::OneShot;
    m_pitch = pitchSemis;
    m_velocity = std::clamp(double(velocity), 0.0, 1.0);
    m_note = note;
    m_attackDone = 0.0;
    m_env = 1.0;
    m_released = false;
    m_fade = 1.0;
    m_fadeStep = 0.0;
    m_ic1[0] = m_ic1[1] = m_ic2[0] = m_ic2[1] = 0.0;
    m_hold = 0;
    m_held[0] = m_held[1] = 0.0f;
    m_ringPhase = 0.0;
}

void PadVoice::release() noexcept
{
    if (!active() || m_released)
        return;
    m_released = true;
    if (!m_oneShot && m_fadeStep == 0.0)
        m_fadeStep = 1.0 / (kReleaseSeconds * m_rate);
}

void PadVoice::choke() noexcept
{
    if (!active())
        return;
    m_released = true;
    m_fadeStep = std::max(m_fadeStep, 1.0 / (kChokeSeconds * m_rate));
}

float PadVoice::read(const float *channel, double position) const noexcept
{
    // position: frames into the region in play order (may run past either end: clamped to the sample).
    const double f = m_reverse ? m_regionStart + m_regionLength - 1.0 - position : m_regionStart + position;
    const auto last = std::ptrdiff_t(m_sample->frames()) - 1;
    const auto i = std::ptrdiff_t(std::floor(f));
    const float t = float(f - double(i));
    const auto at = [&](std::ptrdiff_t k) { return channel[std::clamp<std::ptrdiff_t>(k, 0, last)]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    // 4-point Hermite
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void PadVoice::render(const PadBuses &out, const float *gate, std::uint32_t frames, const PadSettings &s,
                      double bpm) noexcept
{
    if (!active())
        return;
    const Sample &sample = *m_sample;
    const float *left = sample.left.data();
    const float *right = sample.stereo() ? sample.right.data() : left;
    double rate = sample.rate / m_rate;
    if (s.syncBeats > 0 && bpm > 0.0)
        rate = m_regionLength / (double(s.syncBeats) * 60.0 / bpm * m_rate);
    rate *= std::exp2((s.tuneSemis + m_pitch) / 12.0);

    const double attackStep = 1.0 / (std::max(s.attack, kMinAttackSeconds) * m_rate);
    const double decayFactor = s.decay > 0.0 ? std::exp(-kLn1000 / (s.decay * m_rate)) : 1.0;
    const double seam = std::min(kSeamSeconds * sample.rate, m_regionLength / 4.0);
    const double endFade = kEndFadeSeconds * m_rate;
    const double velocityGain = 0.25 + 0.75 * m_velocity;
    const double ringStep = s.ringHz / m_rate;
    const double k = 2.0 * (1.0 - 0.97 * std::clamp(s.resonance, 0.0, 1.0));
    const double driveGain = 1.0 + 15.0 * s.drive * s.drive;
    const double driveNorm = 1.0 / std::tanh(driveGain);
    const double fold = std::clamp((s.drive - 0.5) * 2.0, 0.0, 1.0);
    const double quant = s.bits > 0 ? double(1 << (s.bits - 1)) : 0.0;

    for (std::uint32_t i = 0; i < frames; ++i) {
        // Where we are, and whether the hit is over.
        if (!m_loop && m_pos >= m_regionLength) {
            m_sample = nullptr;
            return;
        }
        double env = std::min(1.0, m_attackDone);
        m_attackDone += attackStep;
        if (m_attackDone >= 1.0)
            m_env *= decayFactor;
        env *= m_env;
        if (s.decay > 0.0 && m_attackDone >= 1.0 && m_env < 1e-4) {
            m_sample = nullptr;
            return;
        }
        if (m_fadeStep > 0.0) {
            m_fade -= m_fadeStep;
            if (m_fade <= 0.0) {
                m_sample = nullptr;
                return;
            }
        }
        if (!m_loop) // the region's last moments fade, so a cut mid-wave doesn't click
            env *= std::min(1.0, (m_regionLength - m_pos) / rate / endFade);

        // The sample (a loop crossfades its seam with what comes before its start).
        double l = read(left, m_pos), r = read(right, m_pos);
        if (m_loop && seam > 1.0 && m_pos > m_regionLength - seam) {
            const double t = (m_regionLength - m_pos) / seam;
            l = t * l + (1.0 - t) * read(left, m_pos - m_regionLength);
            r = t * r + (1.0 - t) * read(right, m_pos - m_regionLength);
        }
        m_pos += rate;
        if (m_loop && m_pos >= m_regionLength)
            m_pos -= m_regionLength;

        // Filter, swept by the envelope.
        if (s.filter != FilterType::Off) {
            if (i % kFilterUpdate == 0) {
                const double fc = std::clamp(s.cutoffHz * std::exp2(s.filterEnvOctaves * env), 10.0, 0.45 * m_rate);
                const double g = std::tan(std::numbers::pi * fc / m_rate);
                m_k = k;
                m_a1 = 1.0 / (1.0 + g * (g + m_k));
                m_a2 = g * m_a1;
                m_a3 = g * m_a2;
            }
            double *x[2] = {&l, &r};
            for (int c = 0; c < 2; ++c) {
                const double in = *x[c];
                const double v3 = in - m_ic2[c];
                const double v1 = m_a1 * m_ic1[c] + m_a2 * v3;
                const double v2 = m_ic2[c] + m_a2 * m_ic1[c] + m_a3 * v3;
                m_ic1[c] = 2.0 * v1 - m_ic1[c];
                m_ic2[c] = 2.0 * v2 - m_ic2[c];
                *x[c] = s.filter == FilterType::LowPass ? v2 : s.filter == FilterType::BandPass ? v1
                                                                                                : in - m_k * v1 - v2;
            }
        }
        // Drive: soft clip, folding over above half way.
        if (s.drive > 0.0) {
            const auto shape = [&](double x) {
                const double clipped = std::tanh(x * driveGain) * driveNorm;
                return fold > 0.0 ? (1.0 - fold) * clipped + fold * std::sin(x * driveGain * 0.5 * std::numbers::pi)
                                  : clipped;
            };
            l = shape(l);
            r = shape(r);
        }
        // Bitcrusher: the rate held down, then the bits.
        if (s.downsample > 1) {
            if (m_hold == 0) {
                m_held[0] = float(l);
                m_held[1] = float(r);
            }
            m_hold = (m_hold + 1) % s.downsample;
            l = m_held[0];
            r = m_held[1];
        }
        if (quant > 0.0) { // n bits: 2^n levels from -1 up to just under +1, as a converter has
            l = std::clamp(std::floor(l * quant + 0.5), -quant, quant - 1.0) / quant;
            r = std::clamp(std::floor(r * quant + 0.5), -quant, quant - 1.0) / quant;
        }
        // Ring modulator.
        if (s.ring > 0.0) {
            const double carrier = std::sin(2.0 * std::numbers::pi * m_ringPhase);
            const double mix = 1.0 - s.ring + s.ring * carrier;
            l *= mix;
            r *= mix;
        }
        m_ringPhase += ringStep;
        m_ringPhase -= std::floor(m_ringPhase);

        double amp = env * m_fade * velocityGain * s.gain;
        if (s.gate)
            amp *= double(gate[i]);
        const float outL = float(l * amp * s.panLeft * 1.4142);
        const float outR = float(r * amp * s.panRight * 1.4142);
        out.left[i] += outL;
        out.right[i] += outR;
        out.delayLeft[i] += float(outL * s.delaySend);
        out.delayRight[i] += float(outR * s.delaySend);
        out.reverbLeft[i] += float(outL * s.reverbSend);
        out.reverbRight[i] += float(outR * s.reverbSend);
    }
}

} // namespace klang
