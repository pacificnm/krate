#pragma once

#include <cstdint>

#include "dsp/BlepOscillator.h"
#include "dsp/Envelope.h"
#include "dsp/Lfo.h"
#include "krimson/dsp/Filter.h"

namespace krimson {

using krate::Envelope;
using krate::Lfo;
using krate::Oscillator;
using krate::Wave;

// One slot's sound in DSP units, worked out once per block (docs/BRIEF.md, docs/DSP.md).
struct SlotSettings
{
    Wave osc1Wave = Wave::Saw;
    Wave osc2Wave = Wave::Saw;
    double osc2Semis = 0.0;  // semitones + fine
    bool osc2Kbd = true;     // false: OSC 2 stays at middle C (+ semitones)
    double pw = 0.0;         // 0..1: width 50 % .. 5 %
    double mix = 0.5;        // 0 = OSC 1 .. 1 = OSC 2
    double fm = 0.0;         // 0..1
    bool sync = false;
    bool ring = false;

    Envelope::Times menv;    // attack / decay (sustain 0)
    double menvAmount = 0.0; // -1..1
    int menvDest = 0;        // 0 OSC 2 pitch, 1 FM, 2 pulse width

    double lfo1Hz = 2.0;
    Lfo::Wave lfo1Wave = Lfo::Wave::Triangle;
    double lfo1Amount = 0.0;
    int lfo1Dest = 0;        // 0 OSC 1+2 pitch, 1 OSC 2 pitch, 2 filter, 3 pulse width, 4 FM
    double lfo2Amount = 0.0;
    int lfo2Dest = 0;        // 0 amp, 1 filter, 2 OSC 2 pitch

    MultiFilter::Type filterType = MultiFilter::Type::Lp12;
    double cutoffOctaves = 10.0; // log2(Hz)
    double res = 0.0;
    double drive = 0.0;
    double fenvOctaves = 0.0;
    double fenvVelocity = 0.0; // 0..1: how much velocity scales the filter envelope
    double kbdTrack = 0.0;     // 0, 1/3, 2/3, 1
    Envelope::Times fenv, aenv;
    double ampVelocity = 0.5;
    double gain = 0.64;

    double bendSemis = 0.0;
    double vibratoSemis = 0.0; // depth now (wheel to Vibrato)
};

// One voice at twice the host rate (docs/DSP.md).
class Voice
{
public:
    void prepare(double sampleRate2x) noexcept;
    void noteOn(int note, float velocity, bool retrigger) noexcept;
    void noteOff() noexcept;
    void kill() noexcept;
    bool active() const noexcept { return m_aenv.active(); }
    bool gate() const noexcept { return m_gate; }
    int note() const noexcept { return m_note; }

    // Adds 2 * frames samples to out2x. lfo2 and vibrato: one value per host frame.
    void render(float *out2x, std::uint32_t frames, const float *lfo2, const float *vibrato,
                const SlotSettings &s) noexcept;

private:
    double m_rate = 96000.0;
    Oscillator m_osc1, m_osc2;
    MultiFilter m_filter;
    Envelope m_fenv, m_aenv, m_menv;
    Lfo m_lfo1;
    int m_note = 60;
    double m_velocity = 1.0;
    bool m_gate = false;
    double m_osc2Value = 0.0;
    double m_cutoff = -1.0; // smoothed, octaves
};

} // namespace krimson
