#pragma once

#include "app/InstrumentRegistry.h"
#include "app/KlickController.h"
#include "dsp/Klick.h"

namespace klick {

// Klick as a hostable instrument: the metronome and its panel controller ("Klick").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }

    Klick engine;
    KlickController controller;
};

krate::InstrumentEntry registryEntry();

} // namespace klick
