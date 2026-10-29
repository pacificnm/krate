#include "app/KrunchInstrument.h"

namespace krunch {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), nullptr)
    , cabinets(engine, context.dataDir + QStringLiteral("/irs"), context.configDir + QStringLiteral("/cabinets.json"), nullptr)
    , pedals(engine, context.configDir + QStringLiteral("/pedals.json"), nullptr)
{
}

QJsonObject Instance::saveSound()
{
    QJsonObject sound = krate::InstrumentInstance::saveSound();
    sound.insert(QStringLiteral("cabinets"), cabinets.save());
    sound.insert(QStringLiteral("pedals"), pedals.save());
    return sound;
}

void Instance::loadSound(const QJsonObject &sound)
{
    krate::InstrumentInstance::loadSound(sound);
    if (sound.contains(u"cabinets"))
        cabinets.load(sound.value(u"cabinets").toArray());
    if (sound.contains(u"pedals"))
        pedals.load(sound.value(u"pedals").toArray());
}

void Instance::triggerAction(const QString &action)
{
    if (action.startsWith(u"channel")) {
        const int channel = action.mid(7).toInt() - 1;
        if (channel >= 0 && channel < kChannels)
            controller.setValue(QStringLiteral("channel"), qreal(channel) / (kChannels - 1));
    } else if (action.startsWith(u"pedal")) {
        pedals.toggle(action.mid(5).toInt() - 1);
    } else if (action == u"loop") {
        const QString key = QStringLiteral("ch%1_loop").arg(units::channel(float(controller.value(QStringLiteral("channel")))) + 1);
        controller.setValue(key, controller.value(key) > 0.5 ? 0.0 : 1.0);
    }
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("krunch");
    entry.name = QStringLiteral("Krunch");
    entry.defaultMidiChannel = -1; // plays the guitar input, not notes
    entry.playsNotes = false;
    entry.qmlModule = QStringLiteral("Krunch");
    entry.panelComponent = QStringLiteral("Panel");
    for (int c = 1; c <= kChannels; ++c)
        entry.actions << QStringLiteral("channel%1").arg(c);
    entry.actions << QStringLiteral("loop");
    for (int p = 1; p <= PedalList::kMaxPedals; ++p)
        entry.actions << QStringLiteral("pedal%1").arg(p);
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace krunch
