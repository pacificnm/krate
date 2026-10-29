#pragma once

#include <array>
#include <cstdint>

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/ToneStack.h"

namespace krunch {

// A channel's voice: how many gain stages and how they're filtered.
enum class Voice : int { Clean, Crunch, Heavy, Lead, Count };

// One channel's preamp (docs/DSP.md#preamp): a pre-filter (how much bass and
// mid the first stage sees), one to four valve-like gain stages at 4x the
// sample rate, the tone stack and presence. Mono, in place, base rate.
class Preamp
{
public:
    struct Settings
    {
        Voice voice = Voice::Clean;
        float gain = 0.5f; // controls 0..1
        float bass = 0.5f;
        float mid = 0.5f;
        float treble = 0.5f;
        float presence = 0.5f;
        bool operator==(const Settings &) const = default;
    };

    static constexpr int kMaxStages = 4;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // Between blocks; retunes only what changed.
    void set(const Settings &settings) noexcept;
    void process(float *buffer, std::uint32_t frames) noexcept;

private:
    // A triode stage: gain, a biased soft clipper (the bias makes it
    // asymmetric: even harmonics), the coupling capacitor and the stage's
    // treble roll-off.
    struct Stage
    {
        double gain = 1.0;
        double bias = 0.0;
        double offset = 0.0; // the clipper's output at rest, removed
        OnePole coupling;    // high-pass
        OnePole rolloff;     // low-pass
        float process(float x) noexcept;
    };

    void retune() noexcept;

    double m_rate = 48000.0;
    Settings m_settings;
    bool m_tuned = false;

    Biquad m_preHighpass; // tightens the bass into the first stage
    Biquad m_preMid;      // a mid push into the first stage
    Oversampler4x m_oversampler;
    std::array<Stage, kMaxStages> m_stages;
    int m_stageCount = 1;
    ToneStack m_stack;
    Biquad m_presence;
    Biquad m_presenceCut; // the stages' top end above where a guitar speaker reaches
    float m_trim = 1.0f;  // evens out the voices' levels
};

} // namespace krunch
