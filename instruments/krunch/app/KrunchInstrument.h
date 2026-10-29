#pragma once

#include "app/InstrumentRegistry.h"
#include "app/CabinetController.h"
#include "app/KrunchController.h"
#include "dsp/Krunch.h"

namespace krunch {

// Krunch as a hostable instrument: the amp, its panel controller ("Krunch")
// and its cabinets and IR library ("Cabinets", ~/.local/share/krunch/irs).
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    // "channel1".."channel4", "loop" (the playing channel's effects loop).
    void triggerAction(const QString &action) override;
    // The knobs, plus each channel's IRs (by library id).
    QJsonObject saveSound() override;
    void loadSound(const QJsonObject &sound) override;

    Krunch engine;
    KrunchController controller;
    CabinetController cabinets; // after the engine: empties its slots first when destroyed
};

krate::InstrumentEntry registryEntry();

} // namespace krunch
