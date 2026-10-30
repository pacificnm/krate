#include "krimson/app/ProgramFile.h"

#include <algorithm>
#include <cmath>
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
constexpr double kLp12 = at(0, p::kFilterType), kLp24 = at(1, p::kFilterType), kBp = at(2, p::kFilterType),
                 kHp = at(3, p::kFilterType), kNotch = at(4, p::kFilterType);
constexpr double kMono = at(1, p::kVoiceMode), kLegato = at(2, p::kVoiceMode), kUnison = at(3, p::kVoiceMode);
constexpr double kMenvFm = at(1, p::kMenvDest), kMenvPw = at(2, p::kMenvDest);
constexpr double kLfoOsc = at(0, p::kLfo1Dest), kLfoFilter = at(2, p::kLfo1Dest), kLfoPw = at(3, p::kLfo1Dest),
                 kLfoFm = at(4, p::kLfo1Dest);
constexpr double kLfoSquare = at(2, p::kLfo1Wave), kLfoSampleHold = at(3, p::kLfo1Wave);
constexpr double kLfo2Amp = at(0, p::kLfo2Dest), kLfo2Filter = at(1, p::kLfo2Dest);
constexpr double kArpUp = at(0, p::kArpMode), kArpUpDown = at(2, p::kArpMode), kArpRandom = at(3, p::kArpMode);
constexpr double kArp1 = at(0, p::kArpRange), kArp2 = at(1, p::kArpRange);
constexpr double kArp8th = at(0, p::kArpRate), kArp16th = at(1, p::kArpRate), kArp16thT = at(3, p::kArpRate);
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

    // ---- Bank 3: industrial and EBM ------------------------------------------
    case 16: return make("EBM Bass", {{"osc1_wave", kSaw1}, {"osc2_wave", kPulse2}, {"osc2_semi", semi(-12)}, {"pw", 0.3},
                                      {"filter_type", kLp24}, {"cutoff", 0.28}, {"res", 0.3}, {"drive", 0.4},
                                      {"fenv_amt", 0.45}, {"fenv_d", 0.3}, {"fenv_s", 0}, {"voice_mode", kMono},
                                      {"aenv_r", 0.1}},
                         {{"cutoff", 0.5}, {"drive", 0.7}});
    case 17: return make("Metal Stab", {{"osc1_wave", kPulse1}, {"osc2_wave", kPulse2}, {"osc2_semi", semi(6)}, {"ring", 1},
                                        {"osc_mix", 0.8}, {"cutoff", 0.7}, {"res", 0.25}, {"drive", 0.5}, {"fenv_amt", 0.2},
                                        {"aenv_d", 0.4}, {"aenv_s", 0}, {"aenv_r", 0.35}, {"reverb_level", 0.3}});
    case 18: return make("Grind Lead", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"sync", 1}, {"osc_mix", 0.8},
                                        {"osc2_semi", semi(5)}, {"menv_amt", 0.6}, {"menv_d", 0.4}, {"drive", 0.8},
                                        {"cutoff", 0.6}, {"res", 0.4}, {"voice_mode", kLegato}, {"glide", 0.12}},
                         {{"menv_amt", 0.95}, {"cutoff", 0.85}});
    case 19: return make("Clang Hit", {{"analog", 0}, {"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(13)}, {"osc_mix", 0},
                                       {"fm", 0.6}, {"menv_dest", kMenvFm}, {"menv_amt", 0.9}, {"menv_d", 0.35},
                                       {"cutoff", 0.85}, {"fenv_amt", 0}, {"aenv_d", 0.4}, {"aenv_s", 0}, {"aenv_r", 0.4},
                                       {"reverb_level", 0.35}});
    case 20: return make("Noise Snare", {{"osc1_wave", kTri1}, {"osc2_wave", kNoise2}, {"osc_mix", 0.8}, {"filter_type", kHp},
                                         {"cutoff", 0.45}, {"res", 0.15}, {"drive", 0.35}, {"gain", 1.0},
                                         {"fenv_amt", 0.3}, {"fenv_d", 0.25}, {"fenv_s", 0},
                                         {"aenv_d", 0.3}, {"aenv_s", 0}, {"aenv_r", 0.25}, {"reverb_level", 0.25}});
    case 21: return make("Dark Drone", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(-12)}, {"osc2_fine", 0.53},
                                        {"filter_type", kLp24}, {"cutoff", 0.25}, {"res", 0.5}, {"fenv_amt", 0},
                                        {"lfo1_dest", kLfoFilter}, {"lfo1_amt", 0.3}, {"lfo1_rate", 0.15}, {"aenv_a", 0.5},
                                        {"aenv_r", 0.7}, {"reverb_level", 0.5}, {"reverb_size", 0.8}},
                         {{"cutoff", 0.55}, {"res", 0.75}});
    case 22: return make("Body Bass", {{"osc1_wave", kPulse1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(-12)}, {"pw", 0.3},
                                       {"filter_type", kLp24}, {"cutoff", 0.3}, {"res", 0.2}, {"drive", 0.3},
                                       {"fenv_amt", 0.3}, {"fenv_d", 0.35}, {"fenv_s", 0.2}, {"voice_mode", kMono},
                                       {"glide", 0.05}, {"aenv_r", 0.12}});
    case 23: return make("Factory Hit", {{"analog", 0}, {"osc1_wave", kSaw1}, {"osc2_wave", kNoise2}, {"osc_mix", 0.5}, {"filter_type", kNotch},
                                         {"cutoff", 0.5}, {"res", 0.5}, {"drive", 0.7}, {"fenv_amt", 0.35}, {"fenv_d", 0.3},
                                         {"aenv_d", 0.45}, {"aenv_s", 0}, {"aenv_r", 0.4}, {"reverb_level", 0.45},
                                         {"reverb_size", 0.7}});
    // ---- Bank 4: acid and techno basses ---------------------------------------
    case 24: return make("Acid Squelch", {{"osc1_wave", kSaw1}, {"osc_mix", 0}, {"cutoff", 0.25}, {"res", 0.88},
                                          {"fenv_amt", 0.6}, {"fenv_d", 0.32}, {"fenv_s", 0}, {"drive", 0.45},
                                          {"voice_mode", kMono}, {"glide", 0.18}, {"aenv_r", 0.15}},
                         {{"cutoff", 0.6}, {"res", 0.95}});
    case 25: return make("Acid Square", {{"osc1_wave", kPulse1}, {"osc_mix", 0}, {"cutoff", 0.28}, {"res", 0.85},
                                         {"fenv_amt", 0.55}, {"fenv_d", 0.3}, {"fenv_s", 0}, {"drive", 0.3},
                                         {"voice_mode", kMono}, {"glide", 0.15}, {"aenv_r", 0.15}},
                         {{"cutoff", 0.55}});
    case 26: return make("Rubber Bass", {{"osc1_wave", kTri1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(-12)},
                                         {"filter_type", kLp24}, {"cutoff", 0.32}, {"res", 0.45}, {"fenv_amt", 0.4},
                                         {"fenv_d", 0.28}, {"fenv_s", 0}, {"voice_mode", kMono}, {"aenv_r", 0.12}});
    case 27: return make("Motor Bass", {{"osc1_wave", kSaw1}, {"osc2_wave", kPulse2}, {"osc2_semi", semi(-12)},
                                        {"filter_type", kLp24}, {"cutoff", 0.38}, {"res", 0.2}, {"fenv_amt", 0.3},
                                        {"fenv_d", 0.4}, {"fenv_s", 0.3}, {"voice_mode", kMono}, {"glide", 0.05}});
    case 28: return make("Minimal Blip", {{"analog", 0}, {"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(12)}, {"fm", 0.15},
                                          {"cutoff", 0.8}, {"fenv_amt", 0}, {"aenv_d", 0.25}, {"aenv_s", 0}, {"aenv_r", 0.2},
                                          {"delay_level", 0.35}, {"delay_fb", 0.4}});
    case 29: return make("Warehouse Sub", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc_mix", 0.2}, {"cutoff", 0.5},
                                           {"fenv_amt", 0}, {"drive", 0.2}, {"voice_mode", kMono}, {"aenv_r", 0.15}});
    case 30: return make("Kick Bass", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc_mix", 0.6}, {"menv_amt", 0.9},
                                       {"menv_d", 0.2}, {"cutoff", 0.7}, {"fenv_amt", 0}, {"drive", 0.6}, {"gain", 1.0},
                                       {"aenv_d", 0.35}, {"aenv_s", 0.3}, {"voice_mode", kMono}, {"aenv_r", 0.15}});
    case 31: return make("Resonant Seq", {{"osc1_wave", kSaw1}, {"osc_mix", 0}, {"filter_type", kLp12}, {"cutoff", 0.5},
                                          {"res", 0.65}, {"drive", 0.3}, {"gain", 1.0}, {"fenv_amt", 0.35}, {"fenv_d", 0.3}, {"lfo1_dest", kLfoFilter}, {"lfo1_amt", 0.25}, {"lfo1_rate", 0.6},
                                          {"aenv_d", 0.35}, {"aenv_s", 0.2}, {"arp_on", 1}, {"arp_mode", kArpUp},
                                          {"arp_range", kArp1}, {"arp_rate", kArp16th}},
                         {{"cutoff", 0.6}});
    // ---- Bank 5: rave and stabs -------------------------------------------------
    case 32: return make("Rave Chord", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(7)},
                                        {"voice_mode", kUnison}, {"unison_detune", 0.6}, {"cutoff", 0.65}, {"fenv_amt", 0.3},
                                        {"fenv_d", 0.35}, {"fenv_s", 0.2}, {"aenv_d", 0.5}, {"aenv_s", 0.2}});
    case 33: return make("Rave Whoosh", {{"osc1_wave", kPulse1}, {"osc2_wave", kPulse2}, {"pw", 0.55}, {"menv_dest", kMenvPw},
                                         {"menv_amt", 0.3}, {"voice_mode", kUnison}, {"unison_detune", 0.9}, {"glide", 0.3},
                                         {"cutoff", 0.7}, {"lfo1_amt", 0.08}, {"lfo1_rate", 0.4}},
                         {{"unison_detune", 1.0}, {"glide", 0.5}});
    case 34: return make("Keys Stab", {{"osc1_wave", kTri1}, {"osc2_wave", kPulse2}, {"osc2_semi", semi(12)}, {"cutoff", 0.7},
                                       {"fenv_amt", 0.3}, {"fenv_d", 0.3}, {"aenv_d", 0.45}, {"aenv_s", 0.1},
                                       {"aenv_r", 0.35}, {"chorus", kChorus}});
    case 35: return make("Minor Stab", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(3)}, {"cutoff", 0.6},
                                        {"res", 0.3}, {"fenv_amt", 0.4}, {"fenv_d", 0.32}, {"fenv_s", 0}, {"aenv_d", 0.4},
                                        {"aenv_s", 0}, {"aenv_r", 0.3}, {"reverb_level", 0.25}});
    case 36: return make("Hardcore Lead", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"sync", 1}, {"osc2_semi", semi(5)},
                                           {"osc_mix", 0.7}, {"voice_mode", kUnison}, {"unison_detune", 0.7}, {"drive", 0.5},
                                           {"cutoff", 0.8}});
    case 37: return make("Rave Siren", {{"osc1_wave", kTri1}, {"osc2_wave", kSaw2}, {"lfo1_dest", kLfoOsc}, {"lfo1_amt", 0.35},
                                        {"lfo1_rate", 0.55}, {"voice_mode", kLegato}, {"cutoff", 0.7}, {"delay_level", 0.3},
                                        {"delay_fb", 0.4}},
                         {{"lfo1_rate", 0.75}});
    case 38: return make("Bleep", {{"analog", 0}, {"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(24)}, {"osc_mix", 0.3},
                                   {"cutoff", 0.9}, {"fenv_amt", 0}, {"aenv_d", 0.2}, {"aenv_s", 0}, {"aenv_r", 0.15},
                                   {"delay_level", 0.4}, {"delay_fb", 0.45}});
    case 39: return make("Gated Chord", {{"osc1_wave", kSaw1}, {"osc2_wave", kPulse2}, {"osc2_semi", semi(7)}, {"cutoff", 0.6},
                                         {"lfo2_dest", kLfo2Amp}, {"lfo2_amt", 0.9}, {"lfo2_rate", 0.65}, {"aenv_a", 0.2},
                                         {"aenv_r", 0.4}, {"chorus", kEnsemble}});
    // ---- Bank 6: house ---------------------------------------------------------------
    case 40: return make("House Organ", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(19)}, {"osc_mix", 0.4},
                                         {"cutoff", 0.75}, {"fenv_amt", 0}, {"aenv_d", 0.4}, {"aenv_s", 0.6},
                                         {"aenv_r", 0.2}, {"chorus", kChorus}});
    case 41: return make("House Keys", {{"osc1_wave", kTri1}, {"osc2_wave", kPulse2}, {"pw", 0.2}, {"cutoff", 0.6},
                                        {"fenv_amt", 0.35}, {"fenv_d", 0.35}, {"fenv_s", 0.15}, {"aenv_d", 0.55},
                                        {"aenv_s", 0.2}, {"aenv_r", 0.4}, {"chorus", kChorus}, {"reverb_level", 0.2}});
    case 42: return make("Deep Chord", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.55}, {"filter_type", kLp24},
                                        {"cutoff", 0.4}, {"res", 0.25}, {"fenv_amt", 0.3}, {"fenv_d", 0.45}, {"fenv_s", 0.25},
                                        {"aenv_d", 0.5}, {"aenv_s", 0.4}, {"aenv_r", 0.5}, {"chorus", kEnsemble},
                                        {"delay_level", 0.25}},
                         {{"cutoff", 0.65}});
    case 43: return make("Garage Bass", {{"osc1_wave", kSaw1}, {"osc_mix", 0}, {"filter_type", kLp24}, {"cutoff", 0.42}, {"gain", 1.0},
                                         {"res", 0.3}, {"fenv_amt", 0.35}, {"fenv_d", 0.3}, {"fenv_s", 0.1},
                                         {"voice_mode", kMono}, {"glide", 0.1}, {"aenv_r", 0.12}});
    case 44: return make("Soul Strings", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.56}, {"cutoff", 0.55},
                                          {"fenv_amt", 0.05}, {"aenv_a", 0.45}, {"aenv_r", 0.55}, {"chorus", kEnsemble},
                                          {"reverb_level", 0.35}});
    case 45: return make("Disco Bass", {{"osc1_wave", kPulse1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(-12)}, {"pw", 0.35},
                                        {"cutoff", 0.4}, {"fenv_amt", 0.35}, {"fenv_d", 0.25}, {"fenv_s", 0.3},
                                        {"amp_vel", 0.7}, {"voice_mode", kMono}, {"aenv_r", 0.12}});
    case 46: return make("House Pad", {{"osc1_wave", kPulse1}, {"osc2_wave", kSaw2}, {"pw", 0.45}, {"lfo1_dest", kLfoPw},
                                       {"lfo1_amt", 0.25}, {"lfo1_rate", 0.3}, {"cutoff", 0.45}, {"fenv_amt", 0.05},
                                       {"aenv_a", 0.45}, {"aenv_r", 0.6}, {"chorus", kEnsemble}, {"reverb_level", 0.45}},
                         {{"cutoff", 0.7}});
    case 47: return make("Vowel Stab", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(12)}, {"filter_type", kBp},
                                        {"cutoff", 0.55}, {"res", 0.6}, {"lfo1_dest", kLfoFilter}, {"lfo1_amt", 0.15},
                                        {"lfo1_rate", 0.35}, {"aenv_d", 0.4}, {"aenv_s", 0.3}, {"aenv_r", 0.3},
                                        {"chorus", kChorus}},
                         {{"cutoff", 0.75}});
    // ---- Bank 7: leads and arpeggios -------------------------------------------------
    case 48: return make("Acid Lead", {{"osc1_wave", kSaw1}, {"osc_mix", 0}, {"cutoff", 0.45}, {"res", 0.75},
                                       {"fenv_amt", 0.45}, {"fenv_d", 0.4}, {"voice_mode", kLegato}, {"glide", 0.2},
                                       {"delay_level", 0.3}, {"delay_fb", 0.4}},
                         {{"cutoff", 0.7}, {"res", 0.9}});
    case 49: return make("Trance Lead", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"voice_mode", kUnison},
                                         {"unison_detune", 0.55}, {"cutoff", 0.75}, {"res", 0.15}, {"delay_level", 0.35},
                                         {"delay_fb", 0.45}, {"reverb_level", 0.3}});
    case 50: return make("Sync Scream", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"sync", 1}, {"osc_mix", 1},
                                         {"menv_amt", 0.95}, {"menv_d", 0.6}, {"drive", 0.6}, {"cutoff", 0.8},
                                         {"voice_mode", kLegato}, {"glide", 0.15}},
                         {{"menv_d", 0.85}});
    case 51: return make("Arp Techno", {{"osc1_wave", kSaw1}, {"osc2_wave", kPulse2}, {"cutoff", 0.45}, {"res", 0.5}, {"gain", 1.0},
                                        {"fenv_amt", 0.5}, {"fenv_d", 0.3}, {"fenv_s", 0}, {"aenv_d", 0.35}, {"aenv_s", 0.2},
                                        {"arp_on", 1}, {"arp_mode", kArpUp}, {"arp_range", kArp2}, {"arp_rate", kArp16th},
                                        {"delay_level", 0.3}, {"delay_fb", 0.35}},
                         {{"cutoff", 0.65}, {"res", 0.7}});
    case 52: return make("Arp Bells", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(12)}, {"osc_mix", 0},
                                       {"fm", 0.3}, {"cutoff", 0.85}, {"fenv_amt", 0}, {"aenv_d", 0.4}, {"aenv_s", 0},
                                       {"aenv_r", 0.45}, {"arp_on", 1}, {"arp_mode", kArpUpDown}, {"arp_range", kArp2},
                                       {"arp_rate", kArp16th}, {"reverb_level", 0.35}});
    case 53: return make("Square Lead", {{"osc1_wave", kPulse1}, {"osc2_wave", kPulse2}, {"osc2_fine", 0.53}, {"cutoff", 0.6},
                                         {"res", 0.2}, {"fenv_amt", 0.2}, {"voice_mode", kLegato}, {"glide", 0.12},
                                         {"delay_level", 0.25}});
    case 54: return make("Ring Lead", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_semi", semi(7)}, {"ring", 1},
                                       {"osc_mix", 0.6}, {"cutoff", 0.7}, {"voice_mode", kLegato}, {"glide", 0.1},
                                       {"delay_level", 0.3}});
    case 55: return make("Chip Arp", {{"osc1_wave", kPulse1}, {"osc2_wave", kTri2}, {"pw", 0.6}, {"osc_mix", 0.3},
                                      {"cutoff", 0.9}, {"fenv_amt", 0}, {"aenv_d", 0.2}, {"aenv_s", 0}, {"aenv_r", 0.15},
                                      {"arp_on", 1}, {"arp_mode", kArpRandom}, {"arp_range", kArp2}, {"arp_rate", kArp16thT}});
    // ---- Bank 8: pads and effects ---------------------------------------------------
    case 56: return make("Analog Dawn", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.55}, {"filter_type", kLp24},
                                         {"cutoff", 0.3}, {"res", 0.2}, {"fenv_amt", 0.4}, {"fenv_a", 0.7}, {"fenv_d", 0.8},
                                         {"fenv_s", 0.5}, {"aenv_a", 0.6}, {"aenv_r", 0.7}, {"chorus", kEnsemble},
                                         {"reverb_level", 0.5}},
                         {{"cutoff", 0.6}});
    case 57: return make("Cold Wave", {{"osc1_wave", kPulse1}, {"osc2_wave", kTri2}, {"pw", 0.3}, {"lfo1_dest", kLfoPw},
                                       {"lfo1_amt", 0.35}, {"lfo1_rate", 0.25}, {"cutoff", 0.5}, {"fenv_amt", 0.05},
                                       {"aenv_a", 0.5}, {"aenv_r", 0.65}, {"chorus", kChorus}, {"reverb_level", 0.45}});
    case 58: return make("Dark Choir", {{"osc1_wave", kSaw1}, {"osc2_wave", kSaw2}, {"osc2_fine", 0.54}, {"filter_type", kBp},
                                        {"cutoff", 0.5}, {"res", 0.55}, {"fenv_amt", 0}, {"aenv_a", 0.6}, {"aenv_r", 0.7},
                                        {"chorus", kEnsemble}, {"reverb_level", 0.6}, {"reverb_size", 0.8}},
                         {{"cutoff", 0.62}});
    case 59: return make("Sweep FX", {{"osc2_wave", kNoise2}, {"osc_mix", 1}, {"filter_type", kHp}, {"res", 0.75},
                                      {"cutoff", 0.1}, {"fenv_amt", 0.7}, {"fenv_a", 0.8}, {"fenv_d", 0.8}, {"fenv_s", 0.4},
                                      {"lfo1_dest", kLfoFilter}, {"lfo1_amt", 0.3}, {"lfo1_rate", 0.2}, {"aenv_a", 0.4},
                                      {"aenv_r", 0.6}, {"reverb_level", 0.5}});
    case 60: return make("Laser Zap", {{"analog", 0}, {"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc_mix", 1}, {"menv_amt", 1.0},
                                       {"menv_d", 0.3}, {"cutoff", 0.85}, {"fenv_amt", 0}, {"aenv_d", 0.35}, {"aenv_s", 0},
                                       {"aenv_r", 0.3}, {"delay_level", 0.3}, {"delay_fb", 0.35}});
    case 61: return make("Riser", {{"osc1_wave", kSaw1}, {"osc2_wave", kNoise2}, {"osc_mix", 0.6}, {"filter_type", kHp},
                                   {"cutoff", 0.1}, {"res", 0.4}, {"fenv_amt", 0.7}, {"fenv_a", 0.9}, {"fenv_s", 1.0},
                                   {"aenv_a", 0.8}, {"aenv_r", 0.5}, {"reverb_level", 0.5}});
    case 62: return make("Glass Pad", {{"osc1_wave", kSine1}, {"osc2_wave", kTri2}, {"osc2_semi", semi(12)}, {"fm", 0.2},
                                       {"lfo1_dest", kLfoFm}, {"lfo1_amt", 0.2}, {"lfo1_rate", 0.2}, {"cutoff", 0.75},
                                       {"fenv_amt", 0}, {"aenv_a", 0.5}, {"aenv_r", 0.7}, {"chorus", kEnsemble},
                                       {"reverb_level", 0.55}},
                         {{"fm", 0.45}});
    case 63: return make("Drone Noise", {{"osc1_wave", kSaw1}, {"osc2_wave", kNoise2}, {"osc_mix", 0.5}, {"filter_type", kNotch},
                                         {"cutoff", 0.45}, {"res", 0.6}, {"lfo1_dest", kLfoFilter}, {"lfo1_amt", 0.35},
                                         {"lfo1_rate", 0.12}, {"lfo1_wave", kLfoSampleHold}, {"fenv_amt", 0},
                                         {"aenv_a", 0.7}, {"aenv_r", 0.8}, {"reverb_level", 0.6}, {"reverb_size", 0.85}});
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
    if (result == LoadResult::Loaded) {
        *bank = programsFromJson(json);
        // Slots still a plain Init (never stored to) get the factory sound for
        // that number: factory sounds added since the file was made appear.
        const Program init = initProgram();
        for (int i = 0; i < kProgramCount; ++i) {
            Program &p = bank->programs[std::size_t(i)];
            bool untouched = p.name == init.name && p.morph.isEmpty();
            for (auto it = init.values.begin(); untouched && it != init.values.end(); ++it)
                untouched = std::abs(p.values.value(it.key(), it.value()) - it.value()) < 1e-6;
            if (untouched)
                p = factoryProgram(i);
        }
    }
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
