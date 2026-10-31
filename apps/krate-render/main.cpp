// krate-render: render the Krate rack offline to a WAV file. Korrode and Klick
// (Kessel with --kessel, Kalliope with --kalliope, Krimson with --krimson, Kobalt
// with --kobalt, Klang with --klang) play
// together on one transport,
// through the rack's mixer and master stage, exactly as the rack app runs them.
// Without those flags the output is unchanged from before they existed (a
// reference render). Kalliope holds a chord on MIDI channel 2 with its
// arpeggiator on (Up & Down, 2 octaves, 1/16), so it plays in time; Krimson
// holds one on channel 3 (slot A, Sync-Lead-like) with slot A's arpeggiator
// on (Up, 1 octave, 1/8); Kobalt plays a 16-step bass line from its own
// sequencer on channel 7, some steps legato (they slide, with Portamento Auto);
// Klang plays its starter kit's pattern A (sounds it makes itself) on channel 11.
//
//   krate-render [--out file.wav] [--bars N] [--tempo BPM] [--rate HZ]
//                [--korrode LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]
//                [--kessel LEVEL] [--kessel-pattern N] [--kalliope LEVEL] [--krimson LEVEL]
//                [--kobalt LEVEL] [--klang LEVEL]
//
// Song mode (docs/SONG_PLAYER.md) plays a .gp tab file or a .mid file through
// all the instruments, each track on the instrument it's assigned to:
//
//   krate-render --song FILE [--out file.wav] [--assign TRACK:INSTRUMENT[:SLOT]]...
//                [--start-bar N] [--bars N] [--speed X] [--click] [--rate HZ]
//
// INSTRUMENT is Korrode, Kessel, kalliope, krimson (SLOT A-D), kobalt or none. Without
// --assign, tracks are assigned by what they look like: drums to Kessel, the
// first bass to Korrode, keys and others to Kalliope and Krimson's slots, guitars
// left out (they're for you to play).

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "core/Transport.h"
#include "dsp/Engine.h"
#include "dsp/Klick.h"
#include "kessel/dsp/Engine.h"
#include "kalliope/dsp/Engine.h"
#include "kobalt/dsp/Engine.h"
#include "klang/dsp/Engine.h"
#include "klang/dsp/StarterKit.h"
#include "krimson/dsp/Engine.h"
#include "rack/Rack.h"
#include "song/Arrange.h"
#include "song/GpFile.h"
#include "song/MidiFile.h"
#include "song/SongPlayer.h"
#include "tools/WavFile.h"

