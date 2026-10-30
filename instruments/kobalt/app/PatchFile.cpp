#include "kobalt/app/PatchFile.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include <QJsonArray>

#include "kobalt/dsp/Params.h"

namespace kobalt::persistence {

namespace {

using Settings = std::initializer_list<std::pair<const char *, double>>;

// Detent position helpers for readable factory patches.
constexpr double at(int index, int count) { return double(index) / double(count - 1); }
namespace p = kobalt::positions;
constexpr double k16 = at(0, p::kVcoRange), k4 = at(2, p::kVcoRange);
constexpr double kLfoPwm = at(0, p::kPwmSrc), kEnvPwm = at(2, p::kPwmSrc);
constexpr double kSub2 = at(1, p::kSubType), kSub2Pulse = at(2, p::kSubType);
constexpr double kSquareLfo = at(1, p::kLfoWave), kRandomLfo = at(2, p::kLfoWave), kTriangleLfo = at(0, p::kLfoWave);
constexpr double kGateOnly = at(1, p::kEnvTrig), kLfoTrig = at(2, p::kEnvTrig);
constexpr double kPortaAuto = at(1, p::kPortaMode), kPortaOn = at(2, p::kPortaMode);
constexpr double kVcaGate = 1.0;

Patch make(const char *name, Settings settings)
{
    Patch patch = initPatch();
    patch.name = QString::fromUtf8(name);
    for (const auto &[key, value] : settings)
        patch.values.insert(QString::fromUtf8(key), value);
    return patch;
}

} // namespace

Patch initPatch()
{
    Patch patch;
    patch.name = QStringLiteral("Init");
    for (int i = 0; i < ParamCount; ++i) {
        if (!inPatch(ParamId(i)))
            continue;
        const auto &d = kParams[std::size_t(i)];
        patch.values.insert(QString::fromUtf8(d.key.data(), qsizetype(d.key.size())), d.defaultValue);
    }
    return patch;
}

Patch factoryPatch(int index)
{
    switch (index) {
    case 0: return make("Sub Bass", {{"vco_range", k16}, {"saw_level", 0.5}, {"sub_level", 0.9}, {"cutoff", 0.35},
                                     {"res", 0.2}, {"vcf_env", 0.3}, {"env_d", 0.4}, {"env_s", 0.3}, {"env_r", 0.2}});
    case 1: return make("Glide Lead", {{"saw_level", 0.7}, {"pulse_level", 0.5}, {"cutoff", 0.6}, {"res", 0.35},
                                       {"vcf_env", 0.25}, {"env_s", 0.7}, {"porta_mode", kPortaAuto},
                                       {"porta_time", 0.45}, {"env_trig", kGateOnly}, {"delay_level", 0.3}});
    case 2: return make("PWM Lead", {{"saw_level", 0.0}, {"pulse_level", 0.9}, {"pw", 0.7}, {"pwm_src", kLfoPwm},
                                     {"lfo_rate", 0.4}, {"cutoff", 0.65}, {"vcf_env", 0.15}, {"env_a", 0.3},
                                     {"env_s", 0.8}, {"env_r", 0.45}, {"delay_level", 0.4}, {"delay_time", at(3, p::kDelayTime)},
                                     {"delay_fb", 0.45}, {"reverb_level", 0.2}});
    case 3: return make("Squelch Bass", {{"vco_range", k16}, {"cutoff", 0.25}, {"res", 0.75}, {"vcf_env", 0.55},
                                         {"env_d", 0.35}, {"env_s", 0.0}, {"env_r", 0.2}, {"porta_mode", kPortaAuto},
                                         {"porta_time", 0.3}, {"env_trig", kGateOnly}});
    case 4: return make("Pluck", {{"pulse_level", 0.6}, {"saw_level", 0.4}, {"cutoff", 0.35}, {"vcf_env", 0.5},
                                  {"env_d", 0.35}, {"env_s", 0.0}, {"env_r", 0.3}, {"reverb_level", 0.15}});
    case 5: return make("LFO Gate", {{"saw_level", 0.6}, {"sub_level", 0.5}, {"cutoff", 0.3}, {"res", 0.5},
                                     {"vcf_env", 0.5}, {"env_d", 0.3}, {"env_s", 0.0}, {"env_trig", kLfoTrig},
                                     {"lfo_wave", kSquareLfo}, {"lfo_rate", 0.55}});
    case 6: return make("Noise Hat", {{"saw_level", 0.0}, {"noise_level", 1.0}, {"cutoff", 0.85}, {"res", 0.2},
                                      {"vcf_env", 0.0}, {"key_follow", 0.0}, {"env_d", 0.25}, {"env_s", 0.0},
                                      {"env_r", 0.2}});
    case 7: return make("Zap Tom", {{"saw_level", 0.0}, {"noise_level", 0.05}, {"cutoff", 0.2}, {"res", 1.0},
                                    {"vcf_env", 0.6}, {"key_follow", 1.0}, {"env_d", 0.4}, {"env_s", 0.0},
                                    {"env_r", 0.35}});
    case 8: return make("Wobble Bass", {{"vco_range", k16}, {"saw_level", 0.8}, {"sub_level", 0.6}, {"cutoff", 0.3},
                                        {"res", 0.55}, {"vcf_env", 0.1}, {"vcf_mod", 0.4}, {"lfo_wave", kTriangleLfo},
                                        {"lfo_rate", 0.45}, {"env_s", 1.0}});
    case 9: return make("Siren", {{"saw_level", 0.8}, {"vco_mod", 0.6}, {"lfo_rate", 0.35}, {"cutoff", 0.7},
                                  {"vcf_env", 0.0}, {"env_s", 1.0}, {"reverb_level", 0.25}});
    case 10: return make("Hollow Square", {{"saw_level", 0.0}, {"pulse_level", 0.9}, {"sub_level", 0.4},
                                           {"sub_type", kSub2}, {"cutoff", 0.55}, {"vcf_env", 0.15}, {"env_s", 0.8}});
    case 11: return make("Arp Blip", {{"saw_level", 0.0}, {"pulse_level", 0.7}, {"pw", 0.5}, {"cutoff", 0.4},
                                      {"vcf_env", 0.45}, {"env_d", 0.3}, {"env_s", 0.0}, {"play_mode", at(1, p::kPlayMode)},
                                      {"arp_mode", at(1, p::kArpMode)}, {"arp_range", at(1, p::kArpRange)},
                                      {"delay_level", 0.3}, {"delay_fb", 0.4}});
    case 12: return make("Random Bleeps", {{"saw_level", 0.7}, {"lfo_wave", kRandomLfo}, {"lfo_rate", 0.6},
                                           {"vcf_mod", 0.5}, {"res", 0.6}, {"cutoff", 0.45}, {"vcf_env", 0.2},
                                           {"env_trig", kLfoTrig}, {"env_d", 0.3}, {"env_s", 0.0},
                                           {"delay_level", 0.35}});
    case 13: return make("Gate Organ", {{"saw_level", 0.0}, {"pulse_level", 0.8}, {"sub_level", 0.6},
                                        {"sub_type", kSub2Pulse}, {"vco_range", k4}, {"vca_mode", kVcaGate},
                                        {"cutoff", 0.7}, {"vcf_env", 0.0}, {"reverb_level", 0.2}});
    case 14: return make("Brass Lead", {{"saw_level", 0.9}, {"cutoff", 0.4}, {"res", 0.15}, {"vcf_env", 0.4},
                                        {"env_a", 0.3}, {"env_d", 0.5}, {"env_s", 0.6}, {"porta_mode", kPortaOn},
                                        {"porta_time", 0.2}});
    case 15: return make("Bend Lead", {{"saw_level", 0.6}, {"pulse_level", 0.6}, {"pw", 0.3}, {"pwm_src", kEnvPwm},
                                       {"cutoff", 0.6}, {"bend_vco", 1.0}, {"bend_vcf", 0.3}, {"lever_mod", 0.7},
                                       {"env_s", 0.8}, {"delay_level", 0.35}, {"reverb_level", 0.25}});
    default: return initPatch();
    }
}

PatchBank defaultBank()
{
    PatchBank bank;
    for (int i = 0; i < kPatchCount; ++i)
        bank.patches[std::size_t(i)] = factoryPatch(i);
    return bank;
}

QJsonObject toJson(const PatchBank &bank)
{
    QJsonArray patches;
    for (const Patch &patch : bank.patches) {
        QJsonObject values;
        for (auto it = patch.values.begin(); it != patch.values.end(); ++it)
            values.insert(it.key(), it.value());
        patches.append(QJsonObject{{QStringLiteral("name"), patch.name}, {QStringLiteral("params"), values}});
    }
    return {
        {QStringLiteral("version"), kPatchFileVersion},
        {QStringLiteral("current"), bank.current},
        {QStringLiteral("manual"), bank.manual},
        {QStringLiteral("patches"), patches},
    };
}

PatchBank fromJson(const QJsonObject &json)
{
    PatchBank bank;
    const Patch init = initPatch();
    bank.patches.fill(init);
    bank.current = std::clamp(json.value(u"current").toInt(0), 0, kPatchCount - 1);
    bank.manual = json.value(u"manual").toBool(false);
    const QJsonArray patches = json.value(u"patches").toArray();
    for (int i = 0; i < kPatchCount && i < patches.size(); ++i) {
        const QJsonObject po = patches.at(i).toObject();
        Patch &patch = bank.patches[std::size_t(i)];
        patch.name = po.value(u"name").toString(init.name).left(16);
        const QJsonObject values = po.value(u"params").toObject();
        for (auto it = patch.values.begin(); it != patch.values.end(); ++it)
            if (const QJsonValue v = values.value(it.key()); v.isDouble())
                it.value() = std::clamp(v.toDouble(), 0.0, 1.0);
    }
    return bank;
}

LoadResult loadPatches(const QString &path, PatchBank *bank)
{
    *bank = defaultBank();
    QJsonObject json;
    const LoadResult result = krate::files::readVersionedJson(path, kPatchFileVersion, &json);
    if (result == LoadResult::Loaded)
        *bank = fromJson(json);
    return result;
}

bool savePatches(const QString &path, const PatchBank &bank)
{
    return krate::files::writeJsonFile(path, toJson(bank));
}

} // namespace kobalt::persistence
