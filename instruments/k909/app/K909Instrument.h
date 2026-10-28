#pragma once

#include "app/InstrumentRegistry.h"
#include "k909/app/DrumsController.h"
#include "k909/dsp/Engine.h"

namespace k909 {

// K909 as a hostable instrument: the engine and its panel controller ("Drums").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    DrumsController drums;
};

krate::InstrumentEntry registryEntry();

} // namespace k909
