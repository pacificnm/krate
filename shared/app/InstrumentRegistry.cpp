#include "app/InstrumentRegistry.h"

#include <algorithm>

#include "core/Instrument.h"

namespace krate {

QJsonObject InstrumentInstance::saveSound()
{
    IInstrument &inst = instrument();
    const auto params = inst.parameters();
    QJsonObject values;
    for (std::size_t i = 0; i < params.size(); ++i)
        values[QString::fromLatin1(params[i].key.data(), qsizetype(params[i].key.size()))] = double(inst.param(int(i)));
    return QJsonObject{{QStringLiteral("params"), values}};
}

void InstrumentInstance::loadSound(const QJsonObject &sound)
{
    IInstrument &inst = instrument();
    const auto params = inst.parameters();
    const QJsonObject values = sound.value(u"params").toObject();
    for (std::size_t i = 0; i < params.size(); ++i) {
        const QJsonValue v = values.value(QString::fromLatin1(params[i].key.data(), qsizetype(params[i].key.size())));
        if (v.isDouble())
            inst.setParamFromHost(int(i), float(std::clamp(v.toDouble(), 0.0, 1.0)));
    }
}

} // namespace krate
