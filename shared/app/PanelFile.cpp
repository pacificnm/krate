#include "app/PanelFile.h"

#include <algorithm>

#include <QJsonObject>

namespace krate::files {

namespace {

QString keyString(const ParamDescriptor &param)
{
    return QString::fromLatin1(param.key.data(), qsizetype(param.key.size()));
}

} // namespace

PanelValues defaultPanel(std::span<const ParamDescriptor> params)
{
    PanelValues values;
    values.reserve(params.size());
    for (const auto &p : params)
        values.push_back(p.defaultValue);
    return values;
}

LoadResult loadPanel(const QString &path, std::span<const ParamDescriptor> params, PanelValues *values)
{
    *values = defaultPanel(params);
    QJsonObject json;
    const LoadResult result = readVersionedJson(path, kPanelFileVersion, &json);
    if (result != LoadResult::Loaded)
        return result;

    const QJsonObject saved = json.value(u"params").toObject();
    for (std::size_t i = 0; i < params.size(); ++i) {
        const QJsonValue v = saved.value(keyString(params[i]));
        if (v.isDouble())
            (*values)[i] = float(std::clamp(v.toDouble(), 0.0, 1.0));
    }
    return LoadResult::Loaded;
}

bool savePanel(const QString &path, std::span<const ParamDescriptor> params, const PanelValues &values)
{
    QJsonObject saved;
    for (std::size_t i = 0; i < params.size() && i < values.size(); ++i)
        saved.insert(keyString(params[i]), double(values[i]));
    return writeJsonFile(path, {{QStringLiteral("version"), kPanelFileVersion}, {QStringLiteral("params"), saved}});
}

} // namespace krate::files
