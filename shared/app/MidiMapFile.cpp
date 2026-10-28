#include "app/MidiMapFile.h"

#include <QJsonArray>
#include <QJsonObject>

namespace krate::files {

namespace {

QString keyString(const ParamDescriptor &param)
{
    return QString::fromLatin1(param.key.data(), qsizetype(param.key.size()));
}

} // namespace

LoadResult loadMidiMap(const QString &path, std::span<const ParamDescriptor> params, MidiMap *map)
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
    }
    return LoadResult::Loaded;
}

bool saveMidiMap(const QString &path, std::span<const ParamDescriptor> params, const MidiMap &map)
{
    QJsonArray mappings;
    for (int cc = 0; cc < 128; ++cc) {
        const int param = map.ccToParam[std::size_t(cc)];
        if (param >= 0 && param < int(params.size()))
            mappings.append(QJsonObject{
                {QStringLiteral("cc"), cc},
                {QStringLiteral("param"), keyString(params[std::size_t(param)])},
            });
    }
    return writeJsonFile(path, {
        {QStringLiteral("version"), kMidiMapFileVersion},
        {QStringLiteral("inputPortMatch"), map.inputPortMatch},
        {QStringLiteral("mappings"), mappings},
    });
}

} // namespace krate::files
