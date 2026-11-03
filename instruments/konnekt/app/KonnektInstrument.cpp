#include "konnekt/app/KonnektInstrument.h"

#include <QDir>

namespace konnekt {

namespace {

// Its patch, in ~/.local/share/<id> (made first).
QString madeDir(const QString &dir)
{
    QDir().mkpath(dir);
    return dir;
}

} // namespace

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, madeDir(context.dataDir), nullptr)
{
}

QJsonObject Instance::saveSound()
{
    QJsonObject sound = InstrumentInstance::saveSound();
    sound.insert(QStringLiteral("patch"), controller.toJson());
    return sound;
}

void Instance::loadSound(const QJsonObject &sound)
{
    InstrumentInstance::loadSound(sound);
    if (const QJsonObject patch = sound.value(u"patch").toObject(); !patch.isEmpty() && patch != controller.toJson() && controller.fromJson(patch))
        controller.save(); // (and kept as the patch it starts with)
}

krate::InstrumentEntry registryEntry()
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("konnekt");
    entry.maxCopies = 16; // the Krate rack: up to 16 of it
    entry.name = QStringLiteral("Konnekt");
    // None of its own: the keys play it when it has focus.
    entry.defaultMidiChannel = -1;
    entry.qmlModule = QStringLiteral("Konnekt");
    entry.panelComponent = QStringLiteral("Panel");
    entry.create = [](const krate::InstrumentContext &context) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<Instance>(context);
    };
    return entry;
}

} // namespace konnekt
