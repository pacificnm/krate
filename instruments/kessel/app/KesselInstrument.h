#pragma once

#include "app/InstrumentRegistry.h"
#include "kessel/app/DrumsController.h"
#include "kessel/app/KitController.h"
#include "kessel/app/SampleController.h"
#include "kessel/dsp/Engine.h"

namespace kessel {

// Kessel as a hostable instrument: the engine, its panel controller ("Drums"),
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

} // namespace Kessel
