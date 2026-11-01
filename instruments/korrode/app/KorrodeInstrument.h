#pragma once

#include "app/InstrumentRegistry.h"
#include "app/PatternModel.h"
#include "app/SynthController.h"

namespace korrode {

// Korrode as a hostable instrument: the engine with its two controllers (its
// panel's "synth" and "pattern"). Built through registryEntry(), by the
// standalone app and (from K4) by the Krate rack.
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);

    krate::IInstrument &instrument() override { return synth.engine(); }
    QVariantMap panelProperties() override
    {
        return {{QStringLiteral("synth"), QVariant::fromValue(&synth)},
                {QStringLiteral("pattern"), QVariant::fromValue(&pattern)}};
    }
    // "pattern1".."pattern8" select a pattern; "runstop" toggles play.
    void triggerAction(const QString &action) override;

    SynthController synth;
    PatternModel pattern;
};

krate::InstrumentEntry registryEntry();

} // namespace Korrode
