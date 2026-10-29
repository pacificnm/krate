#include "kalliope/app/PatchFile.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include <QJsonArray>

#include "kalliope/dsp/Params.h"

namespace kalliope::persistence {

namespace {

using Settings = std::initializer_list<std::pair<const char *, double>>;

// Detent position helpers for readable factory patches.
constexpr double at(int index, int count) { return double(index) / double(count - 1); }
namespace p = kalliope::positions;
constexpr double kTriangle = at(0, p::kVco1Wave), kSaw1 = at(1, p::kVco1Wave), kPulse1 = at(2, p::kVco1Wave),
                 kSquare1 = at(3, p::kVco1Wave);
constexpr double kSine2 = at(0, p::kVco2Wave), kSaw2 = at(1, p::kVco2Wave), kPulse2 = at(2, p::kVco2Wave),
                 kNoise2 = at(3, p::kVco2Wave);
constexpr double k16 = at(0, p::kVco1Range), k4 = at(2, p::kVco1Range);
constexpr double k2Low = at(0, p::kVco2Range), k2_16 = at(1, p::kVco2Range), k2_4 = at(3, p::kVco2Range),
                 k2_2 = at(4, p::kVco2Range);
constexpr double kUnison = at(1, p::kKeyMode), kSolo = at(2, p::kKeyMode);

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
    case 0: return make("Brass", {{"vco1_wave", kSaw1}, {"vco2_wave", kSaw2}, {"vco2_fine", 0.56}, {"cutoff", 0.45},
                                  {"res", 0.15}, {"vcf_env", 0.35}, {"env1_a", 0.35}, {"env1_d", 0.5}, {"env1_s", 0.4},
                                  {"env2_a", 0.25}, {"env2_s", 0.9}, {"env2_r", 0.4}});
    case 1: return make("Strings", {{"vco1_wave", kSaw1}, {"vco2_wave", kSaw2}, {"vco2_fine", 0.54}, {"hpf", 0.3},
                                    {"cutoff", 0.58}, {"vcf_env", 0.05}, {"mod_lfo", 0.02}, {"lfo_rate", 0.6},
                                    {"lfo_delay", 0.25}, {"env2_a", 0.55}, {"env2_r", 0.6}, {"chorus", 1.0}});
    case 2: return make("Soft Pad", {{"vco1_wave", kPulse1}, {"vco2_wave", kPulse2}, {"vco2_fine", 0.53}, {"pwm", 0.7},
                                     {"pwm_src", 0.0}, {"lfo_rate", 0.35}, {"cutoff", 0.5}, {"vcf_env", 0.1},
                                     {"env2_a", 0.6}, {"env2_r", 0.65}, {"chorus", 1.0}});
    case 3: return make("Bass", {{"vco1_wave", kSaw1}, {"vco1_range", k16}, {"vco2_wave", kPulse2}, {"vco2_range", k2_16},
                                 {"mix", 0.4}, {"cutoff", 0.3}, {"res", 0.35}, {"vcf_env", 0.45}, {"env1_d", 0.4},
                                 {"env1_s", 0.0}, {"env2_d", 0.45}, {"env2_s", 0.6}, {"env2_r", 0.2}, {"chorus", 0.0}});
    case 4: return make("Sync Lead", {{"vco1_wave", kSaw1}, {"vco2_wave", kSaw2}, {"sync", 1.0}, {"mix", 1.0},
                                      {"mod_env", 0.6}, {"mod_dest", 1.0}, {"env1_d", 0.55}, {"env1_s", 0.0},
                                      {"cutoff", 0.8}, {"vcf_env", 0.0}, {"key_mode", kSolo}, {"delay_level", 0.3},
                                      {"delay_fb", 0.35}});
    case 5: return make("Bells", {{"vco1_wave", kTriangle}, {"vco2_wave", kSine2}, {"vco2_range", k2_2},
                                  {"vco2_fine", 0.62}, {"cross_mod", 0.45}, {"mix", 0.0}, {"cutoff", 0.85},
                                  {"vcf_env", 0.0}, {"env2_d", 0.6}, {"env2_s", 0.0}, {"env2_r", 0.6},
                                  {"delay_level", 0.35}, {"delay_fb", 0.5}});
    case 6: return make("Unison Lead", {{"vco1_wave", kSaw1}, {"vco2_wave", kSaw2}, {"key_mode", kUnison},
                                        {"cutoff", 0.6}, {"res", 0.2}, {"vcf_env", 0.3}, {"env1_d", 0.5},
                                        {"env1_s", 0.3}, {"delay_level", 0.25}, {"chorus", 0.0}});
    case 7: return make("Sweep Bass", {{"vco1_wave", kSaw1}, {"vco1_range", k16}, {"mix", 0.0}, {"cutoff", 0.2},
                                       {"res", 0.85}, {"vcf_env", 0.7}, {"env1_a", 0.55}, {"env1_d", 0.6},
                                       {"env1_s", 0.2}, {"chorus", 0.0}});
    case 8: return make("Organ", {{"vco1_wave", kSquare1}, {"vco2_wave", kPulse2}, {"vco2_range", k2_4},
                                  {"cutoff", 0.7}, {"vcf_env", 0.0}, {"env2_a", 0.0}, {"env2_d", 0.0}, {"env2_s", 1.0},
                                  {"env2_r", 0.2}, {"chorus", 1.0}});
    case 9: return make("Pluck", {{"vco1_wave", kPulse1}, {"vco2_wave", kSaw2}, {"pwm", 0.5}, {"cutoff", 0.3},
                                  {"vcf_env", 0.5}, {"env1_d", 0.4}, {"env1_s", 0.0}, {"env2_d", 0.5}, {"env2_s", 0.0},
                                  {"env2_r", 0.4}});
    case 10: return make("Tremolo Keys", {{"vco1_wave", kTriangle}, {"vco2_wave", kSine2}, {"vco2_range", k2_4},
                                          {"cutoff", 0.7}, {"vcf_env", 0.1}, {"vca_lfo", 0.5}, {"lfo_rate", 0.5},
                                          {"env2_d", 0.55}, {"env2_s", 0.5}, {"env2_r", 0.45}});
    case 11: return make("Noise Sweep", {{"vco2_wave", kNoise2}, {"mix", 1.0}, {"res", 0.7}, {"cutoff", 0.2},
                                         {"vcf_env", 0.6}, {"env1_a", 0.6}, {"env1_d", 0.7}, {"env1_s", 0.3},
                                         {"env2_a", 0.5}, {"env2_r", 0.6}});
    case 12: return make("Arp Pluck", {{"vco1_wave", kSaw1}, {"vco2_wave", kPulse2}, {"cutoff", 0.35}, {"vcf_env", 0.45},
                                       {"env1_d", 0.38}, {"env1_s", 0.0}, {"env2_d", 0.45}, {"env2_s", 0.0},
                                       {"env2_r", 0.35}, {"arp_on", 1.0}, {"arp_mode", at(2, p::kArpMode)},
                                       {"arp_range", at(1, p::kArpRange)}, {"delay_level", 0.3}, {"delay_fb", 0.4}});
    case 13: return make("Detuned Saws", {{"vco1_wave", kSaw1}, {"vco2_wave", kSaw2}, {"vco2_fine", 0.6},
                                          {"cutoff", 0.65}, {"vcf_env", 0.15}, {"env2_r", 0.45}});
    case 14: return make("Sub Bass", {{"vco1_wave", kTriangle}, {"vco1_range", k16}, {"vco2_wave", kSine2},
                                      {"vco2_range", k2_16}, {"cutoff", 0.35}, {"vcf_env", 0.1}, {"key_mode", kSolo},
                                      {"env2_r", 0.25}, {"chorus", 0.0}});
    case 15: return make("Low Wobble", {{"vco1_wave", kSaw1}, {"vco2_wave", kSaw2}, {"vco2_range", k2Low},
                                        {"cross_mod", 0.3}, {"mix", 0.0}, {"cutoff", 0.4}, {"res", 0.5},
                                        {"vcf_lfo", 0.35}, {"lfo_rate", 0.55}});
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

} // namespace kalliope::persistence
