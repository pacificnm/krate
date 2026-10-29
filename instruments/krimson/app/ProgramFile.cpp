#include "krimson/app/ProgramFile.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include <QJsonArray>

namespace krimson::persistence {

namespace {

using Settings = std::initializer_list<std::pair<const char *, double>>;
namespace p = krimson::positions;

constexpr double at(int index, int count) { return double(index) / double(count - 1); }
constexpr double semi(int semitones) { return 0.5 + semitones / 48.0; }
// Waves and lists by position.
constexpr double kSine1 = at(0, p::kOsc1Wave), kTri1 = at(1, p::kOsc1Wave), kSaw1 = at(2, p::kOsc1Wave),
                 kPulse1 = at(3, p::kOsc1Wave);
constexpr double kTri2 = at(0, p::kOsc2Wave), kSaw2 = at(1, p::kOsc2Wave), kPulse2 = at(2, p::kOsc2Wave),
                 kNoise2 = at(3, p::kOsc2Wave);
constexpr double kLp12 = at(0, p::kFilterType), kLp24 = at(1, p::kFilterType), kHp = at(3, p::kFilterType),
                 kNotch = at(4, p::kFilterType);
constexpr double kMono = at(1, p::kVoiceMode), kLegato = at(2, p::kVoiceMode), kUnison = at(3, p::kVoiceMode);
constexpr double kMenvFm = at(1, p::kMenvDest), kMenvPw = at(2, p::kMenvDest);
constexpr double kLfoFilter = at(2, p::kLfo1Dest), kLfoPw = at(3, p::kLfo1Dest);
constexpr double kChorus = at(1, p::kChorus), kEnsemble = at(2, p::kChorus);

QString keyOf(int index)
{
    const auto k = kParams[std::size_t(index)].key;
    return QString::fromUtf8(k.data(), qsizetype(k.size()));
}

// A value as stored: stepped controls snapped through float, as loading does.
double stored(const QString &key, double value)
{
    for (const auto &d : kParams)
        if (key == QLatin1StringView(d.key.data(), qsizetype(d.key.size())))
            return d.detents > 1 ? double(krate::snapToDetent(d, float(value))) : value;
    return value;
}

Program make(const char *name, Settings settings, Settings morph = {})
{
    Program program = initProgram();
    program.name = QString::fromUtf8(name);
    for (const auto &[key, value] : settings)
        program.values.insert(QString::fromUtf8(key), stored(QString::fromUtf8(key), value));
    for (const auto &[key, value] : morph)
        program.morph.insert(QString::fromUtf8(key), value); // continuous controls only (tst_programs checks)
    return program;
}

QJsonObject hashToJson(const QHash<QString, qreal> &hash)
{
    QJsonObject o;
    for (auto it = hash.begin(); it != hash.end(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

} // namespace

Program initProgram()
{
    Program program;
    program.name = QStringLiteral("Init");
    for (int i = 0; i < SlotParamCount; ++i)
        program.values.insert(keyOf(i), kParams[std::size_t(i)].defaultValue);
    return program;
}

Program factoryProgram(int index)
{
    switch (index) {
    case 0: return make("Sync Lead", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"sync", 1}, {"osc_mix", 1},
                                      {"osc2_semi", semi(7)}, {"menv_amt", 0.85}, {"menv_d", 0.5}, {"cutoff", 0.85},
                                      {"fenv_amt", 0.1}, {"voice_mode", kLegato}, {"glide", 0.25}, {"delay_level", 0.3},
                                      {"delay_fb", 0.35}},
                        {{"menv_amt", 1.0}, {"cutoff", 1.0}});
    case 1: return make("FM Bells", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(19)}, {"osc_mix", 0},
                                     {"fm", 0.35}, {"menv_dest", kMenvFm}, {"menv_amt", 0.8}, {"menv_d", 0.55},
                                     {"cutoff", 0.9}, {"fenv_amt", 0}, {"aenv_d", 0.65}, {"aenv_s", 0}, {"aenv_r", 0.6},
                                     {"reverb_level", 0.4}, {"reverb_size", 0.6}},
                        {{"fm", 0.7}});
    case 2: return make("Ring Clang", {{"osc1_wave", kPulse1}, {"osc2_wave", kPulse2}, {"ring", 1}, {"osc_mix", 1},
                                       {"osc2_semi", semi(11)}, {"cutoff", 0.8}, {"aenv_d", 0.55}, {"aenv_s", 0},
                                       {"aenv_r", 0.5}, {"reverb_level", 0.25}});
    case 3: return make("Acid 12", {{"osc1_wave", kSaw1}, {"osc_mix", 0}, {"cutoff", 0.3}, {"res", 0.8}, {"fenv_amt", 0.5},
                                    {"fenv_d", 0.4}, {"fenv_s", 0}, {"drive", 0.3}, {"aenv_r", 0.2}, {"voice_mode", kMono},
                                    {"glide", 0.15}},
                        {{"cutoff", 0.55}, {"res", 0.9}});
    case 4: return make("Unison Lead", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.56},
                                        {"voice_mode", kUnison}, {"unison_detune", 0.6}, {"cutoff", 0.7}, {"res", 0.2},
                                        {"fenv_amt", 0.2}, {"chorus", kChorus}, {"delay_level", 0.25}});
    case 5: return make("Saw Bass", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(-12)},
                                     {"filter_type", kLp24}, {"cutoff", 0.35}, {"res", 0.25}, {"fenv_amt", 0.35},
                                     {"fenv_d", 0.45}, {"fenv_s", 0.1}, {"voice_mode", kMono}, {"glide", 0.12},
                                     {"aenv_r", 0.15}});
    case 6: return make("Morph Pad", {{"osc1_wave", kPulse1}, {"osc2_wave", kSaw2}, {"pw", 0.4}, {"osc2_fine", 0.54},
                                      {"lfo1_dest", kLfoPw}, {"lfo1_amt", 0.3}, {"lfo1_rate", 0.35}, {"cutoff", 0.35},
                                      {"fenv_amt", 0.05}, {"aenv_a", 0.6}, {"aenv_r", 0.65}, {"chorus", kEnsemble},
                                      {"reverb_level", 0.35}},
                        {{"cutoff", 0.8}, {"res", 0.5}, {"lfo1_rate", 0.6}});
    case 7: return make("Pluck", {{"osc1_wave", kPulse1}, {"osc2_wave", kSaw2}, {"pw", 0.3}, {"cutoff", 0.3},
                                  {"fenv_amt", 0.5}, {"fenv_d", 0.4}, {"fenv_s", 0}, {"aenv_d", 0.5}, {"aenv_s", 0},
                                  {"aenv_r", 0.4}, {"delay_level", 0.3}, {"delay_fb", 0.3}});
    case 8: return make("Strings", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.55}, {"cutoff", 0.6},
                                    {"fenv_amt", 0.05}, {"aenv_a", 0.55}, {"aenv_r", 0.6}, {"lfo1_amt", 0.1},
                                    {"lfo1_rate", 0.55}, {"chorus", kEnsemble}, {"reverb_level", 0.3}});
    case 9: return make("Brass", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.56}, {"cutoff", 0.4},
                                  {"res", 0.15}, {"fenv_amt", 0.4}, {"fenv_a", 0.35}, {"fenv_d", 0.5}, {"fenv_s", 0.4},
                                  {"aenv_a", 0.3}, {"aenv_r", 0.4}},
                        {{"fenv_a", 0.15}, {"cutoff", 0.55}});
    case 10: return make("Notch Sweep", {{"osc1_wave", kPulse1}, {"osc2_wave", kSaw2}, {"pw", 0.5}, {"osc2_fine", 0.55},
                                         {"filter_type", kNotch}, {"res", 0.6}, {"cutoff", 0.5}, {"lfo1_dest", kLfoFilter},
                                         {"lfo1_amt", 0.5}, {"lfo1_rate", 0.3}, {"fenv_amt", 0}, {"aenv_a", 0.5},
                                         {"aenv_r", 0.6}, {"reverb_level", 0.3}});
    case 11: return make("Rave Stab", {{"osc1_wave", kPulse1}, {"osc2_wave", kPulse2}, {"pw", 0.6}, {"menv_dest", kMenvPw},
                                       {"menv_amt", 0.2}, {"menv_d", 0.5}, {"voice_mode", kUnison}, {"unison_detune", 0.8},
                                       {"glide", 0.2}, {"cutoff", 0.75}, {"fenv_amt", 0.2}, {"aenv_d", 0.55},
                                       {"aenv_s", 0.3}});
    case 12: return make("Sub Bass", {{"osc1_wave", kTri1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(-12)}, {"cutoff", 0.4},
                                      {"fenv_amt", 0.05}, {"voice_mode", kMono}, {"aenv_r", 0.2}});
    case 13: return make("Perc Organ", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(12)},
                                        {"cutoff", 0.75}, {"fenv_amt", 0}, {"aenv_a", 0}, {"aenv_d", 0.45}, {"aenv_s", 0.7},
                                        {"aenv_r", 0.2}, {"chorus", kChorus}});
    case 14: return make("Arp Pluck", {{"osc1_wave", kSaw1}, {"osc2_wave", kPulse2}, {"cutoff", 0.35}, {"fenv_amt", 0.45},
                                       {"fenv_d", 0.38}, {"fenv_s", 0}, {"aenv_d", 0.45}, {"aenv_s", 0}, {"aenv_r", 0.35},
                                       {"arp_on", 1}, {"arp_mode", at(2, p::kArpMode)}, {"arp_range", at(1, p::kArpRange)},
                                       {"delay_level", 0.3}, {"delay_fb", 0.4}});
    case 15: return make("Noise Sweep", {{"osc2_wave", kNoise2}, {"osc_mix", 1}, {"filter_type", kHp}, {"res", 0.7},
                                         {"cutoff", 0.2}, {"fenv_amt", 0.6}, {"fenv_a", 0.6}, {"fenv_d", 0.7}, {"fenv_s", 0.3},
                                         {"aenv_a", 0.5}, {"aenv_r", 0.6}, {"reverb_level", 0.4}});
    default: return initProgram();
    }
}

