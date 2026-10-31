#include "konnekt/dsp/PatchSpec.h"

#include "konnekt/dsp/Modules.h"

namespace konnekt {

namespace {
template<typename T>
int find(std::span<const T> items, const std::string &key)
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (key == items[i].key)
            return int(i);
    return -1;
}
} // namespace

int inputIndex(const ModuleInfo &info, const std::string &key) { return find(info.inputs, key); }
int outputIndex(const ModuleInfo &info, const std::string &key) { return find(info.outputs, key); }
int knobIndex(const ModuleInfo &info, const std::string &key) { return find(info.knobs, key); }

std::vector<std::unique_ptr<Module>> build(Engine &engine, const PatchSpec &spec, int *skipped)
{
    int skips = 0;
    std::vector<std::unique_ptr<Module>> modules(kMaxModules);
    std::vector<int> slots;
    for (const auto &m : spec.modules) {
        const ModuleInfo *info = findModule(m.type);
        if (!info || m.slot < 0 || m.slot >= kMaxModules || modules[std::size_t(m.slot)]) {
            ++skips;
            continue;
        }
        auto module = std::unique_ptr<Module>(info->create());
        for (const auto &[key, value] : m.knobs) {
            const int k = knobIndex(*info, key);
            if (k < 0) {
                ++skips;
                continue;
            }
            module->setKnobPosition(k, info->knobs[std::size_t(k)].position(value));
        }
        module->prepare(engine.sampleRate());
        engine.addModule(m.slot, module.get());
        modules[std::size_t(m.slot)] = std::move(module);
        slots.push_back(m.slot);
    }
    std::vector<Engine::Cable> cables;
    for (const auto &c : spec.cables) {
        const Module *from = c.fromSlot >= 0 && c.fromSlot < kMaxModules ? modules[std::size_t(c.fromSlot)].get() : nullptr;
        const Module *to = c.toSlot >= 0 && c.toSlot < kMaxModules ? modules[std::size_t(c.toSlot)].get() : nullptr;
        const int out = from ? outputIndex(from->info(), c.fromJack) : -1;
        const int in = to ? inputIndex(to->info(), c.toJack) : -1;
        if (out < 0 || in < 0) {
            ++skips;
            continue;
        }
        engine.connect(c.toSlot, in, c.fromSlot, out);
        cables.push_back({c.fromSlot, out, c.toSlot, in});
    }
    engine.setOrder(Engine::signalOrder(slots, cables));
    engine.setVoices(spec.voices);
    if (skipped)
        *skipped = skips;
    return modules;
}

} // namespace konnekt
