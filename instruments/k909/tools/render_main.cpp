// k909-render: render K909 offline to WAV files, or benchmark it.
//
//   k909-render [--out-dir DIR] [--rate HZ] [--tempo BPM] [--<param> 0..1 ...]
//   k909-render --bench [--rate HZ]
//
// Writes DIR/k909-voices.wav (every voice in turn, normal then accented, with
// its peak level printed) and DIR/k909-beat.wav (a 4-bar demo beat). <param> is
// any key from docs/BRIEF.md#parameters. Everything is played through MIDI
// notes on the IInstrument interface, as a host would.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "core/MasterStage.h"
#include "dsp/Engine.h"
#include "dsp/Params.h"
#include "tools/WavFile.h"

namespace {

constexpr std::uint32_t kBlock = 64;

// The first note of each voice in docs/BRIEF.md#midi-notes, in DrumId order.
constexpr int kNotes[k909::DrumCount] = {36, 38, 41, 45, 48, 37, 39, 42, 46, 49, 51};
constexpr const char *kNames[k909::DrumCount] = {"bass drum", "snare drum", "low tom", "mid tom", "hi tom",
                                                  "rim", "clap", "closed hat", "open hat", "crash", "ride"};

struct NoteAt
{
    std::uint64_t frame;
    int note;
    int velocity;
};

// Renders `frames` frames, sending each note on its exact frame; appends to `out`.
// Returns the peak absolute sample before the master stage.
float render(krate::IInstrument &instrument, krate::MasterStage &master, const std::vector<NoteAt> &notes,
             std::uint64_t frames, std::vector<float> &out)
{
    std::vector<float> l(kBlock), r(kBlock);
    std::vector<krate::MidiEvent> midi;
    std::size_t next = 0;
    float peak = 0.0f;
    for (std::uint64_t start = 0; start < frames; start += kBlock) {
        midi.clear();
        while (next < notes.size() && notes[next].frame < start + kBlock) {
            const NoteAt &n = notes[next++];
            midi.push_back({std::uint32_t(n.frame - start), 3, {0x99, std::uint8_t(n.note), std::uint8_t(n.velocity)}});
        }
        krate::processStereo(instrument, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
        for (std::uint32_t i = 0; i < kBlock; ++i)
            peak = std::max({peak, std::fabs(l[i]), std::fabs(r[i])});
        float *const outputs[2] = {l.data(), r.data()};
        master.process(outputs, 2, kBlock); // the same master stage as the JACK host
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            out.push_back(l[i]);
            out.push_back(r[i]);
        }
    }
    return peak;
}

// A 4-bar groove, one string of 16 steps per voice: '.' rest, 'x' hit, 'X' accent.
struct Track
{
    int note;
    const char *steps[4];
};
const Track kBeat[] = {
    {36, {"X...x...X...x...", "X...x...X...x...", "X...x...X...x...", "X...x...X..xx.x."}},
    {38, {"....X.......X...", "....X.......X...", "....X.......X..x", "....X.......X.xX"}},
    {39, {"............x...", "............x...", "............x...", "............x..."}},
    {42, {"x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x....."}},
    {46, {"..x...x...x...x.", "..x...x...x...x.", "..x...x...x...x.", "..x...x...x....."}},
    {37, {"...x.....x......", "...x.....x......", "...x.....x......", "................"}},
    {45, {"................", "................", "................", "........x.x....."}},
    {41, {"................", "................", "................", ".........x.x...."}},
    {48, {"................", "................", "................", "...........x...."}},
    {49, {"X...............", "................", "................", "................"}},
    {51, {"................", "x...x...x...x...", "x...x...x...x...", "................"}},
};

void usage()
{
    std::fprintf(stderr, "usage: k909-render [--out-dir DIR] [--rate HZ] [--tempo BPM] [--<param> 0..1 ...]\n"
                         "       k909-render --bench [--rate HZ]\n");
}

int paramIndex(std::string_view key)
{
    for (int i = 0; i < k909::ParamCount; ++i)
        if (k909::kParams[std::size_t(i)].key == key)
            return i;
    return -1;
}

} // namespace

