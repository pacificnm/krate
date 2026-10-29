#pragma once

#include "app/InstrumentRegistry.h"
#include "kalliope/app/KalliopeController.h"
#include "kalliope/app/PatchController.h"
#include "kalliope/dsp/Engine.h"

namespace kalliope {

// Kalliope as a hostable instrument: the engine, its panel controller
// ("Kalliope") and its patches ("Patches").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    KalliopeController controller;
    PatchController patches;
};

krate::InstrumentEntry registryEntry();

} // namespace kalliope
