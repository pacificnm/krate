#pragma once

#include <array>

#include "dsp/ToneStack.h"
#include "dsp/Tube.h"

namespace krunch {

// A channel's voice: the four built-in heads.
enum class Voice : int { Clean, Crunch, Heavy, Lead, Count };

// An amp head (docs/HEADS.md): how Krunch's tube amp is built, from the input
// to the speaker jack. The four voices are built-in heads; others come from
// files (fitted to an amp capture by krunch-fit, or saved by hand). Plain
// data: handed to the audio thread by pointer.
struct HeadDesign
{
    static constexpr int kMaxStages = 4;

    // --- The preamp: two to four triode stages (dsp/Tube.h).
    int stages = 2;
    std::array<TriodeStage::Design, kMaxStages> stage{};
    bool channelTube = true;                  // every stage the channel's TUBES choice (the built-ins)
    std::array<PreTube, kMaxStages> tube{};   // otherwise each stage's own
    std::array<double, kMaxStages> divider{}; // into the third and fourth stages
    double potMinDb = -30.0;                  // the Gain pot (between the first two stages): its range, up to 0 dB
    double padIntoSecond = 1.0;               // a fixed loss into the second stage as well (the clean voice's tone stack)
    ToneStack::Parts stack = ToneStack::kBritish;
    double topCutHz = 11000.0; // the stages' top end above where a guitar speaker reaches
    double trimDb = 0.0;       // the preamp's level into the power amp

    // --- The power amp.
    bool channelPower = true;  // the POWER TUBE stepper's tubes (the built-ins); otherwise `power`
    PowerTubeDesign power{1.0, 2.5, 0.35, 1.0, 800.0, 0.0, 0.7};
    double masterOffsetDb = 0.0; // how hard Master's noon drives the pair, against the built-ins
    // After the power amp: the output transformer and the speaker's pull (off: lowCutHz 0).
    double lowCutHz = 0.0, highCutHz = 0.0;
    double peakHz = 1000.0, peakDb = 0.0, peakQ = 0.7;
    double outputDb = 0.0; // level: a fitted head as loud as its capture
};

const HeadDesign &builtInHead(Voice voice);

} // namespace krunch
