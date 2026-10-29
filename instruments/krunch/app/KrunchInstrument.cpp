#include "app/KrunchInstrument.h"

namespace krunch {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), nullptr)
{
}

void Instance::triggerAction(const QString &action)
{
    if (action.startsWith(u"channel")) {
        const int channel = action.mid(7).toInt() - 1;
        if (channel >= 0 && channel < kChannels)
            controller.setValue(QStringLiteral("channel"), qreal(channel) / (kChannels - 1));
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
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace krunch
