#include "k909/dsp/Voices.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace k909 {

namespace {

constexpr double kLn1000 = 6.907755278982137; // 60 dB
constexpr double kSilent = 1e-5;              // -100 dB: a voice below this stops

// Exponential knob mapping: v = 0 -> lo, v = 1 -> hi.
double expMap(float v, double lo, double hi) { return lo * std::pow(hi / lo, double(v)); }

// Fixed pan per voice, -1 (left) .. 1 (right) (docs/BRIEF.md#outputs).
constexpr std::array<double, DrumCount> kPan{0.0, 0.0, -0.35, 0.0, 0.35, 0.15, -0.1, 0.25, 0.25, -0.2, -0.3};

// Output trim per voice so default-level hits peak in a similar range.
constexpr std::array<double, DrumCount> kTrim{0.9, 0.75, 0.8, 0.8, 0.8, 0.35, 0.9, 1.4, 1.4, 1.9, 1.75};

constexpr std::array<double, 6> kHatRatios{1.0, 1.48, 1.80, 2.55, 2.63, 3.90};
constexpr std::array<double, 6> kCrashRatios{1.0, 1.34, 1.73, 2.21, 2.76, 3.43};
constexpr std::array<double, 6> kRideRatios{1.0, 1.21, 1.59, 1.73, 2.12, 2.87};
constexpr double kHatBaseHz = 330.0;
constexpr double kCrashBaseHz = 300.0;
constexpr double kRideBaseHz = 480.0;

// Tom tuning ranges: low, mid, hi.
constexpr std::array<double, 3> kTomLo{70.0, 110.0, 160.0};
constexpr std::array<double, 3> kTomHi{130.0, 190.0, 280.0};

// PolyBLEP residual for a discontinuity at phase 0 (t in 0..1, dt = increment).
double polyBlep(double t, double dt) noexcept
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

} // namespace

// ---- Building blocks

void Env::start(double level, double seconds, double sampleRate) noexcept
{
    m_level = level;
    setTime(seconds, sampleRate);
}

void Env::setTime(double seconds, double sampleRate) noexcept
{
    m_factor = std::exp(-kLn1000 / (std::max(seconds, 1e-4) * sampleRate));
}

void Svf::set(double hz, double q, double sampleRate) noexcept
{
    const double g = std::tan(std::numbers::pi * std::min(hz, 0.49 * sampleRate) / sampleRate);
    m_k = 1.0 / q;
    m_a1 = 1.0 / (1.0 + g * (g + m_k));
    m_a2 = g * m_a1;
    m_a3 = g * m_a2;
}

Svf::Out Svf::process(float in) noexcept
{
    const double v3 = double(in) - m_ic2;
    const double v1 = m_a1 * m_ic1 + m_a2 * v3;
    const double v2 = m_ic2 + m_a2 * m_ic1 + m_a3 * v3;
    m_ic1 = 2.0 * v1 - m_ic1;
    m_ic2 = 2.0 * v2 - m_ic2;
    return {float(v2), float(v1), float(double(in) - m_k * v1 - v2)};
}

float Sine::next(double hz, double sampleRate) noexcept
{
    const double s = std::sin(2.0 * std::numbers::pi * m_phase);
    m_phase += hz / sampleRate;
    m_phase -= std::floor(m_phase);
    return float(s);
}

void MetalBank::set(double baseHz, const std::array<double, 6> &ratios, double sampleRate) noexcept
{
    for (std::size_t i = 0; i < 6; ++i)
        m_increment[i] = std::min(baseHz * ratios[i] / sampleRate, 0.45);
}

void MetalBank::reset() noexcept
{
    // Fixed, spread start phases: the same metallic attack on every hit.
    for (std::size_t i = 0; i < 6; ++i)
        m_phase[i] = double(i) / 6.0;
}

