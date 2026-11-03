#pragma once

#include "app/InstrumentRegistry.h"
#include "konnekt/app/KonnektController.h"
#include "konnekt/dsp/Engine.h"

namespace konnekt {

// Konnekt as a hostable instrument: the engine and its controller (its
// panel's "konnekt"). Built through registryEntry() by the standalone app and
// the Krate rack. It keeps its own time (its Clock modules), not the rack's.
class Instance final : public krate::InstrumentInstance
{
public:
    explicit Instance(const krate::InstrumentContext &context);

    krate::IInstrument &instrument() override { return engine; }
    QVariantMap panelProperties() override
    {
        return {{QStringLiteral("konnekt"), QVariant::fromValue(&controller)}};
    }
    // Its Volume and its patch (modules, knobs and cables).
    QJsonObject saveSound() override;
    void loadSound(const QJsonObject &sound) override;

    // Declaration order: the controller (which frees the modules) goes before the engine.
    Engine engine;
    KonnektController controller;
};

krate::InstrumentEntry registryEntry();

} // namespace konnekt
