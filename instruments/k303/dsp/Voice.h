#pragma once

#include "dsp/DiodeLadder.h"
#include "dsp/HalfbandDecimator.h"
#include "dsp/Oscillator.h"

namespace k303 {

// Knob targets in engine units, set once per audio block (Engine maps them
// from the normalized ParamStore). The voice smooths them per sample.
struct VoiceParams
{
    double cutoffHz = 500.0;
    double resonance = 0.5;       // 0..1
    double envModOctaves = 2.0;
    double decaySeconds = 0.6;
    double accent = 0.5;          // 0..1
    double sweepOctaves = 2.0;    // accent sweep depth at full Accent (before the resonance scaling)
    bool square = false;
    double tuningSemitones = 0.0;
};

// One monophonic voice: oscillator -> DC blocker -> diode ladder (4x
// oversampled) -> VCA, with filter envelope (MEG), amp envelope (VEG), accent
// and slide. See docs/DSP.md. Realtime-safe: no allocation after construction.
class Voice
{
public:
    static constexpr int kOversample = 4;

    Voice();

    void setSampleRate(double sampleRate) noexcept;
    void setParams(const VoiceParams &params) noexcept;

    // New note: pitch jumps, both envelopes retrigger.
    void noteOn(int midiNote, bool accent) noexcept;
    // Legato: pitch glides, envelopes continue.
    void slideTo(int midiNote, bool accent) noexcept;
    void gateOff() noexcept;

    float next() noexcept;

private:
    enum class VegStage { Attack, Decay, Release };

    void updateEnvelopeRates() noexcept;

    Oscillator m_osc;
    DiodeLadder m_ladder;
    krate::HalfbandDecimator<31> m_decimate4to2;
    krate::HalfbandDecimator<31> m_decimate2to1;

    double m_sampleRate = 48000.0;
    VoiceParams m_target;

    // Smoothed knob values
    double m_cutoffLog2 = 0.0;
    double m_resonance = 0.0;
    double m_envMod = 0.0;
    double m_accentAmount = 0.0;
    double m_tuning = 0.0;
    double m_paramCoeff = 0.0;

    // Pitch
    double m_pitch = 48.0;        // MIDI note, fractional while sliding
    double m_pitchTarget = 48.0;
    bool m_sliding = false;
    double m_slideCoeff = 0.0;

    // Note state
    bool m_noteAccent = false;

    // Envelopes
    double m_meg = 0.0;
    double m_megMul = 0.0;
    double m_megMulAccent = 0.0;
    double m_veg = 0.0;
    VegStage m_vegStage = VegStage::Release;
    double m_vegAttackStep = 0.0;
    double m_vegDecayMul = 0.0;
    double m_vegReleaseMul = 0.0;

    // Accent
    double m_sweep = 0.0;
    double m_sweepCharge = 0.0;
    double m_sweepDischarge = 0.0;
    double m_accentGain = 1.0;
    double m_accentGainCoeff = 0.0;

    // DC blocker (at the oversampled rate)
    double m_hpfCoeff = 0.0;
    double m_hpfIn = 0.0;
    double m_hpfOut = 0.0;
};

} // namespace k303
