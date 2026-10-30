#pragma once

#include "app/InstrumentRegistry.h"
#include "klang/app/KlangController.h"
#include "klang/app/KlangPadController.h"
#include "klang/app/KlangPatternController.h"
#include "klang/dsp/Engine.h"

namespace klang {

// Klang as a hostable instrument: the engine, its panel controller ("Klang"),
// its pads and samples ("Pads") and its patterns ("Patterns").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;

    Engine engine;
    KlangController controller;
    KlangPadController pads;
    KlangPatternController patterns;
};

krate::InstrumentEntry registryEntry();

} // namespace klang
