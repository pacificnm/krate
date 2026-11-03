#include "dsp/Head.h"

#include <algorithm>

namespace krunch {

namespace {

// A built-in head: the preamp's stages, dividers, Gain pot, tone stack and
// trim; the tubes and the power amp are the channel's steppers.
HeadDesign voice(int stages, std::array<TriodeStage::Design, HeadDesign::kMaxStages> stage,
                 std::array<double, HeadDesign::kMaxStages> divider, double potMinDb, double padIntoSecond,
                 const ToneStack::Parts &stack, double trimDb)
{
    HeadDesign h;
    h.stages = stages;
    h.stage = stage;
    h.divider = divider;
    h.potMinDb = potMinDb;
    h.padIntoSecond = padIntoSecond;
    h.stack = stack;
    h.trimDb = trimDb;
    return h;
}

} // namespace

// Each voice wired as its kind of amp is. A stage: {cathode ohms, bypass Hz
// (below it the cathode isn't bypassed: less gain), coupling capacitor Hz in
// front, treble roll-off Hz}.
// - Clean: two stages, fully bypassed, big coupling caps, the tone stack's
//   loss between them; it only breaks up with the Gain high.
// - Crunch: a partly bypassed first stage (less bass into the drive), the
//   Gain pot, a cold-biased clipper (unbypassed 10 k: asymmetric), a third.
// - Heavy: four stages; small coupling caps and partial bypasses at every one
//   (each clips only from the low mids up: tight palm mutes, no mud), the
//   cold clipper second, more treble rolled off as it goes (no fizz).
// - Lead: four stages, more bass let through and rounder (bigger caps, lower
//   roll-offs): thick, sustaining.
const HeadDesign &builtInHead(Voice v)
{
    static const HeadDesign kHeads[int(Voice::Count)] = {
        voice(2, {{{1500, 4, 25, 14000}, {1500, 4, 20, 11000}, {}, {}}}, {0, 0, 0, 0}, -32.0, 0.12, ToneStack::kAmerican, 20.0),
        voice(3, {{{2700, 40, 8, 12000}, {10000, 1500, 20, 6000}, {1500, 50, 25, 3500}, {}}}, {0, 0, 0.45, 0}, -30.0, 1.0,
              ToneStack::kBritish, 0.0),
        voice(4, {{{1500, 60, 15, 12000}, {10000, 1500, 40, 6000}, {1500, 60, 30, 3500}, {1500, 60, 30, 3000}}},
              {0, 0, 0.22, 0.25}, -40.0, 1.0, ToneStack::kBritish, 0.0),
        voice(4, {{{1500, 40, 10, 11000}, {2700, 100, 30, 6000}, {1500, 70, 30, 3800}, {1500, 100, 40, 3200}}},
              {0, 0, 0.25, 0.2}, -38.0, 1.0, ToneStack::kBritish, 0.0),
    };
    return kHeads[std::clamp(int(v), 0, int(Voice::Count) - 1)];
}

} // namespace krunch
