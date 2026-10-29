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
//
// Song mode (docs/SONG_PLAYER.md) plays a .gp tab file or a .mid file through
// all the instruments, each track on the instrument it's assigned to:
//
//   krate-render --song FILE [--out file.wav] [--assign TRACK:INSTRUMENT[:SLOT]]...
//                [--start-bar N] [--bars N] [--speed X] [--click] [--rate HZ]
//
// INSTRUMENT is k303, k909, kalliope, krimson (SLOT A-D) or none. Without
// --assign, tracks are assigned by what they look like: drums to K909, the
// first bass to k303, keys and others to Kalliope and Krimson's slots, guitars
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
#include "k909/dsp/Engine.h"
#include "kalliope/dsp/Engine.h"
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
                 "                    [--k303 LEVEL] [--klick LEVEL] [--mute-klick] [--solo-klick]\n"
                 "                    [--k909 LEVEL] [--k909-pattern 0..3] [--kalliope LEVEL]\n"
                 "                    [--krimson LEVEL]\n"
                 "       krate-render --song FILE.gp|FILE.mid [--out file.wav] [--assign TRACK:INSTRUMENT[:SLOT]]...\n"
                 "                    [--start-bar N] [--bars N] [--speed 0.25..2] [--click] [--rate HZ]\n"
                 "  LEVEL is mixer gain, 1 = unity; INSTRUMENT is k303, k909, kalliope, krimson (SLOT A-D) or none\n");
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
    k303::Engine bass;
    klick::Klick metronome;
    k909::Engine drums;
    kalliope::Engine poly;
    krimson::Engine lead;
    bass.setSequencerEnabled(false);
    drums.setPattern(k909::Pattern{});
    krate::Rack rack(kBlock);
    enum Device { K303, Klick, K909, Kalliope, Krimson };
    rack.addDevice(bass, 0);
    rack.addDevice(metronome);
    rack.addDevice(drums, 9);
    rack.addDevice(poly, 1);
    rack.addDevice(lead, 2);
    rack.prepare(o.rate);
    rack.setMute(Klick, !o.click);
    rack.setLevel(K909, 0.8f);

    // Assignments.
    s::SongPlayer player;
    const int trackCount = int(std::min<std::size_t>(song->tracks.size(), s::SongPlayer::kMaxTracks));
    std::vector<std::string> names(std::size_t(trackCount), "none");
    const auto assign = [&](int t, const std::string &instrument, int slot) {
        if (instrument == "k303")
            player.setAssignment(t, {K303, 0});
        else if (instrument == "k909")
            player.setAssignment(t, {K909, 9, true});
        else if (instrument == "kalliope")
            player.setAssignment(t, {Kalliope, 1});
        else if (instrument == "krimson")
            player.setAssignment(t, {Krimson, 2 + std::clamp(slot, 0, 3)});
        else
            return false;
        names[std::size_t(t)] = instrument + (instrument == "krimson" ? std::string(" ") + char('A' + slot) : "");
        return true;
    };
    if (o.assigns.empty()) {
        bool bassTaken = false;
        int nextKeys = 0; // Kalliope, then Krimson A-D
        for (int t = 0; t < trackCount; ++t) {
            const s::Track &track = song->tracks[std::size_t(t)];
            if (track.noteCount == 0)
                continue;
            const s::TrackRole role = s::guessRole(track, t < int(scoreTracks.size()) ? scoreTracks[std::size_t(t)] : nullptr);
            if (role == s::TrackRole::Drums)
                assign(t, "k909", 0);
            else if (role == s::TrackRole::Bass && !bassTaken)
                bassTaken = assign(t, "k303", 0);
            else if (role != s::TrackRole::Guitar && nextKeys < 5) {
                assign(t, nextKeys == 0 ? "kalliope" : "krimson", nextKeys - 1);
                ++nextKeys;
            }
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
