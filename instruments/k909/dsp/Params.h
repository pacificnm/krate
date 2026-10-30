#pragma once

#include <array>
#include <string>

#include "core/ParamDescriptor.h"

namespace k909 {

// K909's parameters, in panel order (docs/BRIEF.md#parameters): the voices'
// own knobs (SOUND page), then each voice's pan, drive and effect sends (MIX
// page), the effects and the master. The enum indexes kParams; keys are stable
// (panel.json, midimap.json, kits, rack sessions). The defaults play the
// original kit exactly (docs/DSP.md).
enum class ParamId : int {
    Accent,
    BdTune, BdLevel, BdAttack, BdDecay, BdSweep, BdBend, BdTone,
    SdTune, SdLevel, SdTone, SdSnappy, SdDecay, SdCrack,
    LtTune, LtLevel, LtDecay,
    MtTune, MtLevel, MtDecay,
    HtTune, HtLevel, HtDecay,
    RsTune, RsLevel, RsDecay,
    CpTune, CpLevel, CpDecay, CpSpread,
    ChLevel, ChDecay, OhLevel, OhDecay, HhTune,
    CrLevel, CrTune, RdLevel, RdTune,
    MeTune, MeLevel, MeDecay, MeMaterial,
    CkTune, CkLevel, CkDecay, CkHard,
    NzTone, NzLevel, NzDecay, NzSweep,
    BmTune, BmLevel, BmDecay,
    BdPan, SdPan, LtPan, MtPan, HtPan, RsPan, CpPan, ChPan, OhPan, CrPan, RdPan, MePan, CkPan, NzPan, BmPan,
    BdDrive, SdDrive, LtDrive, MtDrive, HtDrive, RsDrive, CpDrive, ChDrive, OhDrive, CrDrive, RdDrive, MeDrive, CkDrive, NzDrive, BmDrive,
    BdRev, SdRev, LtRev, MtRev, HtRev, RsRev, CpRev, ChRev, OhRev, CrRev, RdRev, MeRev, CkRev, NzRev, BmRev,
    BdDly, SdDly, LtDly, MtDly, HtDly, RsDly, CpDly, ChDly, OhDly, CrDly, RdDly, MeDly, CkDly, NzDly, BmDly,
    RevSize, RevGate, DlyTime, DlyFb, Comp, Crush,
    Volume,
    Shuffle,
    Tempo,
    Count
};
inline constexpr int ParamCount = int(ParamId::Count);

namespace format {
std::string percent(float v);
std::string level(float v);
std::string pan(float v);
std::string onOff(float v);
std::string delayTime(float v);
std::string shuffle(float v);
std::string tempo(float v);
} // namespace format

inline constexpr int kDelayTimes = 7; // 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2

// The normalized value of position  of an n-position control.
constexpr float position(int index, int count) { return float(index) / float(count - 1); }

inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"accent",      "Total Accent",          0.5f,                        0, &format::percent},
    {"bd_tune",     "Bass Drum Tune",        0.5f,                        0, &format::percent},
    {"bd_level",    "Bass Drum Level",       0.8f,                        0, &format::level},
    {"bd_attack",   "Bass Drum Attack",      0.5f,                        0, &format::percent},
    {"bd_decay",    "Bass Drum Decay",       0.5f,                        0, &format::percent},
    {"bd_sweep",    "Bass Drum Sweep",       0.5f,                        0, &format::percent},
    {"bd_bend",     "Bass Drum Bend Time",   0.5f,                        0, &format::percent},
    {"bd_tone",     "Bass Drum Tone",        0.5f,                        0, &format::percent},
    {"sd_tune",     "Snare Tune",            0.5f,                        0, &format::percent},
    {"sd_level",    "Snare Level",           0.8f,                        0, &format::level},
    {"sd_tone",     "Snare Tone",            0.5f,                        0, &format::percent},
    {"sd_snappy",   "Snare Snappy",          0.5f,                        0, &format::percent},
    {"sd_decay",    "Snare Decay",           0.5f,                        0, &format::percent},
    {"sd_crack",    "Snare Crack",           0.0f,                        0, &format::percent},
    {"lt_tune",     "Low Tom Tune",          0.5f,                        0, &format::percent},
    {"lt_level",    "Low Tom Level",         0.7f,                        0, &format::level},
    {"lt_decay",    "Low Tom Decay",         0.5f,                        0, &format::percent},
    {"mt_tune",     "Mid Tom Tune",          0.5f,                        0, &format::percent},
    {"mt_level",    "Mid Tom Level",         0.7f,                        0, &format::level},
    {"mt_decay",    "Mid Tom Decay",         0.5f,                        0, &format::percent},
    {"ht_tune",     "Hi Tom Tune",           0.5f,                        0, &format::percent},
    {"ht_level",    "Hi Tom Level",          0.7f,                        0, &format::level},
    {"ht_decay",    "Hi Tom Decay",          0.5f,                        0, &format::percent},
    {"rs_tune",     "Rim Tune",              0.5f,                        0, &format::percent},
    {"rs_level",    "Rim Level",             0.7f,                        0, &format::level},
    {"rs_decay",    "Rim Decay",             0.5f,                        0, &format::percent},
    {"cp_tune",     "Clap Tune",             0.5f,                        0, &format::percent},
    {"cp_level",    "Clap Level",            0.7f,                        0, &format::level},
    {"cp_decay",    "Clap Decay",            0.5f,                        0, &format::percent},
    {"cp_spread",   "Clap Spread",           0.5f,                        0, &format::percent},
    {"ch_level",    "Closed Hat Level",      0.7f,                        0, &format::level},
    {"ch_decay",    "Closed Hat Decay",      0.4f,                        0, &format::percent},
    {"oh_level",    "Open Hat Level",        0.7f,                        0, &format::level},
    {"oh_decay",    "Open Hat Decay",        0.4f,                        0, &format::percent},
    {"hh_tune",     "Hi Hat Tune",           0.5f,                        0, &format::percent},
    {"cr_level",    "Crash Level",           0.6f,                        0, &format::level},
    {"cr_tune",     "Crash Tune",            0.5f,                        0, &format::percent},
    {"rd_level",    "Ride Level",            0.6f,                        0, &format::level},
    {"rd_tune",     "Ride Tune",             0.5f,                        0, &format::percent},
    {"me_tune",     "Metal Tune",            0.5f,                        0, &format::percent},
    {"me_level",    "Metal Level",           0.7f,                        0, &format::level},
    {"me_decay",    "Metal Decay",           0.5f,                        0, &format::percent},
    {"me_material", "Metal Material",        0.3f,                        0, &format::percent},
    {"ck_tune",     "Clank Tune",            0.5f,                        0, &format::percent},
    {"ck_level",    "Clank Level",           0.7f,                        0, &format::level},
    {"ck_decay",    "Clank Decay",           0.4f,                        0, &format::percent},
    {"ck_hard",     "Clank Hardness",        0.5f,                        0, &format::percent},
    {"nz_tone",     "Noise Tone",            0.5f,                        0, &format::percent},
    {"nz_level",    "Noise Level",           0.7f,                        0, &format::level},
    {"nz_decay",    "Noise Decay",           0.4f,                        0, &format::percent},
    {"nz_sweep",    "Noise Sweep",           0.5f,                        0, &format::percent},
    {"bm_tune",     "Boom Tune",             0.5f,                        0, &format::percent},
    {"bm_level",    "Boom Level",            0.7f,                        0, &format::level},
    {"bm_decay",    "Boom Decay",            0.5f,                        0, &format::percent},
    {"bd_pan",      "Bass Drum Pan",         0.5f,                        0, &format::pan},
    {"sd_pan",      "Snare Pan",             0.5f,                        0, &format::pan},
    {"lt_pan",      "Low Tom Pan",           0.325f,                      0, &format::pan},
    {"mt_pan",      "Mid Tom Pan",           0.5f,                        0, &format::pan},
    {"ht_pan",      "Hi Tom Pan",            0.675f,                      0, &format::pan},
    {"rs_pan",      "Rim Pan",               0.575f,                      0, &format::pan},
    {"cp_pan",      "Clap Pan",              0.45f,                       0, &format::pan},
    {"ch_pan",      "Closed Hat Pan",        0.625f,                      0, &format::pan},
    {"oh_pan",      "Open Hat Pan",          0.625f,                      0, &format::pan},
    {"cr_pan",      "Crash Pan",             0.4f,                        0, &format::pan},
    {"rd_pan",      "Ride Pan",              0.35f,                       0, &format::pan},
    {"me_pan",      "Metal Pan",             0.65f,                       0, &format::pan},
    {"ck_pan",      "Clank Pan",             0.35f,                       0, &format::pan},
    {"nz_pan",      "Noise Pan",             0.5f,                        0, &format::pan},
    {"bm_pan",      "Boom Pan",              0.5f,                        0, &format::pan},
    {"bd_drive",    "Bass Drum Drive",       0.0f,                        0, &format::percent},
    {"sd_drive",    "Snare Drive",           0.0f,                        0, &format::percent},
    {"lt_drive",    "Low Tom Drive",         0.0f,                        0, &format::percent},
    {"mt_drive",    "Mid Tom Drive",         0.0f,                        0, &format::percent},
    {"ht_drive",    "Hi Tom Drive",          0.0f,                        0, &format::percent},
    {"rs_drive",    "Rim Drive",             0.0f,                        0, &format::percent},
    {"cp_drive",    "Clap Drive",            0.0f,                        0, &format::percent},
    {"ch_drive",    "Closed Hat Drive",      0.0f,                        0, &format::percent},
    {"oh_drive",    "Open Hat Drive",        0.0f,                        0, &format::percent},
    {"cr_drive",    "Crash Drive",           0.0f,                        0, &format::percent},
    {"rd_drive",    "Ride Drive",            0.0f,                        0, &format::percent},
    {"me_drive",    "Metal Drive",           0.0f,                        0, &format::percent},
    {"ck_drive",    "Clank Drive",           0.0f,                        0, &format::percent},
    {"nz_drive",    "Noise Drive",           0.0f,                        0, &format::percent},
    {"bm_drive",    "Boom Drive",            0.0f,                        0, &format::percent},
    {"bd_rev",      "Bass Drum Reverb Send", 0.0f,                        0, &format::percent},
    {"sd_rev",      "Snare Reverb Send",     0.0f,                        0, &format::percent},
    {"lt_rev",      "Low Tom Reverb Send",   0.0f,                        0, &format::percent},
    {"mt_rev",      "Mid Tom Reverb Send",   0.0f,                        0, &format::percent},
    {"ht_rev",      "Hi Tom Reverb Send",    0.0f,                        0, &format::percent},
    {"rs_rev",      "Rim Reverb Send",       0.0f,                        0, &format::percent},
    {"cp_rev",      "Clap Reverb Send",      0.0f,                        0, &format::percent},
    {"ch_rev",      "Closed Hat Reverb Send", 0.0f,                        0, &format::percent},
    {"oh_rev",      "Open Hat Reverb Send",  0.0f,                        0, &format::percent},
    {"cr_rev",      "Crash Reverb Send",     0.0f,                        0, &format::percent},
    {"rd_rev",      "Ride Reverb Send",      0.0f,                        0, &format::percent},
    {"me_rev",      "Metal Reverb Send",     0.0f,                        0, &format::percent},
    {"ck_rev",      "Clank Reverb Send",     0.0f,                        0, &format::percent},
    {"nz_rev",      "Noise Reverb Send",     0.0f,                        0, &format::percent},
    {"bm_rev",      "Boom Reverb Send",      0.0f,                        0, &format::percent},
    {"bd_dly",      "Bass Drum Delay Send",  0.0f,                        0, &format::percent},
    {"sd_dly",      "Snare Delay Send",      0.0f,                        0, &format::percent},
    {"lt_dly",      "Low Tom Delay Send",    0.0f,                        0, &format::percent},
    {"mt_dly",      "Mid Tom Delay Send",    0.0f,                        0, &format::percent},
    {"ht_dly",      "Hi Tom Delay Send",     0.0f,                        0, &format::percent},
    {"rs_dly",      "Rim Delay Send",        0.0f,                        0, &format::percent},
    {"cp_dly",      "Clap Delay Send",       0.0f,                        0, &format::percent},
    {"ch_dly",      "Closed Hat Delay Send", 0.0f,                        0, &format::percent},
    {"oh_dly",      "Open Hat Delay Send",   0.0f,                        0, &format::percent},
    {"cr_dly",      "Crash Delay Send",      0.0f,                        0, &format::percent},
    {"rd_dly",      "Ride Delay Send",       0.0f,                        0, &format::percent},
    {"me_dly",      "Metal Delay Send",      0.0f,                        0, &format::percent},
    {"ck_dly",      "Clank Delay Send",      0.0f,                        0, &format::percent},
    {"nz_dly",      "Noise Delay Send",      0.0f,                        0, &format::percent},
    {"bm_dly",      "Boom Delay Send",       0.0f,                        0, &format::percent},
    {"rev_size",    "Reverb Size",           0.5f,                        0, &format::percent},
    {"rev_gate",    "Reverb Gate",           0.0f,                        2, &format::onOff},
    {"dly_time",    "Delay Time",            position(2, kDelayTimes),    kDelayTimes, &format::delayTime},
    {"dly_fb",      "Delay Feedback",        0.3f,                        0, &format::percent},
    {"comp",        "Compressor",            0.0f,                        0, &format::percent},
    {"crush",       "Crush",                 0.0f,                        0, &format::percent},
    {"volume",      "Volume",                0.8f,                        0, &format::level},
    {"shuffle",     "Shuffle",               0.0f,                        6, &format::shuffle},
    {"tempo",       "Tempo",                 0.4f,                        0, &format::tempo},
}};

inline constexpr const krate::ParamDescriptor &paramDescriptor(ParamId id) { return kParams[std::size_t(id)]; }

namespace units {
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;
inline double gain(float level) { return double(level) * level; }
inline double tempoBpm(float v) { return kTempoMinBpm + (kTempoMaxBpm - kTempoMinBpm) * v; }
inline double tempoValue(double bpm) { return (bpm - kTempoMinBpm) / (kTempoMaxBpm - kTempoMinBpm); }
int shuffleAmount(float v);     // 0..5
inline double pan(float v) { return 2.0 * double(v) - 1.0; } // -1 (left) .. 1 (right)
int index(float v, int count);  // detent position 0..count-1
double delayBeats(float v);     // the Delay Time position in beats
} // namespace units

} // namespace k909
