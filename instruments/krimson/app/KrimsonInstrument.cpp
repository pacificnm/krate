#include "krimson/app/KrimsonInstrument.h"

#include <algorithm>

namespace krimson {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , slotSetup(controller, engine, context.configDir + QStringLiteral("/slots.json"), nullptr)
    , programs(controller, slotSetup, engine, context.dataDir + QStringLiteral("/programs.json"),
               context.dataDir + QStringLiteral("/performances.json"), nullptr)
{
}

QJsonObject Instance::saveSound()
{
    QJsonObject sound = InstrumentInstance::saveSound();
    sound[QStringLiteral("slots")] = slotSetup.toJson();
    return sound;
}

void Instance::loadSound(const QJsonObject &sound)
{
    InstrumentInstance::loadSound(sound); // Volume, Octave... and the edited slot (the slots below win)
    if (const QJsonObject slotState = sound.value(u"slots").toObject(); !slotState.isEmpty()) {
        slotSetup.fromJson(slotState);
        slotSetup.save();
    }
}

void Instance::triggerAction(const QString &action)
{
    if (action.size() == 5 && action.startsWith(u"slot")) // slotA..slotD: play and edit that slot
        slotSetup.pressSlot(action.at(4).unicode() - u'A', false);
    else if (action == u"morph")
        controller.setMorphEdit(!controller.morphEdit());
    else if (action == u"store")
        programs.pressStore();
    else if (action.startsWith(u"bank")) // bankN: program N1 in the edited slot
        programs.select((action.mid(4).toInt() - 1) * persistence::kProgramsPerBank);
    else if (action.startsWith(u"program")) // programN: that number in the current bank
        programs.select(programs.number() / persistence::kProgramsPerBank * persistence::kProgramsPerBank
                        + action.mid(7).toInt() - 1);
    else if (action == u"octave_down" || action == u"octave_up") {
        const int shift = units::octaveShift(float(controller.value(QStringLiteral("octave"))));
        const int next = std::clamp(shift + (action == u"octave_up" ? 1 : -1), -3, 3);
        controller.setValue(QStringLiteral("octave"), (next + 3) / 6.0);
    }
    else if (action == u"arp" || action == u"hold") { // the edited slot's
        const QString key = action == u"arp" ? QStringLiteral("arp_on") : QStringLiteral("arp_hold");
        controller.setValue(key, controller.value(key) > 0.5 ? 0.0 : 1.0);
    }
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("krimson");
    entry.name = QStringLiteral("Krimson");
    entry.defaultMidiChannel = 2;     // channel 3: slot A
    entry.extraMidiChannels = {3, 4, 5}; // channels 4-6: slots B-D
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
