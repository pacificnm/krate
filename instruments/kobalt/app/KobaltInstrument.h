#pragma once

#include "app/InstrumentRegistry.h"
#include "kobalt/app/KobaltController.h"
#include "kobalt/app/KobaltPatchController.h"
#include "kobalt/app/KobaltSequenceController.h"
#include "kobalt/dsp/Engine.h"

namespace kobalt {

// Kobalt as a hostable instrument: the engine, its panel controller
// ("Kobalt"), its patches ("Patches") and its step sequencer ("Sequencer").
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);
    krate::IInstrument &instrument() override { return engine; }
    QVariantMap panelProperties() override
    {
        return {{QStringLiteral("kobalt"), QVariant::fromValue(&controller)},
                {QStringLiteral("patches"), QVariant::fromValue(&patches)},
                {QStringLiteral("sequencer"), QVariant::fromValue(&sequencer)}};
    }
    void triggerAction(const QString &action) override;

    Engine engine;
    KobaltController controller;
    KobaltPatchController patches;
    KobaltSequenceController sequencer;
};

krate::InstrumentEntry registryEntry();

} // namespace kobalt
