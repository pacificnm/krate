// krimson-render: render Kalliope offline to WAV, or benchmark it.
//
//   krimson-render [--out-dir DIR] [--rate HZ] [--<param> 0..1 ...]
//   krimson-render --bench [--rate HZ]
//
// Writes DIR/krimson-sounds.wav: a few hard-coded sounds in turn (brass, PWM
// pad with chorus II, sync lead and cross-mod bell with delay, resonant sweep),
// each playing a short phrase through MIDI notes on the IInstrument interface,
// as a host would (tempo from the Tempo parameter: there is no transport).
// <param> overrides apply on top of every sound.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/MasterStage.h"
#include "krimson/dsp/Engine.h"
#include "krimson/dsp/Params.h"
#include "tools/WavFile.h"

namespace {

using krimson::ParamId;
constexpr std::uint32_t kBlock = 64;

struct NoteEvent
{
    double time; // seconds
    int note;    // > 0 on, < 0 off (-note)
    int velocity;
};

struct Sound
{
    const char *name;
    std::vector<std::pair<ParamId, float>> params;
    std::vector<NoteEvent> phrase;
};

float pos(int index, int count) { return krimson::position(index, count); }

std::vector<NoteEvent> line(std::initializer_list<int> notes, double step, double gate, int velocity = 100)
{
    std::vector<NoteEvent> e;
    double t = 0.0;
    for (int n : notes) {
        e.push_back({t, n, velocity});
        e.push_back({t + gate, -n, 0});
        t += step;
    }
    return e;
}

std::vector<Sound> sounds()
{
    namespace p = krimson::positions;
    const float saw1 = pos(2, p::kOsc1Wave), pulse1 = pos(3, p::kOsc1Wave), sine1 = pos(0, p::kOsc1Wave);
    const float saw2 = pos(1, p::kOsc2Wave), tri2 = pos(0, p::kOsc2Wave), pulse2 = pos(2, p::kOsc2Wave);
    const float semi = 1.0f / 48.0f; // one semitone on OSC 2 Semitones
    return {
        {"sync lead", {{ParamId::Osc1Wave, saw1}, {ParamId::Osc2Wave, saw2}, {ParamId::Sync, 1.0f}, {ParamId::OscMix, 1.0f},
                       {ParamId::MenvAmt, 0.85f}, {ParamId::MenvD, 0.5f}, {ParamId::Cutoff, 0.85f}, {ParamId::FenvAmt, 0.1f},
                       {ParamId::Osc2Semi, 0.5f + 7 * semi}, {ParamId::DelayLevel, 0.35f}, {ParamId::DelayFb, 0.4f}},
         line({60, 63, 67, 72, 70, 67}, 0.3, 0.25)},
        {"fm bell", {{ParamId::Osc1Wave, sine1}, {ParamId::Osc2Wave, tri2}, {ParamId::Osc2Semi, 0.5f + 19 * semi},
                     {ParamId::OscMix, 0.0f}, {ParamId::Fm, 0.35f}, {ParamId::MenvDest, pos(1, p::kMenvDest)},
                     {ParamId::MenvAmt, 0.8f}, {ParamId::MenvD, 0.55f}, {ParamId::Cutoff, 0.9f}, {ParamId::FenvAmt, 0.0f},
                     {ParamId::AenvD, 0.65f}, {ParamId::AenvS, 0.0f}, {ParamId::AenvR, 0.6f}, {ParamId::ReverbLevel, 0.4f},
                     {ParamId::ReverbSize, 0.6f}},
         line({72, 79, 76, 84}, 0.6, 0.1)},
        {"ring clang", {{ParamId::Osc1Wave, pulse1}, {ParamId::Osc2Wave, pulse2}, {ParamId::Ring, 1.0f}, {ParamId::OscMix, 1.0f},
                        {ParamId::Osc2Semi, 0.5f + 11 * semi}, {ParamId::Cutoff, 0.8f}, {ParamId::AenvD, 0.55f},
                        {ParamId::AenvS, 0.0f}, {ParamId::AenvR, 0.5f}},
         line({60, 67, 64, 72}, 0.5, 0.1)},
        {"acid 12 dB bass", {{ParamId::Osc1Wave, saw1}, {ParamId::OscMix, 0.0f}, {ParamId::Cutoff, 0.3f}, {ParamId::Res, 0.8f},
                             {ParamId::FenvAmt, 0.5f}, {ParamId::FenvD, 0.4f}, {ParamId::FenvS, 0.0f}, {ParamId::Drive, 0.3f},
                             {ParamId::AenvR, 0.2f}},
         line({36, 36, 48, 36, 39, 36, 43, 36}, 0.2, 0.15, 110)},
        {"driven saw lead", {{ParamId::Osc1Wave, saw1}, {ParamId::Osc2Wave, saw2}, {ParamId::Osc2Fine, 0.58f},
                             {ParamId::Drive, 0.6f}, {ParamId::Cutoff, 0.6f}, {ParamId::Res, 0.3f}, {ParamId::FenvAmt, 0.3f},
                             {ParamId::FilterType, pos(1, p::kFilterType)}, {ParamId::Lfo1Amt, 0.25f}, {ParamId::Lfo1Rate, 0.45f}},
         line({67, 70, 72, 74, 72}, 0.35, 0.3)},
        {"notch pad", {{ParamId::Osc1Wave, pulse1}, {ParamId::Osc2Wave, saw2}, {ParamId::Pw, 0.5f}, {ParamId::Osc2Fine, 0.55f},
                       {ParamId::FilterType, pos(4, p::kFilterType)}, {ParamId::Res, 0.6f}, {ParamId::Cutoff, 0.5f},
                       {ParamId::Lfo1Dest, pos(2, p::kLfo1Dest)}, {ParamId::Lfo1Amt, 0.5f}, {ParamId::Lfo1Rate, 0.3f},
                       {ParamId::AenvA, 0.55f}, {ParamId::AenvR, 0.6f}, {ParamId::FenvAmt, 0.0f},
                       {ParamId::Chorus, 1.0f}, {ParamId::ReverbLevel, 0.35f}},
         line({57}, 2.0, 1.8)},
    };
}

// Plays `phrase` and appends `seconds` of output to `out`. Returns the peak before the master stage.
float play(krimson::Engine &engine, krate::MasterStage &master, const std::vector<NoteEvent> &phrase,
           double seconds, int rate, std::vector<float> &out)
{
    std::vector<NoteEvent> events = phrase;
    std::sort(events.begin(), events.end(), [](const NoteEvent &a, const NoteEvent &b) { return a.time < b.time; });
    const auto frames = std::uint64_t(seconds * rate);
    std::vector<float> l(kBlock), r(kBlock);
    std::vector<krate::MidiEvent> midi;
    std::size_t next = 0;
    float peak = 0.0f;
    for (std::uint64_t start = 0; start < frames; start += kBlock) {
        midi.clear();
        while (next < events.size() && std::uint64_t(events[next].time * rate) < start + kBlock) {
            const NoteEvent &e = events[next++];
            const auto frame = std::uint32_t(std::uint64_t(e.time * rate) - start);
            if (e.note > 0)
                midi.push_back({frame, 3, {0x90, std::uint8_t(e.note), std::uint8_t(e.velocity)}});
            else
                midi.push_back({frame, 3, {0x80, std::uint8_t(-e.note), 0}});
        }
        krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
        for (std::uint32_t i = 0; i < kBlock; ++i)
            peak = std::max(peak, std::fabs(l[i]));
        float *const outputs[2] = {l.data(), r.data()};
        master.process(outputs, 2, kBlock);
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            out.push_back(l[i]);
            out.push_back(r[i]);
        }
    }
    return peak;
}

