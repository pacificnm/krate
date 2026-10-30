#include "k909/app/KitFile.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include <QJsonArray>

#include "k909/dsp/Params.h"

namespace k909::persistence {

namespace {

using Settings = std::initializer_list<std::pair<const char *, double>>;

constexpr double kGateOn = 1.0;
constexpr double delay(int position) { return double(position) / double(kDelayTimes - 1); } // 0 = 1/16 .. 6 = 1/2

Kit make(const char *name, Settings settings)
{
    Kit kit = initKit();
    kit.name = QString::fromUtf8(name);
    for (const auto &[key, value] : settings)
        kit.values.insert(QString::fromUtf8(key), value);
    return kit;
}

} // namespace

bool inKit(int param)
{
    const auto id = ParamId(param);
    return param >= 0 && param < ParamCount && id != ParamId::Volume && id != ParamId::Tempo && id != ParamId::Shuffle;
}

Kit initKit()
{
    Kit kit;
    kit.name = QStringLiteral("Init");
    for (int i = 0; i < ParamCount; ++i) {
        if (!inKit(i))
            continue;
        const auto &d = kParams[std::size_t(i)];
        kit.values.insert(QString::fromUtf8(d.key.data(), qsizetype(d.key.size())), d.defaultValue);
    }
    return kit;
}

// The factory kits (docs/KITS.md#factory-kits). Classic is every default: the
// original kit, exactly.
Kit factoryKit(int index)
{
    switch (index) {
    case 0: return make("Classic", {});
    case 1: // hard, distorted and metallic: a driven kick, a cracking gated snare, metal and clank up front
        return make("Industrial", {
            {"bd_tune", 0.35}, {"bd_decay", 0.45}, {"bd_attack", 0.8}, {"bd_sweep", 0.6}, {"bd_bend", 0.4},
            {"bd_tone", 0.8}, {"bd_drive", 0.55},
            {"sd_tune", 0.4}, {"sd_tone", 0.7}, {"sd_snappy", 0.7}, {"sd_crack", 0.6}, {"sd_drive", 0.45},
            {"sd_rev", 0.35}, {"rev_gate", kGateOn}, {"rev_size", 0.4},
            {"cp_spread", 0.35}, {"cp_drive", 0.3}, {"cp_rev", 0.2},
            {"ch_decay", 0.25}, {"hh_tune", 0.4}, {"ch_drive", 0.4},
            {"me_tune", 0.45}, {"me_decay", 0.45}, {"me_material", 0.2}, {"me_drive", 0.3}, {"me_rev", 0.25},
            {"ck_hard", 0.7}, {"ck_decay", 0.35}, {"ck_drive", 0.3},
            {"nz_tone", 0.55}, {"nz_sweep", 0.6}, {"nz_decay", 0.35}, {"nz_drive", 0.5},
            {"bm_decay", 0.6}, {"bm_drive", 0.4},
            {"comp", 0.5},
        });
    case 2: // tight and driving: short punchy kick, snappy snare, ticking hats, noise on the delay
        return make("EBM", {
            {"bd_tune", 0.45}, {"bd_decay", 0.3}, {"bd_attack", 0.7}, {"bd_sweep", 0.45}, {"bd_bend", 0.3},
            {"bd_tone", 0.6}, {"bd_drive", 0.3},
            {"sd_tune", 0.55}, {"sd_tone", 0.6}, {"sd_snappy", 0.6}, {"sd_decay", 0.35}, {"sd_crack", 0.3},
            {"sd_rev", 0.2}, {"rev_size", 0.3},
            {"cp_level", 0.8}, {"cp_spread", 0.3},
            {"ch_decay", 0.2}, {"oh_decay", 0.25},
            {"nz_tone", 0.7}, {"nz_sweep", 0.3}, {"nz_decay", 0.2}, {"nz_dly", 0.3},
            {"dly_time", delay(2)}, {"dly_fb", 0.25},
            {"comp", 0.35},
        });
    case 3: // the big chopped snare and toms of the 80s: everything through the gated reverb
        return make("Gated 80s", {
            {"sd_tune", 0.45}, {"sd_decay", 0.6}, {"sd_snappy", 0.6}, {"sd_crack", 0.2}, {"sd_rev", 0.7},
            {"lt_decay", 0.6}, {"mt_decay", 0.6}, {"ht_decay", 0.6},
            {"lt_rev", 0.5}, {"mt_rev", 0.5}, {"ht_rev", 0.5},
            {"bd_rev", 0.15}, {"cp_rev", 0.5},
            {"rev_gate", kGateOn}, {"rev_size", 0.55},
            {"comp", 0.3},
        });
    case 4: // lo-fi and squashed: driven voices into the crusher and a pumping compressor
        return make("Crushed", {
            {"bd_tone", 0.9}, {"bd_drive", 0.6}, {"sd_drive", 0.6}, {"cp_drive", 0.4},
            {"ch_drive", 0.5}, {"oh_drive", 0.5}, {"rs_drive", 0.4},
            {"me_drive", 0.4}, {"nz_drive", 0.5},
            {"crush", 0.45}, {"comp", 0.6},
        });
    case 5: // metal percussion: struck plates, hammered steel, hissing stabs, a deep boom
        return make("Machine Shop", {
            {"me_tune", 0.6}, {"me_decay", 0.55}, {"me_material", 0.7}, {"me_rev", 0.3}, {"me_dly", 0.25},
            {"ck_tune", 0.4}, {"ck_hard", 0.85}, {"ck_dly", 0.2},
            {"nz_tone", 0.6}, {"nz_sweep", 0.8},
            {"bm_decay", 0.7},
            {"bd_tune", 0.3}, {"bd_drive", 0.4},
            {"sd_crack", 0.8}, {"sd_snappy", 0.4}, {"sd_tone", 0.8},
            {"rs_tune", 0.7}, {"rs_decay", 0.3},
            {"dly_time", delay(1)}, {"dly_fb", 0.4}, {"rev_size", 0.6},
            {"comp", 0.3},
        });
    default: return initKit();
    }
}

KitBank defaultKitBank()
{
    KitBank bank;
    for (int i = 0; i < kKitCount; ++i)
        bank.kits[std::size_t(i)] = factoryKit(i);
    return bank;
}

QJsonObject kitsToJson(const KitBank &bank)
{
    QJsonArray kits;
    for (const Kit &kit : bank.kits) {
        QJsonObject values;
        for (auto it = kit.values.begin(); it != kit.values.end(); ++it)
            values.insert(it.key(), it.value());
        kits.append(QJsonObject{{QStringLiteral("name"), kit.name}, {QStringLiteral("params"), values}});
    }
    return {
        {QStringLiteral("version"), kKitFileVersion},
        {QStringLiteral("current"), bank.current},
        {QStringLiteral("kits"), kits},
    };
}

KitBank kitsFromJson(const QJsonObject &json)
{
    KitBank bank = defaultKitBank();
    bank.current = std::clamp(json.value(u"current").toInt(0), 0, kKitCount - 1);
    const QJsonArray kits = json.value(u"kits").toArray();
    const Kit init = initKit();
    for (int i = 0; i < kKitCount && i < kits.size(); ++i) {
        const QJsonObject ko = kits.at(i).toObject();
        Kit &kit = bank.kits[std::size_t(i)];
        kit = init;
        kit.name = ko.value(u"name").toString(init.name).left(16);
        const QJsonObject values = ko.value(u"params").toObject();
        for (auto it = kit.values.begin(); it != kit.values.end(); ++it)
            if (const QJsonValue v = values.value(it.key()); v.isDouble())
                it.value() = std::clamp(v.toDouble(), 0.0, 1.0);
    }
    return bank;
}

LoadResult loadKits(const QString &path, KitBank *bank)
{
    *bank = defaultKitBank();
    QJsonObject json;
    const LoadResult result = krate::files::readVersionedJson(path, kKitFileVersion, &json);
    if (result == LoadResult::Loaded)
        *bank = kitsFromJson(json);
    return result;
}

bool saveKits(const QString &path, const KitBank &bank)
{
    return krate::files::writeJsonFile(path, kitsToJson(bank));
}

} // namespace k909::persistence
