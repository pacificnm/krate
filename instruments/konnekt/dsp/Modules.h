#pragma once

#include <span>
#include <string_view>

#include "konnekt/dsp/Module.h"

namespace konnekt {

// Every module type, in the order the module list shows them.
std::span<const ModuleInfo *const> moduleTypes();
const ModuleInfo *findModule(std::string_view type);

// The types, for the registry (one per file of modules).
namespace modules {
extern const ModuleInfo kMidiIn, kOscillator, kEnvelope, kLfo, kVca, kMixer, kAttenuverter, kOutput;
extern const ModuleInfo kNoise, kLadder, kOtaFilter, kSvf, kSampleHold, kSlew, kDrive, kCrusher, kRingMod, kClock,
    kSequencer, kChorus, kDelay, kReverb, kScope;
} // namespace modules

} // namespace konnekt
