#pragma once

#include <array>
#include <vector>

#include "dsp/Filters.h"
#include "dsp/Pedal.h"

namespace krunch {

// Vibe: the classic photocell vibe, in broad strokes (docs/PEDALS.md#vibe):
// four all-pass stages whose capacitors are far apart (so their notches are
// spread unevenly, not in a neat comb), swept together by a lamp shining on
// photocells: the lamp follows the LFO, the cells answer quickly as it
// brightens and slowly as it dims, which gives the vibe its lopsided, throbbing
// sweep. CHORUS mixes it half and half with the guitar; VIBRATO is the swept
// signal alone (a pitch wobble).
class Vibe final : public Pedal
{
public:
    enum Knob : int { Speed, Intensity, Mode };
    static const PedalInfo kInfo;

    Vibe() : Pedal(kInfo) {}

    static double speedHz(float speed); // 0.5 .. 8 Hz

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    static constexpr int kStages = 4;
    std::array<double, kStages> m_state{};
    double m_rate = 48000.0;
    double m_phase = 0.0;
    double m_cell = 0.0;          // the photocells' light, 0..1, lagging the lamp
    double m_cellUp = 0.0, m_cellDown = 0.0;
};

// Drum Echo: the multi-head magnetic drum echo, in broad strokes
// (docs/PEDALS.md#drum-echo): four playback heads around a spinning drum at
// one, two, three and four times the first head's time; HEADS picks which
// play; SWELL feeds them back (darkened, saturated, as the drum's echoes
// soften), so repeats build into a rhythmic swell; the drum wobbles a little.
class DrumEcho final : public Pedal
{
public:
    enum Knob : int { Time, Heads, Swell, Tone, Mix };
    static const PedalInfo kInfo;
    static constexpr double kMaxHeadSeconds = 0.45;
    static constexpr int kHeads = 4;

    DrumEcho();

    static double headSeconds(float time); // the first head: 60 .. 450 ms
    // Which heads play at a HEADS setting (bit k = head k + 1).
    static unsigned headMask(float heads);

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    std::vector<float> m_line; // four heads at the longest time, at 192 kHz
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_delay = -1.0; // the first head, samples, smoothed
    double m_glide = 0.0;
    double m_wobble = 0.0; // the drum's wobble phase
    OnePole m_toneLowpass, m_lowCut;
    float m_tone = -1.0f;
};

} // namespace krunch
