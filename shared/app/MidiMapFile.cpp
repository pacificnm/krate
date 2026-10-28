#include "app/MidiMapFile.h"

#include <QJsonArray>
#include <QJsonObject>

#include "core/Midi.h"

namespace krate::files {

namespace {

QString keyString(const ParamDescriptor &param)
{
    return QString::fromLatin1(param.key.data(), qsizetype(param.key.size()));
}

} // namespace

LoadResult loadMidiMap(const QString &path, std::span<const ParamDescriptor> params, MidiMap *map,
                       const QStringList &actions)
{
    *map = MidiMap{};
    QJsonObject json;
    const LoadResult result = readVersionedJson(path, kMidiMapFileVersion, &json);
    if (result != LoadResult::Loaded)
        return result;

    map->inputPortMatch = json.value(u"inputPortMatch").toString(map->inputPortMatch);
    for (const auto &entry : json.value(u"mappings").toArray()) {
        const QJsonObject o = entry.toObject();
        const int cc = o.value(u"cc").toInt(-1);
        const QString key = o.value(u"param").toString();
        if (cc < 0 || cc >= 128)
            continue;
        for (std::size_t i = 0; i < params.size(); ++i) {
            if (keyString(params[i]) == key) {
                map->ccToParam[std::size_t(cc)] = int(i);
                break;
            }
        }
        if (const qsizetype action = actions.indexOf(key); action >= 0)
            map->ccToParam[std::size_t(cc)] = MidiLearnMap::actionTarget(int(action));
    }
    return LoadResult::Loaded;
}

bool saveMidiMap(const QString &path, std::span<const ParamDescriptor> params, const MidiMap &map,
                 const QStringList &actions)
{
    QJsonArray mappings;
    for (int cc = 0; cc < 128; ++cc) {
        const int target = map.ccToParam[std::size_t(cc)];
        QString key;
        if (target >= 0 && target < int(params.size()))
            key = keyString(params[std::size_t(target)]);
        else if (MidiLearnMap::isAction(target) && MidiLearnMap::actionIndex(target) < actions.size())
            key = actions.at(MidiLearnMap::actionIndex(target));
        if (!key.isEmpty())
            mappings.append(QJsonObject{{QStringLiteral("cc"), cc}, {QStringLiteral("param"), key}});
    }
    return writeJsonFile(path, {
        {QStringLiteral("version"), kMidiMapFileVersion},
        {QStringLiteral("inputPortMatch"), map.inputPortMatch},
        {QStringLiteral("mappings"), mappings},
    });
}

} // namespace krate::files
