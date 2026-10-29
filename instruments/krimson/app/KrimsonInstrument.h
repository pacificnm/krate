#pragma once

#include "app/InstrumentRegistry.h"
#include "krimson/app/KrimsonController.h"
#include "krimson/app/ProgramController.h"
#include "krimson/app/SlotController.h"
#include "krimson/dsp/Engine.h"

namespace krimson {

// Krimson as a hostable instrument: the engine, its panel controller
// ("Krimson"), the slots' setup ("Slots") and the programs ("Programs").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    void triggerAction(const QString &action) override;
    // The instrument parameters plus all four slots (sounds, morph targets, set-up).
    QJsonObject saveSound() override;
    void loadSound(const QJsonObject &sound) override;

    Engine engine;
    KrimsonController controller;
    SlotController slotSetup;
    ProgramController programs;
};

krate::InstrumentEntry registryEntry();

} // namespace krimson
