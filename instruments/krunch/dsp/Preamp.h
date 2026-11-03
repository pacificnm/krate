#pragma once

#include <array>
#include <cstdint>

#include "dsp/Filters.h"
#include "dsp/Head.h"
#include "dsp/Oversampler.h"
#include "dsp/ToneStack.h"
#include "dsp/Tube.h"

namespace krunch {

// One channel's preamp (docs/DSP.md#preamp): two to four triode stages
// (dsp/Tube.h) at 4x the sample rate, in volts, wired as the head says
// (dsp/Head.h: coupling capacitors, cathode bypass, the Gain pot between the
// first two stages, dividers between the rest), then the tone stack and
// presence. Mono, in place, base rate.
class Preamp
{
public:
    struct Settings
    {
        Voice voice = Voice::Clean;
        const HeadDesign *head = nullptr; // a head of its own (else the voice's built-in one)
        PreTube tube = PreTube::HighGain; // a built-in head's tubes
        float gain = 0.5f; // controls 0..1
        float bass = 0.5f;
        float mid = 0.5f;
        float treble = 0.5f;
        float presence = 0.5f;
        bool operator==(const Settings &) const = default;
    };

    static constexpr int kMaxStages = HeadDesign::kMaxStages;
    // A guitar at full scale is this many volts at the first grid (a hot pickup
    // peaks around 0.5-1 V; an interface's instrument input reaches full scale above that).
    static constexpr double kInputVolts = 1.0;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // Between blocks; retunes only what changed.
    void set(const Settings &settings) noexcept;
    void process(float *buffer, std::uint32_t frames) noexcept;

private:
    void retune() noexcept;

    double m_rate = 48000.0;
    Settings m_settings;
    bool m_tuned = false;

    Biquad m_inputHighpass; // the input jack's coupling (DC, rumble)
    Oversampler4x m_oversampler;
    std::array<TriodeStage, kMaxStages> m_stages;
    std::array<double, kMaxStages> m_into{}; // what reaches each stage's grid: the Gain pot, dividers
    int m_stageCount = 1;
    double m_outScale = 1.0; // volts out of the last stage to the tone stack's level
    OnePole m_outCoupling;   // the last stage's coupling capacitor (its bias moving is no thump out)
    ToneStack m_stack;
    Biquad m_presence;
    Biquad m_presenceCut; // the stages' top end above where a guitar speaker reaches
    float m_trim = 1.0f;  // evens out the voices' levels
};

} // namespace krunch
