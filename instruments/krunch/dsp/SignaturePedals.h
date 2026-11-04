#pragma once

#include "dsp/ClassicPedals.h"
#include "dsp/Filters.h"
#include "dsp/Fuzz.h"
#include "dsp/Pedal.h"
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

} // namespace krunch
