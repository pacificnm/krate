#include "kobalt/app/KobaltInstrument.h"

#include <algorithm>

#include "kobalt/dsp/Params.h"

namespace kobalt {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , patches(controller, engine, context.dataDir + QStringLiteral("/patches.json"), nullptr)
{
    if (context.run)
        controller.setRunControl(context.run, QStringLiteral("kobalt"));
}

void Instance::triggerAction(const QString &action)
{
    const auto toggle = [this](const QString &key) {
        controller.setValue(key, controller.value(key) > 0.5 ? 0.0 : 1.0);
    };
    const QString playMode = QStringLiteral("play_mode");
    if (action.startsWith(u"bank"))
        patches.pressBank(action.mid(4).toInt() - 1);
    else if (action.startsWith(u"patch"))
        patches.pressPatch(action.mid(5).toInt() - 1);
    else if (action == u"write")
        patches.toggleWrite();
    else if (action == u"note")
        controller.setValue(playMode, 0.0);
    else if (action == u"arp") // Arpeggio, or back to Note
        controller.setValue(playMode, controller.value(playMode) == 0.5 ? 0.0 : 0.5);
    else if (action == u"seq") // Sequencer, or back to Note
        controller.setValue(playMode, controller.value(playMode) == 1.0 ? 0.0 : 1.0);
    else if (action == u"hold")
        toggle(QStringLiteral("arp_hold"));
    else if (action == u"start")
        controller.setPlaying(!controller.playing());
    else if (action == u"octave_down" || action == u"octave_up") {
        const int shift = units::octaveShift(float(controller.value(QStringLiteral("octave"))));
        const int next = std::clamp(shift + (action == u"octave_up" ? 1 : -1), -3, 3);
        controller.setValue(QStringLiteral("octave"), (next + 3) / 6.0);
    }
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("kobalt");
    entry.name = QStringLiteral("Kobalt");
    entry.defaultMidiChannel = 6; // channel 7
    entry.qmlModule = QStringLiteral("Kobalt");
    entry.panelComponent = QStringLiteral("Panel");
    for (int i = 1; i <= 8; ++i)
        entry.actions.append(QStringLiteral("bank%1").arg(i));
    for (int i = 1; i <= 8; ++i)
        entry.actions.append(QStringLiteral("patch%1").arg(i));
    entry.actions << QStringLiteral("note") << QStringLiteral("arp") << QStringLiteral("seq") << QStringLiteral("hold")
                  << QStringLiteral("write") << QStringLiteral("start") << QStringLiteral("rest") << QStringLiteral("tie")
                  << QStringLiteral("octave_down") << QStringLiteral("octave_up");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace kobalt
