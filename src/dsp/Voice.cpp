#include "dsp/Voice.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "dsp/Params.h"

namespace k303 {

namespace {

// Starting values (docs/DSP.md): tune by ear and keep the doc in step.
constexpr double kParamSmoothingSeconds = 0.010;
constexpr double kSlideSeconds = 0.060;          // glide time constant
constexpr double kVegAttackSeconds = 0.003;
constexpr double kVegDecayTau = 1.0;             // ~3 s to near silence while held
constexpr double kVegReleaseTau = 0.003;         // ~9 ms to near silence
constexpr double kMegTauPerDecay = 1.0 / 3.0;    // MEG at ~5% after the Decay knob's time
constexpr double kAccentGainMax = 2.0;           // +6 dB at full Accent
constexpr double kAccentGainSeconds = 0.002;
constexpr double kSweepChargeTau = 0.030;
constexpr double kSweepDischargeTau = 0.150;
constexpr double kSweepOctaves = 2.0;            // at full accent, scaled up by resonance
constexpr double kMaxFeedback = 15.0;            // below DiodeLadder::kSelfOscillation
constexpr double kDrive = 1.5;                   // tanh saturation into the ladder
constexpr double kDcBlockHz = 10.0;
constexpr double kOutputGain = 1.0;
// The ladder's resonant peak sits at ~0.67 x its coefficient frequency; scale
// so the Cutoff knob names the frequency where the resonance sings.
constexpr double kLadderTuning = 1.5;
constexpr double kMinCutoffHz = 20.0;
constexpr double kMaxCutoffHz = 20000.0;

double onePoleCoeff(double seconds, double sampleRate)
{
    return 1.0 - std::exp(-1.0 / (seconds * sampleRate));
}

double decayMultiplier(double tau, double sampleRate)
{
    return std::exp(-1.0 / (tau * sampleRate));
}

} // namespace

Voice::Voice()
{
    m_cutoffLog2 = std::log2(m_target.cutoffHz);
    m_resonance = m_target.resonance;
    m_envMod = m_target.envModOctaves;
    m_accentAmount = m_target.accent;
    m_tuning = m_target.tuningSemitones;
    setSampleRate(m_sampleRate);
}

void Voice::setSampleRate(double sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    m_osc.setSampleRate(sampleRate * kOversample);

    m_paramCoeff = onePoleCoeff(kParamSmoothingSeconds, sampleRate);
    m_slideCoeff = onePoleCoeff(kSlideSeconds, sampleRate);
    m_vegAttackStep = 1.0 / (kVegAttackSeconds * sampleRate);
    m_vegDecayMul = decayMultiplier(kVegDecayTau, sampleRate);
    m_vegReleaseMul = decayMultiplier(kVegReleaseTau, sampleRate);
    m_sweepCharge = onePoleCoeff(kSweepChargeTau, sampleRate);
    m_sweepDischarge = onePoleCoeff(kSweepDischargeTau, sampleRate);
    m_accentGainCoeff = onePoleCoeff(kAccentGainSeconds, sampleRate);

    const double dt = 1.0 / (sampleRate * kOversample);
    const double rc = 1.0 / (2.0 * std::numbers::pi * kDcBlockHz);
    m_hpfCoeff = rc / (rc + dt);

    updateEnvelopeRates();
}

void Voice::setParams(const VoiceParams &params) noexcept
{
    const bool decayChanged = params.decaySeconds != m_target.decaySeconds;
    m_target = params;
    m_osc.setShape(params.square ? Oscillator::Shape::Square : Oscillator::Shape::Saw);
    if (decayChanged)
        updateEnvelopeRates();
}

void Voice::updateEnvelopeRates() noexcept
{
    m_megMul = decayMultiplier(m_target.decaySeconds * kMegTauPerDecay, m_sampleRate);
    // Accented notes ignore the Decay knob and use the shortest decay.
    m_megMulAccent = decayMultiplier(units::kDecayMinS * kMegTauPerDecay, m_sampleRate);
}

void Voice::noteOn(int midiNote, bool accent) noexcept
{
    m_pitch = m_pitchTarget = midiNote;
    m_sliding = false;
    m_noteAccent = accent;
    m_meg = 1.0;
    m_vegStage = VegStage::Attack; // from the current level: no click on retrigger
}

void Voice::slideTo(int midiNote, bool accent) noexcept
{
    m_pitchTarget = midiNote;
    m_sliding = true;
    m_noteAccent = accent;
}

void Voice::gateOff() noexcept
{
    m_vegStage = VegStage::Release;
}

float Voice::next() noexcept
{
    // --- Control rate (base sample rate) ---------------------------------------
    m_cutoffLog2 += (std::log2(m_target.cutoffHz) - m_cutoffLog2) * m_paramCoeff;
    m_resonance += (m_target.resonance - m_resonance) * m_paramCoeff;
    m_envMod += (m_target.envModOctaves - m_envMod) * m_paramCoeff;
    m_accentAmount += (m_target.accent - m_accentAmount) * m_paramCoeff;
    m_tuning += (m_target.tuningSemitones - m_tuning) * m_paramCoeff;

    if (m_sliding)
        m_pitch += (m_pitchTarget - m_pitch) * m_slideCoeff;
    m_osc.setFrequency(440.0 * std::exp2((m_pitch + m_tuning - 69.0) / 12.0));

    m_meg *= m_noteAccent ? m_megMulAccent : m_megMul;

    switch (m_vegStage) {
    case VegStage::Attack:
        m_veg += m_vegAttackStep;
        if (m_veg >= 1.0) {
            m_veg = 1.0;
            m_vegStage = VegStage::Decay;
        }
        break;
    case VegStage::Decay:
        m_veg *= m_vegDecayMul;
        break;
    case VegStage::Release:
        m_veg *= m_vegReleaseMul;
        break;
    }

    // Accent sweep: a capacitor charged by the accented filter envelope. It
    // discharges slowly, so consecutive accents stack and push the filter higher.
    const double sweepTarget = m_noteAccent ? m_meg * m_accentAmount : 0.0;
    m_sweep += (sweepTarget - m_sweep) * (sweepTarget > m_sweep ? m_sweepCharge : m_sweepDischarge);

    const double gainTarget = m_noteAccent ? 1.0 + m_accentAmount * (kAccentGainMax - 1.0) : 1.0;
    m_accentGain += (gainTarget - m_accentGain) * m_accentGainCoeff;

    const double octaves = m_cutoffLog2 + m_envMod * m_meg + m_sweep * kSweepOctaves * (0.5 + m_resonance);
    const double maxCutoff = std::min(kMaxCutoffHz, 0.45 * m_sampleRate);
    const double cutoff = std::clamp(kLadderTuning * std::exp2(octaves), kMinCutoffHz, maxCutoff);
    const double k = kMaxFeedback * m_resonance;
    m_ladder.setCoefficients(cutoff, m_sampleRate * kOversample, k);
    // Passband gain falls as 1 / (1 + k); restore half of that (in dB) so high
    // resonance still thins the bass, as intended, without collapsing the level.
    const double makeup = std::sqrt(1.0 + k);

    // --- Audio rate (oversampled) -----------------------------------------------
    float os[kOversample];
    for (int i = 0; i < kOversample; ++i) {
        const double x = m_osc.next();
        m_hpfOut = m_hpfCoeff * (m_hpfOut + x - m_hpfIn);
        m_hpfIn = x;
        os[i] = static_cast<float>(m_ladder.process(std::tanh(kDrive * m_hpfOut)));
    }
    const float y = m_decimate2to1.process(m_decimate4to2.process(os[0], os[1]),
                                           m_decimate4to2.process(os[2], os[3]));

    return static_cast<float>(y * makeup * m_veg * m_accentGain * kOutputGain);
}

} // namespace k303
