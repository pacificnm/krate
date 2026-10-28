#include "app/K909Instrument.h"

namespace k909 {

Instance::Instance(const krate::InstrumentContext &context)
    : drums(engine, context.configDir + QStringLiteral("/panel.json"), context.dataDir + QStringLiteral("/patterns.json"),
            context.transport, nullptr)
{
}

void Instance::triggerAction(const QString &action)
{
    if (action == u"startstop")
        drums.setPlaying(!drums.playing());
    else if (action.size() == 8 && action.startsWith(u"pattern")) // patternA..patternH
        drums.setPatternIndex(action.at(7).unicode() - u'A');
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("k909");
    entry.name = QStringLiteral("K909");
    entry.defaultMidiChannel = 9; // channel 10, the usual drum channel
    entry.qmlModule = QStringLiteral("K909");
    entry.panelComponent = QStringLiteral("Panel");
    entry.actions = {
        QStringLiteral("patternA"), QStringLiteral("patternB"), QStringLiteral("patternC"), QStringLiteral("patternD"),
        QStringLiteral("patternE"), QStringLiteral("patternF"), QStringLiteral("patternG"), QStringLiteral("patternH"),
        QStringLiteral("startstop"),
    };
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace k909
