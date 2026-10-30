#include "klang/app/KlangInstrument.h"

namespace klang {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , pads(controller, engine, context.dataDir, nullptr)
    , patterns(engine, context.dataDir + QStringLiteral("/patterns.json"), nullptr)
{
    if (context.run)
        controller.setRunControl(context.run, QStringLiteral("klang"));
}

void Instance::triggerAction(const QString &action)
{
    if (action.startsWith(u"pad"))
        controller.pressPad(action.mid(3).toInt() - 1);
    else if (action.startsWith(u"pattern"))
        patterns.select(action.mid(7).toInt() - 1);
    else if (action == u"start")
        controller.setPlaying(!controller.playing());
    else if (action == u"slice")
        pads.slice(controller.editedPad(), 8);
    else if (action == u"next_pad")
        controller.setEditedPad((controller.editedPad() + 1) % Engine::kPads);
    else if (action == u"previous_pad")
        controller.setEditedPad((controller.editedPad() + Engine::kPads - 1) % Engine::kPads);
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("klang");
    entry.name = QStringLiteral("Klang");
    entry.defaultMidiChannel = 10; // channel 11
    entry.qmlModule = QStringLiteral("Klang");
    entry.panelComponent = QStringLiteral("Panel");
    for (int i = 1; i <= Engine::kPads; ++i)
        entry.actions.append(QStringLiteral("pad%1").arg(i));
    for (int i = 1; i <= kPatterns; ++i)
        entry.actions.append(QStringLiteral("pattern%1").arg(i));
    entry.actions << QStringLiteral("start") << QStringLiteral("slice") << QStringLiteral("next_pad")
                  << QStringLiteral("previous_pad");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace klang
