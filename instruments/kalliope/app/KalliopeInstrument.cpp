#include "kalliope/app/KalliopeInstrument.h"

namespace kalliope {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
{
}

void Instance::triggerAction(const QString &action)
{
    Q_UNUSED(action); // bank, patch, arp, hold and write arrive in P5-P6
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("kalliope");
    entry.name = QStringLiteral("Kalliope");
    entry.defaultMidiChannel = 1; // channel 2
    entry.qmlModule = QStringLiteral("Kalliope");
    entry.panelComponent = QStringLiteral("Panel");
    for (int i = 1; i <= 8; ++i)
        entry.actions.append(QStringLiteral("bank%1").arg(i));
    for (int i = 1; i <= 8; ++i)
        entry.actions.append(QStringLiteral("patch%1").arg(i));
    entry.actions << QStringLiteral("arp") << QStringLiteral("hold") << QStringLiteral("write");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace kalliope
