#include "kalliope/app/KalliopeInstrument.h"

namespace kalliope {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , patches(controller, engine, context.dataDir + QStringLiteral("/patches.json"), nullptr)
{
}

void Instance::triggerAction(const QString &action)
{
    const auto toggle = [this](const QString &key) {
        controller.setValue(key, controller.value(key) > 0.5 ? 0.0 : 1.0);
    };
    if (action.startsWith(u"bank"))
        patches.pressBank(action.mid(4).toInt() - 1);
    else if (action.startsWith(u"patch"))
        patches.pressPatch(action.mid(5).toInt() - 1);
    else if (action == u"write")
        patches.toggleWrite();
    else if (action == u"arp")
        toggle(QStringLiteral("arp_on"));
    else if (action == u"hold")
        toggle(QStringLiteral("arp_hold"));
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
