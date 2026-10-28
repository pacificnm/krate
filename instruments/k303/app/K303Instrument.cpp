#include "app/K303Instrument.h"

namespace k303 {

Instance::Instance(const krate::InstrumentContext &context)
    : synth(context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , pattern(synth.engine(), context.dataDir + QStringLiteral("/patterns.json"), nullptr)
{
}

void Instance::triggerAction(const QString &action)
{
    if (action == u"runstop")
        synth.setPlaying(!synth.playing());
    else if (action.startsWith(u"pattern"))
        pattern.setPatternIndex(action.mid(7).toInt() - 1);
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("k303");
    entry.name = QStringLiteral("k303");
    entry.defaultMidiChannel = 0; // channel 1
    entry.qmlModule = QStringLiteral("K303");
    entry.panelComponent = QStringLiteral("Panel");
    entry.actions = {
        QStringLiteral("pattern1"), QStringLiteral("pattern2"), QStringLiteral("pattern3"), QStringLiteral("pattern4"),
        QStringLiteral("pattern5"), QStringLiteral("pattern6"), QStringLiteral("pattern7"), QStringLiteral("pattern8"),
        QStringLiteral("runstop"),
    };
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace k303
