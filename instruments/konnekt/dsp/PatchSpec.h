#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "konnekt/dsp/Engine.h"

namespace konnekt {

// A patch as plain data (docs/BRIEF.md#scope-v1): modules in slots with their
// knobs (in their units, by key), and cables. The app's patch files and the
// renderer and tests build engines from it. No Qt.
struct PatchSpec
{
    struct ModuleSpec
    {
        int slot = 0;
        std::string type;
        std::vector<std::pair<std::string, float>> knobs{}; // key, value in its units
        int row = 0, column = 0;                          // where it sits on the rack (the app's)
    };
    struct CableSpec
    {
        int fromSlot = 0;
        std::string fromJack;
        int toSlot = 0;
        std::string toJack;
    };
    int voices = 4;
    std::vector<ModuleSpec> modules;
    std::vector<CableSpec> cables;
};

// Jack and knob indices by key (-1: none).
int inputIndex(const ModuleInfo &info, const std::string &key);
int outputIndex(const ModuleInfo &info, const std::string &key);
int knobIndex(const ModuleInfo &info, const std::string &key);

// Builds `spec` into an engine that has no modules yet: makes the modules
// (prepared at the engine's rate), posts them, the cables and the order; the
// engine takes them at its next block. The modules are returned by slot for
// the caller to own (free them after the engine lets them go, or when it is gone).
// Unknown types, jacks and knobs are skipped (and counted in *skipped).
std::vector<std::unique_ptr<Module>> build(Engine &engine, const PatchSpec &spec, int *skipped = nullptr);

} // namespace konnekt
