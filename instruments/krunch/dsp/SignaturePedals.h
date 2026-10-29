#pragma once

#include "dsp/ClassicPedals.h"
#include "dsp/Filters.h"
#include "dsp/Fuzz.h"
#include "dsp/Pedal.h"
#include "dsp/Preamp.h"
#include "dsp/TimePedals.h"

namespace krunch {

// Signature pedals: one pedal for one well-known sound, its whole chain
// inside, each part brought in by one knob (docs/PEDALS.md#signature-pedals).

// Prism: the early-70s spacey, singing lead and clean sound: a silicon round
// fuzz for the solos, a photocell vibe for the slow swirls, a multi-head drum
// echo and a big reverb, in that order, then a dark-to-bright tilt and the
// level. Each of FUZZ, VIBE, ECHO and SPACE at 0 takes its part out, so the
// one pedal covers the clean parts and the leads. Best on a clean channel
// with lots of headroom (the original rig's amps were loud and clean).
class Prism final : public Pedal
{
public:
    enum Knob : int { Fuzz, VibeSpeed, Echo, Time, Space, Tone, Level };
    static const PedalInfo kInfo;

    Prism() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    void setParts() noexcept; // the parts' knobs and switches, from Prism's

    double m_rate = 48000.0;
    RoundFuzz m_fuzz;
    Vibe m_vibe;
    DrumEcho m_echo;
    Reverb m_reverb;
    Biquad m_lowShelf, m_highShelf; // the tilt
    float m_tilt = -1.0f;
    float m_level = -1.0f; // ramped
};

// Madman: the early-80s neoclassical metal lead and rhythm sound, for the
// clean channel: the rig behind it was a classic op-amp distortion pedal
// (op-amp gain, germanium diodes clipping to ground) into a 10-band EQ
// pushing the mids, into a cranked, master-less British amp. Madman is that
// chain: the distortion (GAIN), a mid push around 1 kHz with the lows trimmed
// (MIDS), the British preamp driven hard (AMP, with its BASS and TREBLE),
// then a doubler for the double-tracked width (DOUBLE) and the level.
class Madman final : public Pedal
{
public:
    enum Knob : int { Gain, Amp, Bass, Mids, Treble, Double, Level };
    static const PedalInfo kInfo;

    Madman() : Pedal(kInfo) {}

    // The distortion's op-amp gain for a GAIN setting (2 .. ~210).
    static double opAmpGain(float gain);

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    double m_rate = 48000.0;
    // The distortion pedal, at 4x.
    OnePole m_input, m_gainLeg, m_diodeLowpass;
    Oversampler4x m_oversampler;
    // The EQ: lows trimmed, mids pushed.
    Biquad m_lowCut, m_midPush;
    float m_mids = -1.0f;
    // The amp.
    Preamp m_amp;
    // The doubler: two short, slowly wandering delays.
    static constexpr std::size_t kLine = 16384; // > 40 ms at 192 kHz; a power of two
    std::array<float, kLine> m_line{};
    std::size_t m_write = 0;
    double m_wander = 0.0;
    float m_level = -1.0f; // ramped
};

} // namespace krunch