float MetalBank::next() noexcept
{
    double sum = 0.0;
    for (std::size_t i = 0; i < 6; ++i) {
        const double t = m_phase[i];
        const double dt = m_increment[i];
        double v = t < 0.5 ? 1.0 : -1.0;
        v += polyBlep(t, dt);
        double t2 = t + 0.5;
        t2 -= std::floor(t2);
        v -= polyBlep(t2, dt);
        sum += v;
        m_phase[i] = t + dt - std::floor(t + dt);
    }
    return float(sum / 6.0);
}

// ---- Voices

Voices::Voices() noexcept
{
    for (std::size_t i = 0; i < std::size_t(DrumCount); ++i) {
        m_panL[i] = float(std::min(1.0, 1.0 - kPan[i]));
        m_panR[i] = float(std::min(1.0, 1.0 + kPan[i]));
    }
    prepare(48000.0);
}

void Voices::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_declick = std::exp(-1.0 / (0.001 * sampleRate));
    m_active.fill(false);
    m_residual.fill(0.0);
    m_last.fill(0.0f);

    m_bdClickFilter.set(3000.0, 0.8, sampleRate);
    m_sdHp.set(800.0, 0.707, sampleRate);
    for (Tom &tom : m_toms)
        tom.noiseLp.set(3000.0, 0.707, sampleRate);
    m_rsLow.set(480.0, 12.0, sampleRate);
    m_rsHigh.set(1700.0, 10.0, sampleRate);
    m_rsHp.set(300.0, 0.707, sampleRate);
    m_cpBand.set(1200.0, 2.0, sampleRate);
    m_chBand.set(10000.0, 1.2, sampleRate);
    m_chHp.set(7000.0, 0.707, sampleRate);
    m_ohBand.set(10000.0, 1.2, sampleRate);
    m_ohHp.set(7000.0, 0.707, sampleRate);
    m_crHp.set(4000.0, 0.707, sampleRate);
    m_rdHp.set(5000.0, 0.707, sampleRate);
    m_crLp.set(13000.0, 0.707, sampleRate); // cymbals: nothing useful above the audible band
    m_rdLp.set(13000.0, 0.707, sampleRate);
    m_chBank.set(kHatBaseHz, kHatRatios, sampleRate);
    m_ohBank.set(kHatBaseHz, kHatRatios, sampleRate);
}

bool Voices::anySounding() const noexcept
{
    return std::any_of(m_active.begin(), m_active.end(), [](bool a) { return a; });
}

