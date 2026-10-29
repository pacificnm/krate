#pragma once

#include "app/InstrumentRegistry.h"
#include "krimson/app/KrimsonController.h"
#include "krimson/dsp/Engine.h"

namespace krimson {

// Krimson as a hostable instrument: the engine and its panel controller ("Krimson").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    KrimsonController controller;
};

krate::InstrumentEntry registryEntry();

} // namespace krimson
