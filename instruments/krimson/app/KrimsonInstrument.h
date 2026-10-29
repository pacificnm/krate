#pragma once

#include "app/InstrumentRegistry.h"
#include "krimson/app/KrimsonController.h"
#include "krimson/app/SlotController.h"
#include "krimson/dsp/Engine.h"

namespace krimson {

// Krimson as a hostable instrument: the engine, its panel controller
// ("Krimson") and the slots' setup ("Slots").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    KrimsonController controller;
    SlotController slotSetup;
};

krate::InstrumentEntry registryEntry();

} // namespace krimson
