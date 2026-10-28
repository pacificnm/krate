// krate-render: render the Krate rack offline to a WAV file. k303 and Klick
// (and K909 with --k909) play together on one transport, through the rack's
// mixer and master stage, exactly as the rack app runs them. Without --k909
// the output is unchanged from before K909 existed (a reference render).
//
//   krate-render [--out file.wav] [--bars N] [--tempo BPM] [--rate HZ]
//                [--k303 LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]
//                [--k909 LEVEL] [--k909-pattern N]

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/Transport.h"
#include "dsp/Engine.h"
#include "dsp/Klick.h"
#include "k909/dsp/Engine.h"
#include "rack/Rack.h"
#include "tools/WavFile.h"

namespace {

void usage()
{
    std::fprintf(stderr,
                 "usage: krate-render [--out file.wav] [--bars N] [--tempo BPM] [--rate HZ]\n"
                 "                    [--k303 LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]\n"
                 "                    [--k909 LEVEL] [--k909-pattern 0..3]\n"
                 "  LEVEL is mixer gain, 1 = unity\n");
}

} // namespace

int main(int argc, char **argv)
{
    std::string out = "krate.wav";
    int bars = 4;
    double bpm = 120.0;
    int rate = 48000;
    float k303Level = 1.0f;
    float klickLevel = 0.7f;
    bool muteKlick = false;
    bool soloKlick = false;
    float k909Level = -1.0f; // < 0: K909 not in the rack
    int k909Pattern = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--out" && hasValue) out = argv[++i];
        else if (arg == "--bars" && hasValue) bars = std::atoi(argv[++i]);
        else if (arg == "--tempo" && hasValue) bpm = std::atof(argv[++i]);
        else if (arg == "--rate" && hasValue) rate = std::atoi(argv[++i]);
        else if (arg == "--k303" && hasValue) k303Level = float(std::atof(argv[++i]));
        else if (arg == "--klick" && hasValue) klickLevel = float(std::atof(argv[++i]));
        else if (arg == "--mute-klick") muteKlick = true;
        else if (arg == "--solo-klick") soloKlick = true;
        else if (arg == "--k909" && hasValue) k909Level = float(std::atof(argv[++i]));
        else if (arg == "--k909-pattern" && hasValue) k909Pattern = std::atoi(argv[++i]);
        else {
            usage();
            return 2;
        }
    }
    if (bars < 1 || rate < 8000 || bpm < 20.0 || bpm > 400.0 || k909Pattern < 0
        || k909Pattern >= k909::kFactoryPatternCount) {
        usage();
        return 2;
    }

    constexpr std::uint32_t kBlock = 64;
    k303::Engine bass;
    klick::Klick metronome;
    k909::Engine drums;
    krate::Rack rack(kBlock);
    const int bassIndex = rack.addDevice(bass, /*midiChannel*/ 0);
    const int klickIndex = rack.addDevice(metronome);
    int drumsIndex = -1;
    if (k909Level >= 0.0f) {
        drums.setPattern(k909::factoryPattern(k909Pattern));
        drumsIndex = rack.addDevice(drums, /*midiChannel*/ 9);
    }
    rack.prepare(rate);
    if (drumsIndex >= 0)
        rack.setLevel(drumsIndex, k909Level);
    rack.setLevel(bassIndex, k303Level);
    rack.setLevel(klickIndex, klickLevel);
    rack.setMute(klickIndex, muteKlick);
    rack.setSolo(klickIndex, soloKlick);

    krate::Transport transport;
    transport.setBpm(bpm);
    transport.setPlaying(true);

    const double secondsPerBar = 4.0 * 60.0 / bpm;
    const auto frames = std::uint64_t((bars * secondsPerBar + 0.5) * rate);
    std::vector<float> l(kBlock), r(kBlock), interleaved;
    interleaved.reserve(frames * 2 + 2 * kBlock);
    for (std::uint64_t done = 0; done < frames; done += kBlock) {
        if (done + kBlock >= frames - std::uint64_t(rate / 2))
            transport.setPlaying(false); // last half second: let it ring out
        const krate::TransportState state = transport.advance(kBlock, rate);
        krate::RackBlock block;
        block.frames = kBlock;
        block.transport = &state;
        block.masterL = l.data();
        block.masterR = r.data();
        rack.process(block);
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            interleaved.push_back(l[i]);
            interleaved.push_back(r[i]);
        }
    }

    if (!krate::writeStereoWav(out, interleaved, rate)) {
        std::fprintf(stderr, "krate-render: could not write %s\n", out.c_str());
        return 1;
    }
    std::printf("wrote %s: k303 + Klick%s through the rack, %d bars at %.0f BPM, %.1f s\n", out.c_str(),
                k909Level >= 0.0f ? " + K909" : "", bars, bpm, double(frames) / rate);
    return 0;
}