ProgramBank defaultBank()
{
    ProgramBank bank;
    for (int i = 0; i < kProgramCount; ++i)
        bank.programs[std::size_t(i)] = factoryProgram(i);
    for (int slot = 0; slot < SlotCount; ++slot)
        bank.current[std::size_t(slot)] = slot;
    return bank;
}

QJsonObject toJson(const ProgramBank &bank)
{
    QJsonArray programs, current;
    for (const Program &p : bank.programs)
        programs.append(QJsonObject{{QStringLiteral("name"), p.name}, {QStringLiteral("params"), hashToJson(p.values)},
                                    {QStringLiteral("morph"), hashToJson(p.morph)}});
    for (int c : bank.current)
        current.append(c);
    return {{QStringLiteral("version"), kFileVersion}, {QStringLiteral("current"), current},
            {QStringLiteral("programs"), programs}};
}

ProgramBank programsFromJson(const QJsonObject &json)
{
    ProgramBank bank;
    const Program init = initProgram();
    bank.programs.fill(init);
    const QJsonArray current = json.value(u"current").toArray();
    for (int slot = 0; slot < SlotCount; ++slot)
        bank.current[std::size_t(slot)] = std::clamp(current.at(slot).toInt(slot), 0, kProgramCount - 1);
    const QJsonArray programs = json.value(u"programs").toArray();
    for (int i = 0; i < kProgramCount && i < programs.size(); ++i) {
        const QJsonObject po = programs.at(i).toObject();
        Program &p = bank.programs[std::size_t(i)];
        p.name = po.value(u"name").toString(init.name).left(16);
        const QJsonObject params = po.value(u"params").toObject();
        for (int k = 0; k < SlotParamCount; ++k)
            if (const QJsonValue v = params.value(keyOf(k)); v.isDouble()) {
                const double value = std::clamp(v.toDouble(), 0.0, 1.0);
                const auto &d = kParams[std::size_t(k)];
                p.values[keyOf(k)] = d.detents > 1 ? double(krate::snapToDetent(d, float(value))) : value;
            }
        const QJsonObject morph = po.value(u"morph").toObject();
        for (int k = 0; k < SlotParamCount; ++k)
            if (const QJsonValue v = morph.value(keyOf(k)); v.isDouble() && kParams[std::size_t(k)].detents == 0)
                p.morph[keyOf(k)] = std::clamp(v.toDouble(), 0.0, 1.0);
    }
    return bank;
}

