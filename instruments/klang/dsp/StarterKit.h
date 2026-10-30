#pragma once

#include <string>
#include <vector>

#include "klang/dsp/Pattern.h"
#include "klang/dsp/Sample.h"

namespace klang {

class Engine;

// The starter kit (docs/BRIEF.md#starter-kit): 16 sounds Klang synthesizes
// itself, so it ships no one's samples: drums, metal, noise, a one-bar break
// to slice, a drone, two vowel-like voices, a reverse swell, a stab, a bass
// pluck, a glitch. Deterministic. Not realtime.
struct StarterSound
{
    std::string file; // e.g. "01 kick.wav"
    Sample sample;
};
std::vector<StarterSound> starterSounds(double rate = 44100.0);

// Each pad's settings for its starter sound (on top of the defaults).
void applyStarterPad(Engine &engine, int pad);
// Patterns A (an industrial beat with the gated drone and stabs) and B (the
// break looped, with the voices); the others empty.
Pattern starterPattern(int index);

} // namespace klang
