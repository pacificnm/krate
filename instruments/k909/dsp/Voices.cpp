#include "k909/dsp/Voices.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "dsp/PolyBlep.h"
#include "k909/dsp/Params.h"

namespace k909 {

namespace {

constexpr double kLn1000 = 6.907755278982137; // 60 dB
constexpr double kSilent = 1e-5;              // -100 dB: a voice below this stops

// Exponential knob mapping: v = 0 -> lo, v = 1 -> hi.
double expMap(float v, double lo, double hi) { return lo * std::pow(hi / lo, double(v)); }
// A scale around a centre: v = 0.5 -> exactly 1, v = 0 -> 1/range, v = 1 -> range.
double around(float v, double range) { return std::pow(range, 2.0 * double(v) - 1.0); }

constexpr double kMaxDrive = 24.0; // Drive 100 %: +28 dB into the tanh (peak kept)
constexpr double kLn2 = 0.6931471805599453;
// The antiderivative of tanh: ln cosh(u), without overflow.
double logCosh(double u)
{
    const double a = std::fabs(u);
    return a + std::log1p(std::exp(-2.0 * a)) - kLn2;
}

// Output trim per voice so default-level hits peak in a similar range.
constexpr std::array<double, DrumCount> kTrim{0.9, 0.75, 0.8, 0.8, 0.8, 0.35, 0.9, 1.4, 1.4, 1.9, 1.75,
                                              1.0, 0.85, 1.0, 1.15, 1.4};

// Metal's modes: a free bar or pipe (stretched, far apart) to a plate or sheet
// (dense, close together); Material morphs between them (log-frequency).
constexpr std::array<double, 6> kPipeRatios{1.0, 2.756, 5.404, 8.933, 13.344, 18.64};
constexpr std::array<double, 6> kPlateRatios{1.0, 1.36, 1.67, 2.10, 2.42, 2.98};
constexpr std::array<double, 6> kModeAmp{1.0, 0.7, 0.55, 0.4, 0.3, 0.22};
constexpr std::array<double, 6> kModeDecay{1.0, 0.8, 0.6, 0.45, 0.35, 0.25}; // of the Decay time
constexpr double kClankRatio1 = 1.4142;
constexpr double kClankRatio2 = 3.7321;

constexpr std::array<double, 6> kHatRatios{1.0, 1.48, 1.80, 2.55, 2.63, 3.90};
constexpr std::array<double, 6> kCrashRatios{1.0, 1.34, 1.73, 2.21, 2.76, 3.43};
constexpr std::array<double, 6> kRideRatios{1.0, 1.21, 1.59, 1.73, 2.12, 2.87};
constexpr double kHatBaseHz = 330.0;
constexpr double kCrashBaseHz = 300.0;
constexpr double kRideBaseHz = 480.0;

// Tom tuning ranges: low, mid, hi.
constexpr std::array<double, 3> kTomLo{70.0, 110.0, 160.0};
constexpr std::array<double, 3> kTomHi{130.0, 190.0, 280.0};

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
        v += krate::polyBlep(t, dt);
        double t2 = t + 0.5;
        t2 -= std::floor(t2);
        v -= krate::polyBlep(t2, dt);
        sum += v;
        m_phase[i] = t + dt - std::floor(t + dt);
    }
    return float(sum / 6.0);
}

// ---- Voices

Voices::Voices() noexcept
{
    for (int d = 0; d < DrumCount; ++d)
        setMix(DrumId(d), Mix{});
    prepare(48000.0);
}

void Voices::setSample(const Sample *sample) noexcept
{
    if (sample == m_sample)
        return;
    const auto d = std::size_t(DrumId::Sm);
    if (m_active[d]) { // stop the old one now; its last sample fades out as the residual
        m_residual[d] += double(m_last[d]);
        m_active[d] = false;
        m_last[d] = 0.0f;
    }
    m_sample = sample;
}

void Voices::setMix(DrumId drum, const Mix &mix) noexcept
{
    const auto d = std::size_t(drum);
    m_panL[d] = float(std::min(1.0, 1.0 - mix.pan));
    m_panR[d] = float(std::min(1.0, 1.0 + mix.pan));
    m_reverbSend[d] = mix.reverb;
    m_delaySend[d] = mix.delay;
    const double gain = kMaxDrive * mix.drive * mix.drive;
    if (gain != m_driveGain[d]) {
        m_driveGain[d] = gain;
        m_driveNorm[d] = gain > 0.0 ? 1.0 / std::tanh(gain) : 1.0;
    }
}