int main(int argc, char **argv)
{
    std::string outDir = ".";
    int rate = 48000;
    double bpm = 128.0;
    bool bench = false;
    k909::Engine engine;
    krate::IInstrument &instrument = engine; // everything below goes through the interface

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--bench") {
            bench = true;
        } else if (arg == "--out-dir" && hasValue) {
            outDir = argv[++i];
        } else if (arg == "--rate" && hasValue) {
            rate = std::atoi(argv[++i]);
        } else if (arg == "--tempo" && hasValue) {
            bpm = std::atof(argv[++i]);
        } else if (arg.rfind("--", 0) == 0 && hasValue && paramIndex(arg.substr(2)) >= 0) {
            instrument.setParam(paramIndex(arg.substr(2)), float(std::atof(argv[++i])));
        } else {
            usage();
            return 2;
        }
    }
    if (rate < 8000 || bpm < 20.0 || bpm > 400.0) {
        usage();
        return 2;
    }
    instrument.prepare(rate, kBlock);
    krate::MasterStage master;

    if (bench) {
        // Every voice hit together on every beat: the worst case.
        std::vector<NoteAt> notes;
        const std::uint64_t frames = std::uint64_t(rate) * 60;
        const auto beat = std::uint64_t(rate * 60.0 / 120.0);
        for (std::uint64_t f = 0; f < frames; f += beat)
            for (int note : kNotes)
                notes.push_back({f, note, 127});
        std::vector<float> out;
        out.reserve(frames * 2 + 2 * kBlock);
        const auto t0 = std::chrono::steady_clock::now();
        render(instrument, master, notes, frames, out);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s at %d Hz (all 11 voices every beat) in %.3f s: %.2f%% of one core\n", rate,
                    seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    // Every voice in turn: normal, then accented.
    std::vector<float> voices;
    std::printf("peak per voice (before the master stage), normal / accented:\n");
    for (int v = 0; v < k909::DrumCount; ++v) {
        const bool long_ = v >= int(k909::DrumId::Oh);
        const auto gap = std::uint64_t(rate * (long_ ? 2.5 : 1.0));
        const float p1 = render(instrument, master, {{0, kNotes[v], 90}}, gap, voices);
        const float p2 = render(instrument, master, {{0, kNotes[v], 127}}, gap, voices);
        std::printf("  %-10s %6.1f dB / %6.1f dB\n", kNames[v], 20.0 * std::log10(std::max(p1, 1e-9f)),
                    20.0 * std::log10(std::max(p2, 1e-9f)));
    }
    const std::string voicesPath = outDir + "/k909-voices.wav";
    if (!krate::writeStereoWav(voicesPath, voices, rate)) {
        std::fprintf(stderr, "k909-render: could not write %s\n", voicesPath.c_str());
        return 1;
    }
    std::printf("wrote %s: %.1f s\n", voicesPath.c_str(), double(voices.size() / 2) / rate);

    // The demo beat.
    std::vector<NoteAt> notes;
    const double framesPerStep = rate * 60.0 / (bpm * 4.0);
    for (const Track &track : kBeat)
        for (int bar = 0; bar < 4; ++bar)
            for (int step = 0; step < 16; ++step) {
                const char c = track.steps[bar][step];
                if (c == 'x' || c == 'X')
                    notes.push_back({std::uint64_t(std::llround((bar * 16 + step) * framesPerStep)), track.note,
                                     c == 'X' ? 127 : 90});
            }
    std::sort(notes.begin(), notes.end(), [](const NoteAt &a, const NoteAt &b) { return a.frame < b.frame; });
    std::vector<float> beat;
    const auto frames = std::uint64_t(64 * framesPerStep + 2.0 * rate); // 4 bars plus 2 s to ring out
    const float peak = render(instrument, master, notes, frames, beat);
    const std::string beatPath = outDir + "/k909-beat.wav";
    if (!krate::writeStereoWav(beatPath, beat, rate)) {
        std::fprintf(stderr, "k909-render: could not write %s\n", beatPath.c_str());
        return 1;
    }
    std::printf("wrote %s: 4 bars at %.0f BPM, %.1f s, peak %.1f dB before the master stage\n", beatPath.c_str(), bpm,
                double(beat.size() / 2) / rate, 20.0 * std::log10(std::max(peak, 1e-9f)));
    return 0;
}