void Voices::trigger(DrumId drum, const Hit &hit) noexcept
{
    if (drum < DrumId::Bd || drum >= DrumId::Count)
        return;
    const auto d = std::size_t(drum);
    m_residual[d] += m_active[d] ? double(m_last[d]) : 0.0; // the old tail, fading over 1 ms
    m_active[d] = true;
    m_gain[d] = hit.gain * kTrim[d];
    const double rate = m_rate;

    switch (drum) {
    case DrumId::Bd:
        m_bdBase = 40.0 + 30.0 * hit.tune;
        m_bdAttack = hit.extra1;
        m_bdOsc.reset();
        m_bdAmp.start(1.0, expMap(hit.decay, 0.08, 0.9), rate);
        m_bdPitch.start(1.0, hit.accent ? 0.15 : 0.2, rate); // ~22 / 30 ms time constant
        m_bdClick.start(1.0, 0.02, rate);
        m_bdClickFilter.reset();
        m_bdPulse = int(0.001 * rate);
        break;
    case DrumId::Sd:
        m_sdBase = 150.0 + 110.0 * hit.tune;
        m_sdBodyMix = 1.0 - 0.5 * hit.extra1;
        m_sdNoiseMix = 0.2 + 0.8 * hit.extra2 + (hit.accent ? 0.15 : 0.0);
        m_sdOsc1.reset();
        m_sdOsc2.reset();
        m_sdBody.start(1.0, 0.18, rate);
        m_sdPitch.start(1.0, 0.07, rate);
        m_sdNoiseEnv.start(1.0, 0.12 + 0.18 * hit.extra2, rate);
        m_sdLp.set(expMap(hit.extra1, 3000.0, 12000.0), 0.707, rate);
        m_sdHp.reset();
        m_sdLp.reset();
        break;
    case DrumId::Lt:
    case DrumId::Mt:
    case DrumId::Ht: {
        const std::size_t t = d - std::size_t(DrumId::Lt);
        Tom &tom = m_toms[t];
        tom.base = expMap(hit.tune, kTomLo[t], kTomHi[t]);
        tom.osc.reset();
        tom.amp.start(1.0, expMap(hit.decay, 0.1, 0.9), rate);
        tom.pitch.start(1.0, 0.5, rate);
        tom.noiseEnv.start(1.0, 0.035, rate);
        tom.noiseLp.reset();
        break;
    }
    case DrumId::Rs:
        m_rsLow.reset();
        m_rsHigh.reset();
        m_rsHp.reset();
        m_rsAmp.start(1.0, 0.09, rate);
        m_rsImpulse = 1;
        break;
    case DrumId::Cp:
        m_cpBand.reset();
        m_cpBurst.start(1.0, 0.02, rate);
        m_cpTail.start(0.0, 0.2, rate);
        m_cpTime = 0;
        break;
    case DrumId::Ch:
        m_chBank.reset();
        m_chBand.reset();
        m_chHp.reset();
        m_chAmp.start(1.0, expMap(hit.decay, 0.02, 0.15), rate);
        // A closed hat chokes a ringing open hat (5 ms fade).
        if (m_active[std::size_t(DrumId::Oh)])
            m_ohAmp.setTime(0.005, rate);
        break;
    case DrumId::Oh:
        m_ohBank.reset();
        m_ohBand.reset();
        m_ohHp.reset();
        m_ohAmp.start(1.0, expMap(hit.decay, 0.1, 1.0), rate);
        break;
    case DrumId::Cr:
        m_crBank.set(kCrashBaseHz * expMap(hit.tune, 0.75, 1.33), kCrashRatios, rate);
        m_crBank.reset();
        m_crHp.reset();
        m_crLp.reset();
        m_crAmp.start(1.0, 1.6, rate);
        break;
    case DrumId::Rd: {
        const double base = kRideBaseHz * expMap(hit.tune, 0.75, 1.33);
        m_rdBank.set(base, kRideRatios, rate);
        m_rdBank.reset();
        m_rdHp.reset();
        m_rdLp.reset();
        m_rdBell.reset();
        m_rdBellHz = base * kRideRatios[3];
        m_rdAmp.start(1.0, 1.5, rate);
        m_rdBellAmp.start(1.0, 0.9, rate);
        break;
    }
    case DrumId::Count:
        break;
    }
}

