// krate-render: render the Krate rack offline to a WAV file. k303 and Klick
// (K909 with --k909, Kalliope with --kalliope, Krimson with --krimson) play
// together on one transport,
// through the rack's mixer and master stage, exactly as the rack app runs them.
// Without those flags the output is unchanged from before they existed (a
// reference render). Kalliope holds a chord on MIDI channel 2 with its
// arpeggiator on (Up & Down, 2 octaves, 1/16), so it plays in time; Krimson
// holds one on channel 3 (slot A, Sync-Lead-like) with slot A's arpeggiator
// on (Up, 1 octave, 1/8).
//
//   krate-render [--out file.wav] [--bars N] [--tempo BPM] [--rate HZ]
//                [--k303 LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]
//                [--k909 LEVEL] [--k909-pattern N] [--kalliope LEVEL] [--krimson LEVEL]

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/Transport.h"
#include "dsp/Engine.h"
#include "dsp/Klick.h"
#include "k909/dsp/Engine.h"
#include "kalliope/dsp/Engine.h"
#include "krimson/dsp/Engine.h"
#include "rack/Rack.h"
#include "tools/WavFile.h"

namespace {

void usage()
{
    std::fprintf(stderr,
                 "usage: krate-render [--out file.wav] [--bars N] [--tempo BPM] [--rate HZ]\n"
                 "                    [--k303 LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]\n"
                 "                    [--k909 LEVEL] [--k909-pattern 0..3] [--kalliope LEVEL]\n"
                 "                    [--krimson LEVEL]\n"
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
    float kalliopeLevel = -1.0f; // < 0: Kalliope not in the rack
    float krimsonLevel = -1.0f;  // < 0: Krimson not in the rack

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
        else if (arg == "--kalliope" && hasValue) kalliopeLevel = float(std::atof(argv[++i]));
        else if (arg == "--krimson" && hasValue) krimsonLevel = float(std::atof(argv[++i]));
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
    kalliope::Engine poly;
    krimson::Engine lead;
    krate::Rack rack(kBlock);
    const int bassIndex = rack.addDevice(bass, /*midiChannel*/ 0);
    const int klickIndex = rack.addDevice(metronome);
    int drumsIndex = -1;
    if (k909Level >= 0.0f) {
        drums.setPattern(k909::factoryPattern(k909Pattern));
        drumsIndex = rack.addDevice(drums, /*midiChannel*/ 9);
    }
    int polyIndex = -1;
    if (kalliopeLevel >= 0.0f) {
        namespace kp = kalliope::positions;
        poly.setParam(kalliope::ParamId::PlayMode, kalliope::position(1, kp::kPlayMode));
        poly.setParam(kalliope::ParamId::ArpMode, kalliope::position(2, kp::kArpMode));
        poly.setParam(kalliope::ParamId::ArpRange, kalliope::position(1, kp::kArpRange));
        poly.setParam(kalliope::ParamId::Cutoff, 0.4f);
        poly.setParam(kalliope::ParamId::VcfEnv, 0.4f);
        poly.setParam(kalliope::ParamId::Env1S, 0.0f);
        poly.setParam(kalliope::ParamId::Env2S, 0.8f);
        polyIndex = rack.addDevice(poly, /*midiChannel*/ 1);
    }
    int leadIndex = -1;
    if (krimsonLevel >= 0.0f) {
        namespace kp = krimson::positions;
        using krimson::ParamId;
        lead.setSlotParam(0, ParamId::ArpOn, 1.0f);
        lead.setSlotParam(0, ParamId::ArpRate, krimson::position(0, kp::kArpRate)); // 1/8
        lead.setSlotParam(0, ParamId::Osc1Wave, krimson::position(2, kp::kOsc1Wave));
        lead.setSlotParam(0, ParamId::Sync, 1.0f);
        lead.setSlotParam(0, ParamId::OscMix, 1.0f);
        lead.setSlotParam(0, ParamId::MenvAmt, 0.8f);
        lead.setSlotParam(0, ParamId::MenvD, 0.5f);
        lead.setSlotParam(0, ParamId::DelayLevel, 0.3f);
        leadIndex = rack.addDevice(lead, /*midiChannel*/ 2);
        for (int channel : {3, 4, 5}) // slots B-D
            rack.setChannelDevice(channel, leadIndex);
    }
    rack.prepare(rate);
    if (leadIndex >= 0)
        rack.setLevel(leadIndex, krimsonLevel);
    if (drumsIndex >= 0)
        rack.setLevel(drumsIndex, k909Level);
    if (polyIndex >= 0)
        rack.setLevel(polyIndex, kalliopeLevel);
    // Kalliope's chord: C minor seventh, held from the first block on channel 2.
    const krate::MidiEvent chord[4] = {{0, 3, {0x91, 48, 100}}, {0, 3, {0x91, 51, 100}},
                                       {0, 3, {0x91, 55, 100}}, {0, 3, {0x91, 58, 100}}};
    // Krimson's: G minor on channel 3 (slot A).
    const krate::MidiEvent leadChord[3] = {{0, 3, {0x92, 67, 100}}, {0, 3, {0x92, 70, 100}}, {0, 3, {0x92, 74, 100}}};
    std::vector<krate::MidiEvent> firstBlock;
    if (polyIndex >= 0)
        firstBlock.insert(firstBlock.end(), chord, chord + 4);
    if (leadIndex >= 0)
        firstBlock.insert(firstBlock.end(), leadChord, leadChord + 3);
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
        if (done == 0 && !firstBlock.empty()) {
            block.midi = firstBlock.data();
            block.midiCount = std::uint32_t(firstBlock.size());
        }
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
    std::printf("wrote %s: k303 + Klick%s%s%s through the rack, %d bars at %.0f BPM, %.1f s\n", out.c_str(),
                k909Level >= 0.0f ? " + K909" : "", kalliopeLevel >= 0.0f ? " + Kalliope" : "",
                krimsonLevel >= 0.0f ? " + Krimson" : "", bars, bpm, double(frames) / rate);
    return 0;
}
