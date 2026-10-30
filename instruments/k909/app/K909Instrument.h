#pragma once

#include "app/InstrumentRegistry.h"
#include "k909/app/DrumsController.h"
#include "k909/app/KitController.h"
#include "k909/app/SampleController.h"
#include "k909/dsp/Engine.h"

namespace k909 {

// K909 as a hostable instrument: the engine, its panel controller ("Drums"),
// the sample library ("Samples") and its kits ("Kits").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    DrumsController drums;
    SampleController samples;
    KitController kits;
};

krate::InstrumentEntry registryEntry();

} // namespace k909