LoadResult loadPrograms(const QString &path, ProgramBank *bank)
{
    *bank = defaultBank();
    QJsonObject json;
    const LoadResult result = krate::files::readVersionedJson(path, kFileVersion, &json);
    if (result == LoadResult::Loaded)
        *bank = programsFromJson(json);
    return result;
}

bool savePrograms(const QString &path, const ProgramBank &bank)
{
    return krate::files::writeJsonFile(path, toJson(bank));
}

QJsonObject toJson(const PerformanceBank &bank)
{
    QJsonArray performances;
    for (const Performance &p : bank.performances)
        performances.append(QJsonObject{{QStringLiteral("name"), p.name}, {QStringLiteral("state"), p.state}});
    return {{QStringLiteral("version"), kFileVersion}, {QStringLiteral("current"), bank.current},
            {QStringLiteral("performances"), performances}};
}

PerformanceBank performancesFromJson(const QJsonObject &json)
{
    PerformanceBank bank;
    bank.current = std::clamp(json.value(u"current").toInt(0), 0, kPerformanceCount - 1);
    const QJsonArray performances = json.value(u"performances").toArray();
    for (int i = 0; i < kPerformanceCount && i < performances.size(); ++i) {
        const QJsonObject po = performances.at(i).toObject();
        bank.performances[std::size_t(i)] = {po.value(u"name").toString().left(16), po.value(u"state").toObject()};
    }
    return bank;
}

LoadResult loadPerformances(const QString &path, PerformanceBank *bank)
{
    *bank = PerformanceBank{};
    QJsonObject json;
    const LoadResult result = krate::files::readVersionedJson(path, kFileVersion, &json);
    if (result == LoadResult::Loaded)
        *bank = performancesFromJson(json);
    return result;
}

bool savePerformances(const QString &path, const PerformanceBank &bank)
{
    return krate::files::writeJsonFile(path, toJson(bank));
}

} // namespace krimson::persistence
