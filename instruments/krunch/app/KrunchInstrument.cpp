#include "app/KrunchInstrument.h"

namespace krunch {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), nullptr)
{
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("krunch");
    entry.name = QStringLiteral("Krunch");
    entry.defaultMidiChannel = -1; // plays the guitar input, not notes
    entry.playsNotes = false;
    entry.qmlModule = QStringLiteral("Krunch");
    entry.panelComponent = QStringLiteral("Panel");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace krunch
