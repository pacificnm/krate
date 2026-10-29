#pragma once

#include "app/InstrumentRegistry.h"
#include "app/KrunchController.h"
#include "dsp/Krunch.h"

namespace krunch {

// Krunch as a hostable instrument: the amp and its panel controller ("Krunch").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    // "channel1".."channel4", "loop" (the playing channel's effects loop).
    void triggerAction(const QString &action) override;

    Krunch engine;
    KrunchController controller;
};

krate::InstrumentEntry registryEntry();

} // namespace krunch
