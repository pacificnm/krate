#include "app/KlickInstrument.h"

namespace klick {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), nullptr)
{
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("klick");
    entry.name = QStringLiteral("Klick");
    entry.defaultMidiChannel = -1; // plays from the transport, not from notes
    entry.playsNotes = false;
    entry.qmlModule = QStringLiteral("Klick");
    entry.panelComponent = QStringLiteral("Panel");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace klick
