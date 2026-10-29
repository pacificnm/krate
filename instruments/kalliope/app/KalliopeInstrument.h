#pragma once

#include "app/InstrumentRegistry.h"
#include "kalliope/app/KalliopeController.h"
#include "kalliope/app/PatchController.h"
#include "kalliope/app/SequenceController.h"
#include "kalliope/dsp/Engine.h"

namespace kalliope {

// Kalliope as a hostable instrument: the engine, its panel controller
// ("Kalliope"), its patches ("Patches") and its step sequencer ("Sequencer").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    KalliopeController controller;
    PatchController patches;
    SequenceController sequencer;
};

krate::InstrumentEntry registryEntry();

} // namespace kalliope
