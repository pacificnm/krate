#include "app/K303Instrument.h"

namespace k303 {

Instance::Instance(const krate::InstrumentContext &context)
    : synth(context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , pattern(synth.engine(), context.dataDir + QStringLiteral("/patterns.json"), nullptr)
{
    if (context.run)
        synth.setRunControl(context.run, QStringLiteral("k303"));
}

void Instance::triggerAction(const QString &action)
{
    if (action == u"runstop")
        synth.setPlaying(!synth.playing());
    else if (action.startsWith(u"pattern")) // 1..8 in the current (or waiting) bank
        pattern.pressNumber(action.mid(7).toInt() - 1);
    else if (action.startsWith(u"bank"))
        pattern.pressBank(action.mid(4).toInt() - 1);
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("k303");
    entry.name = QStringLiteral("k303");
    // None of its own: with one keyboard, the keys play whichever instrument has
    // focus (Kalliope keeps channel 2, K909 channel 10 for pads).
    entry.defaultMidiChannel = -1;
    entry.qmlModule = QStringLiteral("K303");
    entry.panelComponent = QStringLiteral("Panel");
    entry.actions = {
        QStringLiteral("pattern1"), QStringLiteral("pattern2"), QStringLiteral("pattern3"), QStringLiteral("pattern4"),
        QStringLiteral("pattern5"), QStringLiteral("pattern6"), QStringLiteral("pattern7"), QStringLiteral("pattern8"),
        QStringLiteral("runstop"),
        QStringLiteral("bank1"), QStringLiteral("bank2"), QStringLiteral("bank3"), QStringLiteral("bank4"),
        QStringLiteral("bank5"), QStringLiteral("bank6"), QStringLiteral("bank7"), QStringLiteral("bank8"),
    };
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace k303
