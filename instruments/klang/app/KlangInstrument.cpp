#include "klang/app/KlangInstrument.h"

#include <QFileInfo>

namespace klang {

Instance::Instance(const krate::InstrumentContext &context)
    : controller(engine, context.configDir + QStringLiteral("/panel.json"), context.transport, nullptr)
    , pads(controller, engine, context.dataDir, nullptr)
    , patterns(engine, context.dataDir + QStringLiteral("/patterns.json"), nullptr)
{
    if (context.run)
        controller.setRunControl(context.run, context.id.isEmpty() ? QStringLiteral("klang") : context.id);
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
    entry.maxCopies = 16; // the Krate rack: up to 16 of it
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

QStringList Instance::sampleTargets() const
{
    QStringList list;
    for (int p = 0; p < KlangPadController::kPads; ++p)
        list << QStringLiteral("Pad %1").arg(p + 1);
    return list;
}

QVariantList Instance::noteLanes() const
{
    // Pad 1 at the top, each by its sound: "1 · 01 kick" (notes 36..51).
    QVariantList list;
    for (int p = 0; p < KlangPadController::kPads; ++p) {
        const QString sound = pads.name(p);
        list << QVariantMap{{QStringLiteral("name"), QStringLiteral("%1 · %2").arg(p + 1).arg(sound.isEmpty() ? QStringLiteral("empty") : QFileInfo(sound).completeBaseName())},
                            {QStringLiteral("note"), Engine::kFirstPadNote + p}};
    }
    return list;
}

QStringList Instance::sampleTargetContents() const
{
    QStringList list;
    for (int p = 0; p < KlangPadController::kPads; ++p)
        list << pads.name(p);
    return list;
}

bool Instance::assignSample(int target, const QString &path, QString *error)
{
    if (target < 0 || target >= KlangPadController::kPads)
        return false;
    pads.loadUrls(target, {QUrl::fromLocalFile(path)});
    if (pads.message().startsWith(QStringLiteral("Loaded")))
        return true;
    if (error)
        *error = pads.message();
    return false;
}

void Instance::clearSample(int target)
{
    pads.clear(target);
}

} // namespace klang
