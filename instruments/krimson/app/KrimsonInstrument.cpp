#include "krimson/app/KrimsonInstrument.h"

namespace krimson {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
{
}

void Instance::triggerAction(const QString &action)
{
    if (action.size() == 5 && action.startsWith(u"slot")) // slotA..slotD
        controller.setEditedSlot(action.at(4).unicode() - u'A');
    // Programs, store, arpeggiator, morph and octave arrive in later milestones.
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("krimson");
    entry.name = QStringLiteral("Krimson");
    entry.defaultMidiChannel = 2; // channel 3: slot A (B-D on 4-6 arrive with the rack work, C8)
    entry.qmlModule = QStringLiteral("Krimson");
    entry.panelComponent = QStringLiteral("Panel");
    for (const char *slot : {"slotA", "slotB", "slotC", "slotD"})
        entry.actions << QString::fromLatin1(slot);
    for (int i = 1; i <= 8; ++i)
        entry.actions << QStringLiteral("bank%1").arg(i);
    for (int i = 1; i <= 8; ++i)
        entry.actions << QStringLiteral("program%1").arg(i);
    entry.actions << QStringLiteral("store") << QStringLiteral("arp") << QStringLiteral("hold") << QStringLiteral("morph")
                  << QStringLiteral("octave_down") << QStringLiteral("octave_up");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace krimson
