#pragma once

#include "app/InstrumentRegistry.h"
#include "app/CabinetController.h"
#include "app/CaptureController.h"
#include "app/KrunchController.h"
#include "app/PedalController.h"
#include "app/PresetController.h"
#include "app/TunerController.h"
#include "dsp/Krunch.h"

namespace krunch {

// Krunch as a hostable instrument: the amp, its panel controller ("Krunch")
// and its cabinets and IR library ("Cabinets", ~/.local/share/krunch/irs),
// its amp captures ("Captures", ~/.local/share/krunch/captures).
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    QVariantMap panelProperties() override
    {
        return {{QStringLiteral("krunch"), QVariant::fromValue(&controller)},
                {QStringLiteral("presets"), QVariant::fromValue(&presets)},
                {QStringLiteral("cabinets"), QVariant::fromValue(&cabinets)},
                {QStringLiteral("captures"), QVariant::fromValue(&captures)},
                {QStringLiteral("pedals"), QVariant::fromValue(&pedals)},
                {QStringLiteral("tuner"), QVariant::fromValue(&tuner)}};
    }
    // "channel1".."channel4", "loop" (the playing channel's effects loop),
    // "cabinet" (the playing channel's cabinet on or off),
    // "pedal1".."pedal8" (the footswitches, by position on the board),
    // "presetNext", "presetPrevious", "tuner" (open or close it).
    void triggerAction(const QString &action) override;
    // The knobs, plus each channel's IRs (by library id), capture (by name), and the pedalboard.
    QJsonObject saveSound() override;
    void loadSound(const QJsonObject &sound) override;

    Krunch engine;
    KrunchController controller;
    CabinetController cabinets; // after the engine: empties its slots first when destroyed
    CaptureController captures; // likewise
    PedalController pedals;     // likewise: takes the pedals off the chain first
    PresetController presets;   // the whole amp, saved and loaded by name
    TunerController tuner;      // after the engine: closes its tap and unmutes first
};

krate::InstrumentEntry registryEntry();

} // namespace krunch