int paramIndex(std::string_view key)
{
    for (int i = 0; i < krimson::ParamCount; ++i)
        if (krimson::kParams[std::size_t(i)].key == key)
            return i;
    return -1;
}

void usage()
{
    std::fprintf(stderr, "usage: krimson-render [--out-dir DIR] [--rate HZ] [--<param> 0..1 ...]\n"
                         "       krimson-render --bench [--rate HZ]\n");
}

} // namespace

int main(int argc, char **argv)
{
    std::string outDir = ".";
    int rate = 48000;
    bool bench = false;
    std::vector<std::pair<int, float>> overrides;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--bench") {
            bench = true;
        } else if (arg == "--out-dir" && hasValue) {
            outDir = argv[++i];
        } else if (arg == "--rate" && hasValue) {
            rate = std::atoi(argv[++i]);
        } else if (arg.rfind("--", 0) == 0 && hasValue && paramIndex(arg.substr(2)) >= 0) {
            overrides.push_back({paramIndex(arg.substr(2)), float(std::atof(argv[++i]))});
        } else {
            usage();
            return 2;
        }
    }
    if (rate < 8000) {
        usage();
        return 2;
    }

    krate::MasterStage master;
    if (bench) {
        // All 16 voices held with sync, FM, drive and full resonance, through
        // ensemble, delay and reverb, 60 s.
        krimson::Engine engine;
        engine.prepare(rate, kBlock);
        engine.setParam(ParamId::Sync, 1.0f);
        engine.setParam(ParamId::Fm, 0.5f);
        engine.setParam(ParamId::Res, 1.0f);
        engine.setParam(ParamId::Drive, 0.5f);
        engine.setParam(ParamId::Chorus, 1.0f);      // Ensemble
        engine.setParam(ParamId::DelayLevel, 0.5f);
        engine.setParam(ParamId::ReverbLevel, 0.5f);
        std::vector<float> out;
        out.reserve(std::size_t(rate) * 60 * 2 + 2 * kBlock);
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<NoteEvent> chord;
        for (int note = 40; note < 72; note += 2)
            chord.push_back({0.0, note, 100});
        play(engine, master, chord, 60.0, rate, out);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s of 16 voices with effects at %d Hz (2x oversampled) in %.3f s: %.2f%% of one core\n", rate,
                    seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    std::vector<float> out;
    std::printf("peak per sound (before the master stage):\n");
    for (const Sound &sound : sounds()) {
        krimson::Engine engine; // each sound from the default panel
        engine.prepare(rate, kBlock);
        for (const auto &[id, value] : sound.params)
            engine.setParam(id, value);
        for (const auto &[index, value] : overrides)
            engine.setParam(index, value);
        double length = 0.0;
        for (const NoteEvent &e : sound.phrase)
            length = std::max(length, e.time);
        const float peak = play(engine, master, sound.phrase, length + 2.5, rate, out);
        std::printf("  %-15s %6.1f dB\n", sound.name, 20.0 * std::log10(std::max(peak, 1e-9f)));
    }
    const std::string path = outDir + "/krimson-sounds.wav";
    if (!krate::writeStereoWav(path, out, rate)) {
        std::fprintf(stderr, "krimson-render: could not write %s\n", path.c_str());
        return 1;
    }
    std::printf("wrote %s: %.1f s\n", path.c_str(), double(out.size() / 2) / rate);
    return 0;
}