double Voices::drive(std::size_t d, double x) noexcept
{
    // First-order antiderivative anti-aliasing: the average of tanh between
    // this sample and the last, (F(u) - F(u')) / (u - u'), F = ln cosh. Much
    // less aliasing than tanh(u) itself, no oversampling, no latency.
    const double u = m_driveGain[d] * x;
    const double integral = logCosh(u);
    const double du = u - m_drivePrev[d];
    const double y = std::fabs(du) > 1e-6 ? (integral - m_driveIntegral[d]) / du : std::tanh(0.5 * (u + m_drivePrev[d]));
    m_drivePrev[d] = u;
    m_driveIntegral[d] = integral;
    return y * m_driveNorm[d];
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
    m_sdCrackHp.set(4500.0, 0.707, sampleRate);
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
    case DrumId::Bd: {
        m_bdBase = 40.0 + 30.0 * hit.tune;
        m_bdAttack = hit.extra[0];
        m_bdSweep = 3.0 * hit.extra[1];               // starts (1 + sweep) x the pitch: 1 .. 4x
        m_bdShape = 1.5 * around(hit.extra[3], 4.0);  // the body's tanh: round sine .. near square
        m_bdShapeNorm = std::tanh(m_bdShape);
        m_bdOsc.reset();
        m_bdAmp.start(1.0, expMap(hit.decay, 0.08, 0.9), rate);
        // Bend time ~22 / 30 ms time constant at the default, 1/4 .. 4x.
        const double bend = around(hit.extra[2], 4.0);
        m_bdPitch.start(1.0, hit.accent ? 0.15 * bend : 0.2 * bend, rate);
        m_bdClick.start(1.0, 0.02, rate);
        m_bdClickFilter.reset();
        m_bdPulse = int(0.001 * rate);
        break;
    }
    case DrumId::Sd:
        m_sdBase = 150.0 + 110.0 * hit.tune;
        m_sdBodyMix = 1.0 - 0.5 * hit.extra[0];
        m_sdNoiseMix = 0.2 + 0.8 * hit.extra[1] + (hit.accent ? 0.15 : 0.0);
        m_sdCrack = hit.extra[2];
        m_sdOsc1.reset();
        m_sdOsc2.reset();
        m_sdBody.start(1.0, 0.18 * around(hit.decay, 4.0), rate);
        m_sdPitch.start(1.0, 0.07, rate);
        m_sdNoiseEnv.start(1.0, 0.12 + 0.18 * hit.extra[1], rate);
        m_sdCrackEnv.start(1.0, 0.025, rate);
        m_sdLp.set(expMap(hit.extra[0], 3000.0, 12000.0), 0.707, rate);
        m_sdHp.reset();
        m_sdLp.reset();
        m_sdCrackHp.reset();
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
    case DrumId::Rs: {
        const double tune = around(hit.tune, 2.0); // an octave either way
        m_rsLow.set(480.0 * tune, 12.0, rate);
        m_rsHigh.set(1700.0 * tune, 10.0, rate);
        m_rsLow.reset();
        m_rsHigh.reset();
        m_rsHp.reset();
        m_rsAmp.start(1.0, 0.09 * around(hit.decay, 4.0), rate);
        m_rsImpulse = 1;
        break;
    }
    case DrumId::Cp:
        m_cpBand.set(1200.0 * around(hit.tune, 2.0), 2.0, rate);
        m_cpBand.reset();
        m_cpBurstLength = int(0.011 * around(hit.extra[0], 2.5) * rate); // 4.4 .. 27.5 ms apart
        m_cpTailSeconds = 0.2 * around(hit.decay, 4.0);
        m_cpBurst.start(1.0, 0.02, rate);
        m_cpTail.start(0.0, m_cpTailSeconds, rate);
        m_cpTime = 0;
        break;
    case DrumId::Ch: {
        const double tune = around(hit.tune, 1.5);
        m_chBank.set(kHatBaseHz * tune, kHatRatios, rate);
        m_chBand.set(10000.0 * tune, 1.2, rate); // the filters follow, so the metal moves as a whole
        m_chHp.set(7000.0 * tune, 0.707, rate);
        m_chBank.reset();
        m_chBand.reset();
        m_chHp.reset();
        m_chAmp.start(1.0, expMap(hit.decay, 0.02, 0.15), rate);
        // A closed hat chokes a ringing open hat (5 ms fade).
        if (m_active[std::size_t(DrumId::Oh)])
            m_ohAmp.setTime(0.005, rate);
        break;
    }
    case DrumId::Oh: {
        const double tune = around(hit.tune, 1.5);
        m_ohBank.set(kHatBaseHz * tune, kHatRatios, rate);
        m_ohBand.set(10000.0 * tune, 1.2, rate);
        m_ohHp.set(7000.0 * tune, 0.707, rate);
        m_ohBank.reset();
        m_ohBand.reset();
        m_ohHp.reset();
        m_ohAmp.start(1.0, expMap(hit.decay, 0.1, 1.0), rate);
        break;
    }
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
    case DrumId::Me: {
        const double f0 = expMap(hit.tune, 120.0, 1500.0);
        const double decay = expMap(hit.decay, 0.15, 3.0);
        for (std::size_t i = 0; i < std::size_t(kModes); ++i) {
            const double ratio = std::exp(std::log(kPipeRatios[i]) * (1.0 - hit.extra[0])
                                          + std::log(kPlateRatios[i]) * hit.extra[0]);
            m_meHz[i] = f0 * ratio;
            m_meAmp[i] = m_meHz[i] < 0.45 * rate ? kModeAmp[i] : 0.0; // nothing over Nyquist
            m_meModes[i].reset();
            m_meEnv[i].start(1.0, decay * kModeDecay[i], rate);
        }
        m_meStrike.start(1.0, 0.012, rate);
        m_meStrikeBand.set(std::min(3.0 * f0, 0.4 * rate), 1.5, rate);
        m_meStrikeBand.reset();
        break;
    }
    case DrumId::Ck:
        m_ckHz = expMap(hit.tune, 80.0, 900.0);
        m_ckIndex = 12.0 * hit.extra[0];
        m_ckCarrier = m_ckMod1 = m_ckMod2 = 0.0;
        m_ckAmp.start(1.0, expMap(hit.decay, 0.05, 1.2), rate);
        m_ckIndexEnv.start(1.0, std::max(0.015, 0.3 * expMap(hit.decay, 0.05, 1.2)), rate);
        m_ckClick.start(1.0, 0.006, rate);
        break;
    case DrumId::Nz: {
        m_nzHz = expMap(hit.tune, 300.0, 8000.0);
        m_nzSweepOctaves = 4.0 * hit.extra[0];
        const double decay = expMap(hit.decay, 0.03, 1.5);
        m_nzAmp.start(1.0, decay, rate);
        m_nzSweepEnv.start(1.0, 0.5 * decay, rate);
        m_nzFilter.reset();
        break;
    }
    case DrumId::Sm: {
        if (!m_sample || m_sample->data.size() < 4) {
            m_active[d] = false;
            break;
        }
        const double length = double(m_sample->data.size());
        m_smPosition = std::floor(0.9 * double(hit.extra[0]) * length);
        m_smStep = m_sample->rate / rate * std::exp2(units::sampleSemitones(hit.tune) / 12.0);
        const double decay = units::sampleDecaySeconds(hit.decay);
        m_smFade = decay > 0.0;
        m_smAmp.start(1.0, m_smFade ? decay : 1.0, rate);
        m_smFadeInLength = std::max(1, int(0.002 * rate));
        m_smFadeIn = m_smPosition > 0.0 ? m_smFadeInLength : 0; // a start mid-sample would click
        break;
    }
    case DrumId::Bm:
        m_bmBase = expMap(hit.tune, 28.0, 65.0);
        m_bmOsc.reset();
        m_bmAmp.start(1.0, expMap(hit.decay, 0.3, 3.0), rate);
        m_bmPitch.start(1.0, 0.25, rate);
        break;
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
        const double hz = m_bdBase * (1.0 + m_bdSweep * double(pitch));
        const double body = std::tanh(m_bdShape * double(m_bdOsc.next(hz, rate))) / m_bdShapeNorm;
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
        double out = 0.6 * m_sdBodyMix * bodyEnv * body + 1.2 * m_sdNoiseMix * noiseEnv * noise;
        if (m_sdCrack > 0.0)
            out += 1.6 * m_sdCrack * m_sdCrackEnv.next() * m_sdCrackHp.process(m_sdCrackNoise.next()).hp;
        return float(out);
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
        // Three bursts (11 ms apart at the default Spread), then the tail.
        const int burstLength = m_cpBurstLength;
        if (m_cpTime == burstLength || m_cpTime == 2 * burstLength)
            m_cpBurst.start(1.0, 0.02, rate);
        else if (m_cpTime == 3 * burstLength)
            m_cpTail.start(0.7, m_cpTailSeconds, rate);
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
    case DrumId::Me: {
        double sum = 0.0, loudest = 0.0;
        for (std::size_t i = 0; i < std::size_t(kModes); ++i) {
            const double e = m_meEnv[i].next();
            loudest = std::max(loudest, e * m_meAmp[i]);
            if (m_meAmp[i] > 0.0)
                sum += m_meAmp[i] * e * m_meModes[i].next(m_meHz[i], rate);
        }
        const double strike = m_meStrike.next() * m_meStrikeBand.process(m_meNoise.next()).bp;
        if (loudest < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(0.4 * sum + 1.5 * strike);
    }
    case DrumId::Ck: {
        const double index = m_ckIndex * m_ckIndexEnv.next();
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double mod = std::sin(kTwoPi * m_ckMod1) + 0.5 * std::sin(kTwoPi * m_ckMod2);
        const double out = std::sin(kTwoPi * m_ckCarrier + index * mod);
        const double step = m_ckHz / rate;
        m_ckCarrier += step;
        m_ckMod1 += step * kClankRatio1;
        m_ckMod2 += step * kClankRatio2;
        m_ckCarrier -= std::floor(m_ckCarrier);
        m_ckMod1 -= std::floor(m_ckMod1);
        m_ckMod2 -= std::floor(m_ckMod2);
        const double a = m_ckAmp.next();
        if (a < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(a * out + 0.4 * m_ckClick.next() * m_ckNoise.next());
    }
    case DrumId::Nz: {
        const double hz = m_nzHz * std::exp2(m_nzSweepOctaves * m_nzSweepEnv.next());
        m_nzFilter.set(std::min(hz, 0.45 * rate), 3.0, rate);
        const Svf::Out f = m_nzFilter.process(m_nzNoise.next());
        const double a = m_nzAmp.next();
        if (a < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(a * std::tanh(1.5 * (f.lp + 0.6 * f.bp)) / std::tanh(1.5));
    }
    case DrumId::Sm: {
        const std::vector<float> &data = m_sample->data;
        const double length = double(data.size());
        if (m_smPosition >= length - 2.0) {
            m_active[std::size_t(drum)] = false;
            return 0.0f;
        }
        // 4-point (hermite) read between the source's samples.
        const auto i = std::size_t(m_smPosition);
        const double t = m_smPosition - double(i);
        const double xm1 = data[i > 0 ? i - 1 : 0], x0 = data[i], x1 = data[i + 1], x2 = data[i + 2];
        const double c = 0.5 * (x1 - xm1), v = x0 - x1, w = c + v, a = w + v + 0.5 * (x2 - x0), b = w + a;
        double out = ((a * t - b) * t + c) * t + x0;
        // The last 5 ms (of output) fade, so a cut-off file ends without a click.
        const double fadeOut = 0.005 * rate * m_smStep;
        if (length - 2.0 - m_smPosition < fadeOut)
            out *= (length - 2.0 - m_smPosition) / fadeOut;
        if (m_smFadeIn > 0) {
            out *= 1.0 - double(m_smFadeIn) / double(m_smFadeInLength);
            --m_smFadeIn;
        }
        m_smPosition += m_smStep;
        if (m_smFade) {
            const double env = m_smAmp.next();
            out *= env;
            if (env < kSilent)
                m_active[std::size_t(drum)] = false;
        }
        return float(out);
    }
    case DrumId::Bm: {
        const double hz = m_bmBase * (1.0 + 2.0 * double(m_bmPitch.next()));
        const double body = std::tanh(2.5 * double(m_bmOsc.next(hz, rate))) / std::tanh(2.5);
        const double a = m_bmAmp.next();
        if (a < kSilent)
            m_active[std::size_t(drum)] = false;
        return float(a * body);
    }
    case DrumId::Count:
        break;
    }
    return 0.0f;
}

void Voices::render(float *left, float *right, const Sends &sends, std::uint32_t frames) noexcept
{
    for (int v = 0; v < DrumCount; ++v) {
        const auto d = std::size_t(v);
        if (!m_active[d] && std::fabs(m_residual[d]) < kSilent)
            continue;
        const double gain = m_gain[d];
        const float panL = m_panL[d];
        const float panR = m_panR[d];
        const bool driven = m_driveGain[d] > 0.0;
        const auto reverb = float(m_reverbSend[d]), delay = float(m_delaySend[d]);
        for (std::uint32_t i = 0; i < frames; ++i) {
            double s = m_residual[d];
            m_residual[d] *= m_declick;
            if (m_active[d]) {
                double raw = double(tick(DrumId(v)));
                if (driven)
                    raw = drive(d, raw);
                const double voice = gain * raw;
                m_last[d] = float(voice);
                s += voice;
            }
            const float l = float(s) * panL, r = float(s) * panR;
            left[i] += l;
            right[i] += r;
            if (reverb > 0.0f) {
                sends.reverbL[i] += reverb * l;
                sends.reverbR[i] += reverb * r;
            }
            if (delay > 0.0f) {
                sends.delayL[i] += delay * l;
                sends.delayR[i] += delay * r;
            }
        }
        if (!m_active[d])
            m_last[d] = 0.0f;
    }
}

} // namespace k909
