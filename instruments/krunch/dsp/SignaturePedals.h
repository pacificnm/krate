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

// A doubler: two "other takes", 17 and 26 ms late, each drifting a little
// (0.3 and 0.47 Hz), mixed in and levelled as separate performances, for a
// double-tracked width (used by the signature pedals).
class Doubler
{
public:
    void prepare(double sampleRate) noexcept { m_rate = sampleRate; reset(); }
    void reset() noexcept
    {
        m_line.fill(0.0f);
        m_write = 0;
    }
    // amount 0..1; 0 passes the signal untouched.
    void process(float *buffer, std::uint32_t frames, float amount) noexcept;

private:
    static constexpr std::size_t kLine = 16384; // > 40 ms at 192 kHz; a power of two
    std::array<float, kLine> m_line{};
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_wander = 0.0;
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
    Doubler m_doubler;
    float m_level = -1.0f; // ramped
};

// Pyramid: the mid-80s British twin-guitar heavy metal sound, for the clean
// channel: master-volume British heads at a medium crunch (not saturated),
// bright and mid-forward with tight lows, two guitars double-tracked, a touch
// of delay on the leads. Pyramid is that chain: a tightening boost (lows
// trimmed, mids pushed, a little clipping), Krunch's two-stage British
// preamp at a crunch (GAIN, with BODY, MIDS and BITE as its bass, mid and treble), the
// doubler (TWIN), a Delay (ECHO) and the level.
class Pyramid final : public Pedal
{
public:
    enum Knob : int { Gain, Body, Mids, Bite, Twin, Echo, Level };
    static const PedalInfo kInfo;

    Pyramid() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    double m_rate = 48000.0;
    Biquad m_tighten, m_push; // the boost's shaping
    Preamp m_amp;
    Doubler m_doubler;
    Delay m_delay;
    float m_level = -1.0f; // ramped
};

// Haze: the late-60s psychedelic fuzz sound, for the clean channel: a
// germanium round fuzz into a cranked British amp, and on the solos the
// octave-up fuzz (a full-wave rectifier: the wave's lower half flipped up, so
// it rings an octave higher, best high on the neck). Haze is that chain: the
// octave stage (OCTAVE, 0 = off, blended in before the fuzz), the Round Fuzz
// on germanium (FUZZ), Krunch's two-stage British preamp cranked (AMP, with
// TONE), a 60s studio room (ROOM) and the level.
class Haze final : public Pedal
{
public:
    enum Knob : int { Octave, Fuzz, Amp, Tone, Room, Level };
    static const PedalInfo kInfo;

    Haze() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    double m_rate = 48000.0;
    Oversampler4x m_oversampler; // the rectifier, at 4x
    OnePole m_octaveDc;          // the flipped wave's offset, removed (at 4x)
    RoundFuzz m_fuzz;
    Preamp m_amp;
    Reverb m_room;
    float m_level = -1.0f; // ramped
};

} // namespace krunch
