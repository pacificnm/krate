#include "kessel/app/KesselInstrument.h"

#include <QJsonArray>

namespace kessel {

Instance::Instance(const krate::InstrumentContext &context)
    : drums(engine, context.configDir + QStringLiteral("/panel.json"), context.dataDir + QStringLiteral("/patterns.json"),
            context.transport, nullptr)
    , samples(engine, context.dataDir, nullptr)
    , kits(drums, samples, context.dataDir + QStringLiteral("/kits.json"), nullptr)
{
    if (context.run)
        drums.setRunControl(context.run, context.id.isEmpty() ? QStringLiteral("kessel") : context.id);
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
    entry.maxCopies = 16; // the Krate rack: up to 16 of it
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

namespace {
// Kessel's drums as the sound library shows them (DrumId order).
const QStringList &drumNames()
{
    static const QStringList names{
        QStringLiteral("Kick"),      QStringLiteral("Snare"),     QStringLiteral("Low Tom"), QStringLiteral("Mid Tom"),
        QStringLiteral("High Tom"),  QStringLiteral("Rimshot"),   QStringLiteral("Clap"),    QStringLiteral("Closed Hat"),
        QStringLiteral("Open Hat"),  QStringLiteral("Crash"),     QStringLiteral("Ride"),    QStringLiteral("Metal"),
        QStringLiteral("Clank"),     QStringLiteral("Noise"),     QStringLiteral("Boom"),    QStringLiteral("Sample")};
    return names;
}
} // namespace

QStringList Instance::sampleTargets() const
{
    return drumNames();
}

QStringList Instance::sampleTargetContents() const
{
    return samples.drumSamples();
}

bool Instance::assignSample(int target, const QString &path, QString *error)
{
    if (target < 0 || target >= DrumCount)
        return false;
    if (samples.assignFile(target, path))
        return true;
    if (error)
        *error = samples.message();
    return false;
}

QJsonObject Instance::saveSound()
{
    QJsonObject sound = InstrumentInstance::saveSound();
    sound.insert(QStringLiteral("samples"), QJsonArray::fromStringList(samples.drumSamples()));
    return sound;
}

void Instance::loadSound(const QJsonObject &sound)
{
    InstrumentInstance::loadSound(sound);
    if (!sound.contains(u"samples"))
        return; // (saved before drum samples were kept: as they are)
    const QJsonArray names = sound.value(u"samples").toArray();
    for (int d = 0; d < DrumCount; ++d)
        samples.setDrumSample(d, names.at(d).toString());
}

void Instance::clearSample(int target)
{
    const bool had = !samples.drumSample(target).isEmpty();
    samples.setDrumSample(target, QString());
    // Back on its synth: its DECAY as the drum's own (a sample sent there had it at Full).
    if (const int decay = Engine::decayParam(target); had && decay >= 0 && target != int(DrumId::Sm))
        engine.setParamFromHost(decay, kParams[std::size_t(decay)].defaultValue);
}

} // namespace Kessel