namespace {

void usage()
{
    std::fprintf(stderr,
                 "usage: krate-render [--out file.wav] [--bars N] [--tempo BPM] [--rate HZ]\n"
                 "                    [--korrode LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]\n"
                 "                    [--kessel LEVEL] [--kessel-pattern 0..3] [--kalliope LEVEL]\n"
                 "                    [--krimson LEVEL] [--kobalt LEVEL] [--klang LEVEL]\n"
                 "       krate-render --song FILE.gp|FILE.mid [--out file.wav] [--assign TRACK:INSTRUMENT[:SLOT]]...\n"
                 "                    [--start-bar N] [--bars N] [--speed 0.25..2] [--click] [--rate HZ]\n"
                 "  LEVEL is mixer gain, 1 = unity; INSTRUMENT is korrode, kessel, kalliope, krimson (SLOT A-D), kobalt or none\n");
}

// --- Song mode ------------------------------------------------------------------

struct SongOptions
{
    std::string file, out = "song.wav";
    std::vector<std::string> assigns; // TRACK:INSTRUMENT[:SLOT]
    int startBar = 1;
    int bars = 0; // 0 = to the end
    double speed = 1.0;
    bool click = false;
    int rate = 48000;
};

int renderSong(const SongOptions &o)
{
    namespace s = krate::song;
    std::unique_ptr<s::Song> song;
    std::vector<const s::ScoreTrack *> scoreTracks;
    std::optional<s::Score> score;
    const bool isMidi = o.file.size() > 4
                        && (o.file.substr(o.file.size() - 4) == ".mid" || o.file.substr(o.file.size() - 5) == ".midi");
    if (isMidi) {
        auto result = s::readMidiFile(o.file);
        if (!result.song) {
            std::fprintf(stderr, "krate-render: %s\n", result.error.c_str());
            return 1;
        }
        song = std::make_unique<s::Song>(std::move(*result.song));
    } else {
        auto result = s::readGpFile(o.file);
        if (!result.song) {
            std::fprintf(stderr, "krate-render: %s\n", result.error.c_str());
            return 1;
        }
        song = std::make_unique<s::Song>(std::move(*result.song));
        score = std::move(result.score);
    }
    if (score)
        for (const auto &t : score->tracks)
            scoreTracks.push_back(&t);

    // The rack: every instrument, sequencers quiet so only the song plays them.
    constexpr std::uint32_t kBlock = 64;
    korrode::Engine bass;
    klick::Klick metronome;
    kessel::Engine drums;
    kalliope::Engine poly;
    krimson::Engine lead;
    kobalt::Engine mono;
    bass.setSequencerEnabled(false);
    drums.setPattern(kessel::Pattern{});
    mono.setSequencerEnabled(false);
    krate::Rack rack(kBlock);
    enum Device { Korrode, Klick, Kessel, Kalliope, Krimson, Kobalt };
    rack.addDevice(bass, 0);
    rack.addDevice(metronome);
    rack.addDevice(drums, 9);
    rack.addDevice(poly, 1);
    rack.addDevice(lead, 2);
    rack.addDevice(mono, 6);
    rack.prepare(o.rate);
    rack.setMute(Klick, !o.click);
    rack.setLevel(Kessel, 0.8f);

    // Assignments.
    s::SongPlayer player;
    const int trackCount = int(std::min<std::size_t>(song->tracks.size(), s::SongPlayer::kMaxTracks));
    std::vector<std::string> names(std::size_t(trackCount), "none");
    const auto assign = [&](int t, const std::string &instrument, int slot) {
        if (instrument == "korrode")
            player.setAssignment(t, {Korrode, 0});
        else if (instrument == "kessel")
            player.setAssignment(t, {Kessel, 9, true});
        else if (instrument == "kalliope")
            player.setAssignment(t, {Kalliope, 1});
        else if (instrument == "krimson")
            player.setAssignment(t, {Krimson, 2 + std::clamp(slot, 0, 3)});
        else if (instrument == "kobalt")
            player.setAssignment(t, {Kobalt, 6});
        else
            return false;
        names[std::size_t(t)] = instrument + (instrument == "krimson" ? std::string(" ") + char('A' + slot) : "");
        return true;
    };
    if (o.assigns.empty()) { // polysynth parts: Kalliope, then Krimson A-D
        const auto suggested = s::suggestInstruments(*song, scoreTracks);
        for (int t = 0; t < trackCount; ++t) {
            const s::Suggestion &g = suggested[std::size_t(t)];
            if (g.kind == s::Suggestion::Kind::Drums)
                assign(t, "kessel", 0);
            else if (g.kind == s::Suggestion::Kind::Bass)
                assign(t, "korrode", 0);
            else if (g.kind == s::Suggestion::Kind::Poly && g.part < 5)
                assign(t, g.part == 0 ? "kalliope" : "krimson", g.part - 1);
        }
    } else {
        for (const std::string &a : o.assigns) {
            const std::size_t colon = a.find(':');
            const int t = std::atoi(a.substr(0, colon).c_str());
            std::string instrument = colon == std::string::npos ? "" : a.substr(colon + 1);
            int slot = 0;
            if (const std::size_t c2 = instrument.find(':'); c2 != std::string::npos) {
                slot = std::toupper(static_cast<unsigned char>(instrument[c2 + 1])) - 'A';
                instrument = instrument.substr(0, c2);
            }
            if (t < 0 || t >= trackCount || (instrument != "none" && !assign(t, instrument, slot))) {
                std::fprintf(stderr, "krate-render: bad --assign %s\n", a.c_str());
                return 2;
            }
        }
    }
    for (int t = 0; t < trackCount; ++t) {
        const s::Track &track = song->tracks[std::size_t(t)];
        std::printf("  track %2d %-10s %5d notes  -> %s  (%s)\n", t,
                    s::roleName(s::guessRole(track, t < int(scoreTracks.size()) ? scoreTracks[std::size_t(t)] : nullptr)),
                    track.noteCount, names[std::size_t(t)].c_str(), track.name.c_str());
    }

    // From the start bar for N bars (or to the end), then a tail to ring out.
    const s::Tick start = song->tempo.barStart(std::max(0, o.startBar - 1));
    const s::Tick end = o.bars > 0 ? std::min(song->length, song->tempo.barStart(o.startBar - 1 + o.bars)) : song->length;
    const double seconds = (song->tempo.secondsAt(end) - song->tempo.secondsAt(start)) / o.speed;
    const std::string title = song->title;
    player.setSpeed(o.speed);
    player.load(std::move(song));
    player.cue(start);

    krate::Transport transport;
    std::vector<float> l(kBlock), r(kBlock), interleaved;
    std::array<krate::DeviceEvent, 4096> events{};
    const auto total = std::uint64_t((seconds + 1.5) * o.rate);
    const auto playFrames = std::uint64_t(seconds * o.rate);
    interleaved.reserve(total * 2 + 2 * kBlock);
    for (std::uint64_t done = 0; done < total; done += kBlock) {
        transport.setPlaying(done < playFrames);
        player.beforeTransport(transport);
        const krate::TransportState state = transport.advance(kBlock, o.rate);
        krate::RackBlock block;
        block.frames = kBlock;
        block.transport = &state;
        block.deviceMidi = events.data();
        block.deviceMidiCount = player.render(state, kBlock, o.rate, events.data(), std::uint32_t(events.size()));
        block.masterL = l.data();
        block.masterR = r.data();
        rack.process(block);
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            interleaved.push_back(l[i]);
            interleaved.push_back(r[i]);
        }
    }
    if (!krate::writeStereoWav(o.out, interleaved, o.rate)) {
        std::fprintf(stderr, "krate-render: could not write %s\n", o.out.c_str());
        return 1;
    }
    std::printf("wrote %s: \"%s\" from bar %d, %.1f s at %.0f%% speed\n", o.out.c_str(), title.c_str(), o.startBar,
                seconds, o.speed * 100.0);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    // Song mode has its own options.
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--song") {
            SongOptions o;
            for (int j = 1; j < argc; ++j) {
                const std::string arg = argv[j];
                const bool hasValue = j + 1 < argc;
                if (arg == "--song" && hasValue) o.file = argv[++j];
                else if (arg == "--out" && hasValue) o.out = argv[++j];
                else if (arg == "--assign" && hasValue) o.assigns.push_back(argv[++j]);
                else if (arg == "--start-bar" && hasValue) o.startBar = std::max(1, std::atoi(argv[++j]));
                else if (arg == "--bars" && hasValue) o.bars = std::max(0, std::atoi(argv[++j]));
                else if (arg == "--speed" && hasValue) o.speed = std::clamp(std::atof(argv[++j]), 0.25, 2.0);
                else if (arg == "--rate" && hasValue) o.rate = std::atoi(argv[++j]);
                else if (arg == "--click") o.click = true;
                else {
                    usage();
                    return 2;
                }
            }
            if (o.file.empty() || o.rate < 8000) {
                usage();
                return 2;
            }
            return renderSong(o);
        }

    std::string out = "krate.wav";
    int bars = 4;
    double bpm = 120.0;
    int rate = 48000;
    float korrodeLevel = 1.0f;
    float klickLevel = 0.7f;
    bool muteKlick = false;
    bool soloKlick = false;
    float kesselLevel = -1.0f; // < 0: Kessel not in the rack
    int kesselPattern = 0;
    float kalliopeLevel = -1.0f; // < 0: Kalliope not in the rack
    float krimsonLevel = -1.0f;  // < 0: Krimson not in the rack
    float kobaltLevel = -1.0f;   // < 0: Kobalt not in the rack
    float klangLevel = -1.0f;    // < 0: Klang not in the rack

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--out" && hasValue) out = argv[++i];
        else if (arg == "--bars" && hasValue) bars = std::atoi(argv[++i]);
        else if (arg == "--tempo" && hasValue) bpm = std::atof(argv[++i]);
        else if (arg == "--rate" && hasValue) rate = std::atoi(argv[++i]);
        else if (arg == "--korrode" && hasValue) korrodeLevel = float(std::atof(argv[++i]));
        else if (arg == "--klick" && hasValue) klickLevel = float(std::atof(argv[++i]));
        else if (arg == "--mute-klick") muteKlick = true;
        else if (arg == "--solo-klick") soloKlick = true;
        else if (arg == "--kessel" && hasValue) kesselLevel = float(std::atof(argv[++i]));
        else if (arg == "--kessel-pattern" && hasValue) kesselPattern = std::atoi(argv[++i]);
        else if (arg == "--kalliope" && hasValue) kalliopeLevel = float(std::atof(argv[++i]));
        else if (arg == "--krimson" && hasValue) krimsonLevel = float(std::atof(argv[++i]));
        else if (arg == "--kobalt" && hasValue) kobaltLevel = float(std::atof(argv[++i]));
        else if (arg == "--klang" && hasValue) klangLevel = float(std::atof(argv[++i]));
        else {
            usage();
            return 2;
        }
    }
    if (bars < 1 || rate < 8000 || bpm < 20.0 || bpm > 400.0 || kesselPattern < 0
        || kesselPattern >= kessel::kFactoryPatternCount) {
        usage();
        return 2;
    }

    constexpr std::uint32_t kBlock = 64;
    korrode::Engine bass;
    klick::Klick metronome;
    kessel::Engine drums;
    kalliope::Engine poly;
    krimson::Engine lead;
    kobalt::Engine mono;
    klang::Engine sampler;
    const std::vector<klang::StarterSound> kit = klangLevel >= 0.0f ? klang::starterSounds() : std::vector<klang::StarterSound>{};
    krate::Rack rack(kBlock);
    const int bassIndex = rack.addDevice(bass, /*midiChannel*/ 0);
    const int klickIndex = rack.addDevice(metronome);
    int drumsIndex = -1;
    if (kesselLevel >= 0.0f) {
        drums.setPattern(kessel::factoryPattern(kesselPattern));
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
    int monoIndex = -1;
    if (kobaltLevel >= 0.0f) {
        namespace kp = kobalt::positions;
        using kobalt::ParamId;
        mono.setParam(ParamId::PlayMode, kobalt::position(2, kp::kPlayMode)); // the sequencer
        mono.setParam(ParamId::VcoRange, kobalt::position(0, kp::kVcoRange));
        mono.setParam(ParamId::Cutoff, 0.25f);
        mono.setParam(ParamId::Res, 0.7f);
        mono.setParam(ParamId::VcfEnv, 0.5f);
        mono.setParam(ParamId::EnvD, 0.6f); // 250 ms
        mono.setParam(ParamId::EnvS, 0.0f);
        mono.setParam(ParamId::EnvTrig, kobalt::position(1, kp::kEnvTrig));     // Gate: legato doesn't pluck
        mono.setParam(ParamId::PortaMode, kobalt::position(1, kp::kPortaMode)); // Auto: legato slides
        mono.setParam(ParamId::PortaTime, 0.3f);
        // C minor: notes, ties (-1) and rests (0); + marks a legato step.
        kobalt::SeqPattern line;
        const int notes[16] = {36, 36, 48, 0, 36, 39, 41, -1, 36, 0, 48, 46, 43, -1, 41, 39};
        const bool legato[16] = {false, true, false, false, false, true, false, false,
                                 false, false, false, true, false, false, false, false};
        for (int i = 0; i < 16; ++i) {
            kobalt::SeqStep &step = line.steps[std::size_t(i)];
            step.kind = notes[i] > 0 ? kobalt::SeqStep::Kind::Note
                      : notes[i] < 0 ? kobalt::SeqStep::Kind::Tie : kobalt::SeqStep::Kind::Rest;
            step.note = std::uint8_t(std::max(notes[i], 0));
            step.legato = legato[i];
        }
        mono.setSequence(line);
        monoIndex = rack.addDevice(mono, /*midiChannel*/ 6);
    }
    int samplerIndex = -1;
    if (klangLevel >= 0.0f) {
        for (int pad = 0; pad < klang::Engine::kPads; ++pad) {
            sampler.setSample(pad, &kit[std::size_t(pad)].sample);
            klang::applyStarterPad(sampler, pad);
        }
        sampler.setPattern(0, klang::starterPattern(0));
        samplerIndex = rack.addDevice(sampler, /*midiChannel*/ 10);
    }
    rack.prepare(rate);
    if (samplerIndex >= 0)
        rack.setLevel(samplerIndex, klangLevel);
    if (monoIndex >= 0)
        rack.setLevel(monoIndex, kobaltLevel);
    if (leadIndex >= 0)
        rack.setLevel(leadIndex, krimsonLevel);
    if (drumsIndex >= 0)
        rack.setLevel(drumsIndex, kesselLevel);
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
    rack.setLevel(bassIndex, korrodeLevel);
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
    std::printf("wrote %s: korrode + Klick%s%s%s%s%s through the rack, %d bars at %.0f BPM, %.1f s\n", out.c_str(),
                kesselLevel >= 0.0f ? " + Kessel" : "", kalliopeLevel >= 0.0f ? " + Kalliope" : "",
                krimsonLevel >= 0.0f ? " + Krimson" : "", kobaltLevel >= 0.0f ? " + Kobalt" : "", klangLevel >= 0.0f ? " + Klang" : "", bars, bpm,
                double(frames) / rate);
    return 0;
}
