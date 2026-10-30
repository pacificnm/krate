#pragma once

#include "app/InstrumentRegistry.h"
#include "k909/app/DrumsController.h"
#include "k909/app/KitController.h"
#include "k909/dsp/Engine.h"

namespace k909 {

// K909 as a hostable instrument: the engine, its panel controller ("Drums")
// and its kits ("Kits").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    DrumsController drums;
    KitController kits;
};

krate::InstrumentEntry registryEntry();

} // namespace k909
