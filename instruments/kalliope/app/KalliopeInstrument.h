#pragma once

#include "app/InstrumentRegistry.h"
#include "kalliope/app/KalliopeController.h"
#include "kalliope/dsp/Engine.h"

namespace kalliope {

// Kalliope as a hostable instrument: the engine and its panel controller ("Kalliope").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    KalliopeController controller;
};

krate::InstrumentEntry registryEntry();

} // namespace kalliope
