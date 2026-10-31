#include "kessel/app/KesselInstrument.h"

namespace kessel {

Instance::Instance(const krate::InstrumentContext &context)
    : drums(engine, context.configDir + QStringLiteral("/panel.json"), context.dataDir + QStringLiteral("/patterns.json"),
            context.transport, nullptr)
    , samples(engine, context.dataDir, nullptr)
    , kits(drums, samples, context.dataDir + QStringLiteral("/kits.json"), nullptr)
{
    if (context.run)
        drums.setRunControl(context.run, QStringLiteral("kessel"));
}

void Instance::triggerAction(const QString &action)
{
    if (action == u"startstop")
        drums.setPlaying(!drums.playing());
    else if (action.size() == 8 && action.startsWith(u"pattern")) // patternA..patternP
        drums.setPatternIndex(action.at(7).unicode() - u'A');
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("kessel");
    entry.name = QStringLiteral("Kessel");
    entry.defaultMidiChannel = 9; // channel 10, the usual drum channel
    entry.qmlModule = QStringLiteral("Kessel");
    entry.panelComponent = QStringLiteral("Panel");
    entry.actions = {
        QStringLiteral("patternA"), QStringLiteral("patternB"), QStringLiteral("patternC"), QStringLiteral("patternD"),
        QStringLiteral("patternE"), QStringLiteral("patternF"), QStringLiteral("patternG"), QStringLiteral("patternH"),
        QStringLiteral("patternI"), QStringLiteral("patternJ"), QStringLiteral("patternK"), QStringLiteral("patternL"),
        QStringLiteral("patternM"), QStringLiteral("patternN"), QStringLiteral("patternO"), QStringLiteral("patternP"),
        QStringLiteral("startstop"),
    };
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace Kessel