float Voices::tick(DrumId drum) noexcept
{
    const double rate = m_rate;
    switch (drum) {
    case DrumId::Bd: {
        const float pitch = m_bdPitch.next();
        const double hz = m_bdBase * (1.0 + 1.5 * double(pitch));
        const double body = std::tanh(1.5 * double(m_bdOsc.next(hz, rate))) / std::tanh(1.5);
        const float click = m_bdClickFilter.process(m_bdNoise.next()).bp * m_bdClick.next();
        double pulse = 0.0;
        if (m_bdPulse > 0) {
            --m_bdPulse;
            pulse = 0.5;
        }
        const double amp = m_bdAmp.next();
        if (amp < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(amp * body + m_bdAttack * (1.5 * click + pulse) * (amp > 0.5 ? 1.0 : amp * 2.0));
    }
    case DrumId::Sd: {
        const double dip = 1.0 + 0.3 * double(m_sdPitch.next());
        const double body = double(m_sdOsc1.next(m_sdBase * dip, rate)) + 0.6 * m_sdOsc2.next(m_sdBase * 1.65 * dip, rate);
        const float noise = m_sdLp.process(m_sdHp.process(m_sdNoise.next()).hp).lp;
        const double bodyEnv = m_sdBody.next();
        const double noiseEnv = m_sdNoiseEnv.next();
        if (bodyEnv < kSilent && noiseEnv < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(0.6 * m_sdBodyMix * bodyEnv * body + 1.2 * m_sdNoiseMix * noiseEnv * noise);
    }
    case DrumId::Lt:
    case DrumId::Mt:
    case DrumId::Ht: {
        Tom &tom = m_toms[std::size_t(drum) - std::size_t(DrumId::Lt)];
        const double hz = tom.base * (1.0 + 0.15 * double(tom.pitch.next()));
        const double body = tom.osc.next(hz, rate);
        const float noise = tom.noiseLp.process(tom.noise.next()).lp * tom.noiseEnv.next();
        const double amp = tom.amp.next();
        if (amp < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(amp * body + 0.15 * noise);
    }
    case DrumId::Rs: {
        const float in = m_rsImpulse > 0 ? 1.0f : 0.0f;
        m_rsImpulse = 0;
        const float ring = m_rsLow.process(in).bp + 1.4f * m_rsHigh.process(in).bp;
        const double amp = m_rsAmp.next();
        if (amp < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(8.0 * amp * m_rsHp.process(ring).hp);
    }
    case DrumId::Cp: {
        // Three 10 ms bursts 11 ms apart, then the tail.
        const int burstLength = int(0.011 * rate);
        if (m_cpTime == burstLength || m_cpTime == 2 * burstLength)
            m_cpBurst.start(1.0, 0.02, rate);
        else if (m_cpTime == 3 * burstLength)
            m_cpTail.start(0.7, 0.2, rate);
        ++m_cpTime;
        const double env = m_cpTime <= 3 * burstLength ? m_cpBurst.next() : m_cpTail.next();
        if (m_cpTime > 3 * burstLength && env < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(2.5 * env * m_cpBand.process(m_cpNoise.next()).bp);
    }
    case DrumId::Ch:
    case DrumId::Oh: {
        const bool closed = drum == DrumId::Ch;
        MetalBank &bank = closed ? m_chBank : m_ohBank;
        Svf &band = closed ? m_chBand : m_ohBand;
        Svf &hp = closed ? m_chHp : m_ohHp;
        Env &amp = closed ? m_chAmp : m_ohAmp;
        const float metal = bank.next() + 0.25f * m_hatNoise.next();
        const double a = amp.next();
        if (a < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(2.0 * a * hp.process(band.process(metal).bp).hp);
    }
    case DrumId::Cr: {
        const float metal = m_crBank.next() + 0.3f * m_cyNoise.next();
        const double a = m_crAmp.next();
        if (a < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(a * m_crLp.process(m_crHp.process(metal).hp).lp);
    }
    case DrumId::Rd: {
        const float metal = m_rdBank.next() + 0.1f * m_cyNoise.next();
        const double bell = m_rdBell.next(m_rdBellHz, rate) * m_rdBellAmp.next();
        const double a = m_rdAmp.next();
        if (a < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(a * m_rdLp.process(m_rdHp.process(metal).hp).lp + 0.25 * bell);
    }
    case DrumId::Count:
        break;
    }
    return 0.0f;
}

void Voices::render(float *left, float *right, std::uint32_t frames) noexcept
{
    for (int v = 0; v < DrumCount; ++v) {
        const auto d = std::size_t(v);
        if (!m_active[d] && std::fabs(m_residual[d]) < kSilent)
            continue;
        const double gain = m_gain[d];
        const float panL = m_panL[d];
        const float panR = m_panR[d];
        for (std::uint32_t i = 0; i < frames; ++i) {
            double s = m_residual[d];
            m_residual[d] *= m_declick;
            if (m_active[d]) {
                const double voice = gain * double(tick(DrumId(v)));
                m_last[d] = float(voice);
                s += voice;
            }
            left[i] += float(s) * panL;
            right[i] += float(s) * panR;
        }
        if (!m_active[d])
            m_last[d] = 0.0f;
    }
}

} // namespace k909
