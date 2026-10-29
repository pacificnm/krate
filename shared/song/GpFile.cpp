#include "song/GpFile.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <numeric>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "song/Xml.h"
#include "song/Zip.h"

namespace krate::song {

namespace {

using xml::Node;

constexpr std::size_t kMaxFileBytes = std::size_t(64) << 20;
constexpr int kPpq = 960;
constexpr int kMaxRepeat = 16;
constexpr int kMaxVoices = 4;

int toInt(std::string_view s, int fallback = 0)
{
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    int v = fallback;
    if (std::from_chars(s.data(), s.data() + s.size(), v).ec != std::errc())
        return fallback;
    return v;
}

double toDouble(std::string_view s, double fallback = 0.0)
{
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    double v = fallback;
    if (std::from_chars(s.data(), s.data() + s.size(), v).ec != std::errc())
        return fallback;
    return v;
}

std::vector<std::string_view> words(std::string_view s)
{
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r'))
            ++i;
        const std::size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\n' && s[i] != '\t' && s[i] != '\r')
            ++i;
        if (i > start)
            out.push_back(s.substr(start, i - start));
    }
    return out;
}

// A <Property name="..."> under the node's <Properties>.
const Node *property(const Node &node, std::string_view name)
{
    if (const Node *props = node.child("Properties"))
        for (const Node &p : props->children)
            if (p.attribute("name") == name)
                return &p;
    return nullptr;
}

// Elements of a collection (<Notes>, <Beats> ...) by their id attribute.
std::unordered_map<std::string_view, const Node *> byId(const Node *collection)
{
    std::unordered_map<std::string_view, const Node *> out;
    if (collection)
        for (const Node &c : collection->children)
            out.emplace(c.attribute("id"), &c);
    return out;
}

int noteValue(std::string_view name)
{
    if (name == "Whole") return 1;
    if (name == "Half") return 2;
    if (name == "Quarter") return 4;
    if (name == "Eighth") return 8;
    if (name == "16th") return 16;
    if (name == "32nd") return 32;
    if (name == "64th") return 64;
    if (name == "128th") return 128;
    return 4;
}

int dynamicVelocity(std::string_view d)
{
    if (d == "PPP") return 15;
    if (d == "PP") return 31;
    if (d == "P") return 47;
    if (d == "MP") return 63;
    if (d == "MF") return 79;
    if (d == "FF") return 111;
    if (d == "FFF") return 127;
    return 95; // F, and the default
}

// "131 2": the tempo and its beat unit (1 eighth, 2 quarter, 3 dotted quarter,
// 4 half, 5 dotted half), as quarter notes per minute.
double tempoValue(std::string_view value)
{
    const auto w = words(value);
    if (w.empty())
        return 0.0;
    const double bpm = toDouble(w[0]);
    static constexpr double kUnitQuarters[] = {1.0, 0.5, 1.0, 1.5, 2.0, 3.0};
    const int unit = w.size() > 1 ? toInt(w[1], 2) : 2;
    return bpm * (unit >= 0 && unit <= 5 ? kUnitQuarters[unit] : 1.0);
}

struct Rhythm
{
    int value = 4, dots = 0, tupletNum = 0, tupletDen = 0;
    Tick length = kPpq;
};

Rhythm readRhythm(const Node *r)
{
    Rhythm out;
    if (!r)
        return out;
    out.value = noteValue(r->childText("NoteValue"));
    double length = double(kPpq) * 4.0 / out.value;
    if (const Node *dot = r->child("AugmentationDot")) {
        out.dots = std::clamp(toInt(dot->attribute("count")), 0, 3);
        double add = length;
        for (int i = 0; i < out.dots; ++i) {
            add /= 2.0;
            length += add;
        }
    }
    if (const Node *t = r->child("PrimaryTuplet")) {
        out.tupletNum = toInt(t->attribute("num"));
        out.tupletDen = toInt(t->attribute("den"));
        if (out.tupletNum > 0 && out.tupletDen > 0)
            length = length * out.tupletDen / out.tupletNum;
        else
            out.tupletNum = out.tupletDen = 0;
    }
    out.length = std::max<Tick>(1, Tick(std::llround(length)));
    return out;
}

// The interval a harmonic sounds above the string length it divides: the
// fret marks a node at k/n of the string, so it sounds harmonic n
// (12 -> an octave, 7 -> octave + fifth, 5 -> two octaves, 4 -> two octaves + third).
int harmonicInterval(double fret)
{
    if (fret <= 0.0)
        return 0;
    const double x = 1.0 - std::pow(2.0, -fret / 12.0); // where on the string
    for (int n = 2; n <= 8; ++n)
        for (int k = 1; k < n; ++k)
            if (std::gcd(k, n) == 1 && std::abs(x - double(k) / n) < 0.012)
                return int(std::lround(12.0 * std::log2(double(n))));
    return 12;
}

ScoreNote readNote(const Node &n, const ScoreTrack &track)
{
    ScoreNote note;
    if (const Node *p = property(n, "String"))
        note.string = toInt(p->childText("String"));
    if (const Node *p = property(n, "Fret"))
        note.fret = toInt(p->childText("Fret"));
    if (const Node *p = property(n, "Midi"))
        note.pitch = toInt(p->childText("Number"), -1);
    else
        note.pitch = -1;
    if (note.pitch < 0) { // older files: from the tuning
        const int open = note.string >= 0 && note.string < int(track.tuning.size()) ? track.tuning[std::size_t(note.string)] : 40;
        note.pitch = open + track.capo + note.fret;
    }
    if (const Node *type = property(n, "HarmonicType")) {
        // Natural harmonics sound from the open string; the others (artificial,
        // pinch, tapped...) above the fretted note, by the harmonic fret's interval.
        const Node *hf = property(n, "HarmonicFret");
        const double at = hf ? toDouble(hf->childText("HFret"), 12.0) : double(note.fret);
        std::string kind(type->childText("HType"));
        std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (kind == "natural") {
            const int open = note.string >= 0 && note.string < int(track.tuning.size()) ? track.tuning[std::size_t(note.string)] + track.capo : note.pitch - note.fret;
            note.pitch = open + harmonicInterval(at);
        } else {
            note.pitch += harmonicInterval(at);
        }
    }
    note.pitch = std::clamp(note.pitch, 0, 127);
    if (const Node *tie = n.child("Tie"); tie && tie->attribute("destination") == "true")
        note.techniques |= TieDestination;
    if (property(n, "HopoOrigin"))
        note.techniques |= HammerOn;
    if (property(n, "HopoDestination"))
        note.techniques |= HammerTarget;
    if (property(n, "Slide"))
        note.techniques |= Slide;
    if (property(n, "Muted"))
        note.techniques |= DeadNote;
    if (property(n, "PalmMuted"))
        note.techniques |= PalmMute;
    if (property(n, "HarmonicType") || property(n, "Harmonic"))
        note.techniques |= Harmonic;
    if (n.child("Vibrato"))
        note.techniques |= Vibrato;
    if (n.child("Accent"))
        note.techniques |= Accent;
    if (n.child("AntiAccent"))
        note.techniques |= GhostNote;
    if (n.child("LetRing"))
        note.techniques |= LetRing;
    if (property(n, "Bended")) {
        note.techniques |= Bend;
        // Values: 25 per quarter tone; offsets: percent of the note.
        const auto point = [&](const char *offsetName, double defaultOffset, const char *valueName) -> std::optional<BendPoint> {
            const Node *v = property(n, valueName);
            if (!v)
                return std::nullopt;
            const Node *o = property(n, offsetName);
            return BendPoint{std::clamp((o ? toDouble(o->childText("Float"), defaultOffset) : defaultOffset) / 100.0, 0.0, 1.0),
                             toDouble(v->childText("Float")) / 50.0};
        };
        for (const auto &p : {point("BendOriginOffset", 0.0, "BendOriginValue"),
                              point("BendMiddleOffset1", 50.0, "BendMiddleValue"),
                              point("BendDestinationOffset", 100.0, "BendDestinationValue")})
            if (p)
                note.bend.push_back(*p);
        std::stable_sort(note.bend.begin(), note.bend.end(),
                         [](const BendPoint &a, const BendPoint &b) { return a.offset < b.offset; });
    }
    return note;
}

// --- Song events -----------------------------------------------------------------

// Events at the same tick: note-offs first, then bends and controllers, then note-ons.
struct Pending
{
    Event event;
    int order = 0;
};

int channelFor(const ScoreTrack &track, int string)
{
    if (track.drums)
        return kDrumChannel;
    const int c = std::clamp(string, 0, 14);
    return c >= kDrumChannel ? c + 1 : c;
}

std::uint16_t bendValue(double semitones) // for a ±2 semitone range
{
    return std::uint16_t(std::clamp(8192.0 + semitones / 2.0 * 8191.0, 0.0, 16383.0));
}

Track trackEvents(const Score &score, const ScoreTrack &st, Tick songEnd)
{
    Track track;
    track.name = st.name;
    track.instrument = st.instrument;
    track.program = st.program;
    std::vector<Pending> events;

    // A note being held on a string, until its end is known (ties extend it).
    struct Held
    {
        bool active = false;
        int pitch = 0, channel = 0, velocity = 0;
        Tick on = 0, off = 0;
        bool letRing = false;
    };
    std::map<int, Held> held; // voice * 64 + string
    const auto flush = [&](Held &h, Tick nextStart) {
        if (!h.active)
            return;
        const Tick off = h.letRing ? std::max(h.off, nextStart) : h.off;
        events.push_back({{h.on, std::uint8_t(0x90 | h.channel), std::uint8_t(h.pitch), std::uint8_t(h.velocity)}, 2});
        events.push_back({{std::max(off, h.on + 1), std::uint8_t(0x80 | h.channel), std::uint8_t(h.pitch), 0}, 0});
        h.active = false;
    };

    for (std::size_t k = 0; k < score.playOrder.size(); ++k) {
        const ScoreBar &bar = st.bars[std::size_t(score.playOrder[k])];
        const Tick barStart = score.playStarts[k];
        for (std::size_t v = 0; v < bar.voices.size(); ++v) {
            for (const ScoreBeat &beat : bar.voices[v]) {
                const Tick start = barStart + beat.start;
                for (const ScoreNote &note : beat.notes) {
                    Held &h = held[int(v) * 64 + std::clamp(note.string, 0, 63)];
                    const int channel = channelFor(st, note.string);
                    if ((note.techniques & TieDestination) && h.active) {
                        // The note on this string, held on (a tied harmonic isn't marked as one again).
                        h.off = start + beat.length;
                        continue;
                    }
                    flush(h, start);
                    int velocity = beat.velocity;
                    if (note.techniques & Accent)
                        velocity += 16;
                    if (note.techniques & (GhostNote | DeadNote))
                        velocity = velocity * 3 / 5;
                    Tick length = beat.length;
                    if (note.techniques & DeadNote)
                        length = std::min<Tick>(length, score.ppq / 8);
                    h = {true, note.pitch, channel, std::clamp(velocity, 1, 127), start, start + length,
                         (note.techniques & LetRing) != 0};
                    if (!note.bend.empty() && !st.drums) {
                        for (const BendPoint &p : note.bend) {
                            const std::uint16_t value = bendValue(p.semitones);
                            events.push_back({{start + Tick(std::llround(p.offset * double(beat.length))),
                                               std::uint8_t(0xE0 | channel), std::uint8_t(value & 0x7F),
                                               std::uint8_t(value >> 7)},
                                              1});
                        }
                        const std::uint16_t centre = bendValue(0.0);
                        events.push_back({{start + beat.length, std::uint8_t(0xE0 | channel), std::uint8_t(centre & 0x7F),
                                           std::uint8_t(centre >> 7)},
                                          1});
                    }
                }
            }
        }
    }
    for (auto &[key, h] : held)
        flush(h, h.off);
    // In time; at the same tick offs, then bends, then ons, each by channel (string), then pitch.
    std::stable_sort(events.begin(), events.end(), [](const Pending &a, const Pending &b) {
        if (a.event.tick != b.event.tick)
            return a.event.tick < b.event.tick;
        if (a.order != b.order)
            return a.order < b.order;
        if (a.event.channel() != b.event.channel())
            return a.event.channel() < b.event.channel();
        return a.event.data1 < b.event.data1;
    });
    track.events.reserve(events.size());
    for (const Pending &p : events)
        track.events.push_back(p.event);
    finishTrack(track, songEnd);
    if (st.drums) // a drum kit is drums even if a file sets it up oddly
        track.drums = true;
    return track;
}

} // namespace

std::vector<int> playOrder(const std::vector<MasterBar> &bars)
{
    std::vector<int> order;
    const int n = int(bars.size());
    int start = 0; // the current repeat section's first bar
    int pass = 1;  // which time through it
    std::size_t limit = std::size_t(n) * kMaxRepeat + 16;
    for (int i = 0; i < n && order.size() < limit;) {
        const MasterBar &bar = bars[std::size_t(i)];
        if (bar.repeatStart && i != start) {
            start = i;
            pass = 1;
        }
        if (bar.endings != 0 && !(pass <= 32 && (bar.endings & (1u << (pass - 1))))) {
            ++i; // an alternate ending for another pass
            continue;
        }
        order.push_back(i);
        if (bar.repeatEnd) {
            const int count = std::clamp(bar.repeatCount, 2, kMaxRepeat);
            if (pass < count) {
                ++pass;
                i = start;
                continue;
            }
            pass = 1;
            start = i + 1; // a later repeat end without a start goes back to here
        }
        ++i;
    }
    return order;
}

GpFileResult readGpFile(std::span<const std::uint8_t> data)
{
    ZipArchive zip;
    if (!zip.open(data))
        return {std::nullopt, std::nullopt, "not a .gp tab file (older tab formats aren't read yet)"};
    const ZipEntry *entry = zip.find("Content/score.gpif");
    if (!entry)
        return {std::nullopt, std::nullopt, "the tab file has no score in it"};
    const auto bytes = zip.read(*entry);
    if (!bytes)
        return {std::nullopt, std::nullopt, "the tab file's score is damaged"};
    const xml::Document doc = xml::parse(std::string_view(reinterpret_cast<const char *>(bytes->data()), bytes->size()));
    if (!doc.ok())
        return {std::nullopt, std::nullopt, "the tab file's score can't be read: " + doc.error};
    const Node &gp = doc.root;
    if (gp.name != "GPIF")
        return {std::nullopt, std::nullopt, "the tab file's score isn't in the expected format"};

    Score score;
    score.ppq = kPpq;
    if (const Node *s = gp.child("Score")) {
        score.title = std::string(s->childText("Title"));
        score.artist = std::string(s->childText("Artist"));
        score.album = std::string(s->childText("Album"));
    }

    // Tracks.
    if (const Node *tracks = gp.child("Tracks"))
        for (const Node &t : tracks->children) {
            ScoreTrack track;
            track.name = std::string(t.childText("Name"));
            std::string_view type;
            if (const Node *set = t.child("InstrumentSet")) {
                track.instrument = std::string(set->childText("Name"));
                type = set->childText("Type");
            }
            track.drums = type == "drumKit";
            track.type = std::string(type);
            std::vector<const Node *> places{&t};
            if (const Node *staves = t.child("Staves"))
                for (const Node &s : staves->children)
                    places.push_back(&s);
            for (const Node *place : places) {
                if (const Node *p = property(*place, "Tuning"); p && track.tuning.empty())
                    for (std::string_view w : words(p->childText("Pitches")))
                        track.tuning.push_back(std::clamp(toInt(w), 0, 127));
                if (const Node *p = property(*place, "CapoFret"))
                    track.capo = std::clamp(toInt(p->childText("Fret")), 0, 24);
            }
            if (track.drums)
                track.tuning.clear();
            if (const Node *sounds = t.child("Sounds"); sounds && !sounds->children.empty())
                if (const Node *midi = sounds->children.front().child("MIDI"))
                    track.program = toInt(midi->childText("Program"), -1);
            score.tracks.push_back(std::move(track));
        }
    if (score.tracks.empty())
        return {std::nullopt, std::nullopt, "the tab file has no tracks"};

    // Master bars.
    const Node *masterBars = gp.child("MasterBars");
    if (!masterBars || masterBars->children.empty())
        return {std::nullopt, std::nullopt, "the tab file has no bars"};
    TimeSignature time{0, 4, 4};
    Tick written = 0;
    std::vector<std::vector<std::string_view>> barIds; // per master bar, per track
    for (const Node &mb : masterBars->children) {
        MasterBar bar;
        if (const auto t = mb.childText("Time"); !t.empty()) {
            const std::size_t slash = t.find('/');
            const int num = toInt(t.substr(0, slash), 4);
            const int den = slash == std::string_view::npos ? 4 : toInt(t.substr(slash + 1), 4);
            if (num >= 1 && num <= 64 && den >= 1 && den <= 64 && (den & (den - 1)) == 0) {
                time.numerator = num;
                time.denominator = den;
            }
        }
        time.tick = written;
        bar.time = time;
        if (const Node *rep = mb.child("Repeat")) {
            bar.repeatStart = rep->attribute("start") == "true";
            bar.repeatEnd = rep->attribute("end") == "true";
            bar.repeatCount = toInt(rep->attribute("count"), 2);
        }
        for (std::string_view w : words(mb.childText("AlternateEndings")))
            if (const int pass = toInt(w); pass >= 1 && pass <= 32)
                bar.endings |= 1u << (pass - 1);
        written += Tick(kPpq) * 4 * time.numerator / time.denominator;
        barIds.push_back(words(mb.childText("Bars")));
        score.bars.push_back(bar);
    }

    // Tempo marks.
    if (const Node *mt = gp.child("MasterTrack"))
        if (const Node *autos = mt->child("Automations"))
            for (const Node &a : autos->children)
                if (a.childText("Type") == "Tempo")
                    if (const double bpm = tempoValue(a.childText("Value")); bpm > 0.0)
                        score.tempos.push_back({std::clamp(toInt(a.childText("Bar")), 0, int(score.bars.size()) - 1),
                                                std::clamp(toDouble(a.childText("Position")), 0.0, 1.0), bpm});
    std::stable_sort(score.tempos.begin(), score.tempos.end(), [](const TempoMark &a, const TempoMark &b) {
        return a.bar != b.bar ? a.bar < b.bar : a.position < b.position;
    });

    // Each track's bars: voices of beats of notes, shared by id.
    const auto bars = byId(gp.child("Bars"));
    const auto voices = byId(gp.child("Voices"));
    const auto beats = byId(gp.child("Beats"));
    const auto notes = byId(gp.child("Notes"));
    const auto rhythms = byId(gp.child("Rhythms"));
    std::unordered_map<std::string_view, ScoreNote> noteCache;
    for (std::size_t t = 0; t < score.tracks.size(); ++t) {
        ScoreTrack &track = score.tracks[t];
        noteCache.clear(); // notes depend on the track (tuning fallback)
        track.bars.resize(score.bars.size());
        for (std::size_t m = 0; m < score.bars.size(); ++m) {
            const auto found = t < barIds[m].size() ? bars.find(barIds[m][t]) : bars.end();
            if (found == bars.end())
                continue;
            ScoreBar &bar = track.bars[m];
            for (std::string_view voiceId : words(found->second->childText("Voices"))) {
                if (voiceId == "-1" || int(bar.voices.size()) >= kMaxVoices)
                    continue;
                const auto voice = voices.find(voiceId);
                if (voice == voices.end())
                    continue;
                std::vector<ScoreBeat> &out = bar.voices.emplace_back();
                Tick at = 0;
                for (std::string_view beatId : words(voice->second->childText("Beats"))) {
                    const auto b = beats.find(beatId);
                    if (b == beats.end())
                        continue;
                    const Node &beatNode = *b->second;
                    const Node *rhythmRef = beatNode.child("Rhythm");
                    const auto r = rhythmRef ? rhythms.find(rhythmRef->attribute("ref")) : rhythms.end();
                    const Rhythm rhythm = readRhythm(r != rhythms.end() ? r->second : nullptr);
                    ScoreBeat beat;
                    beat.start = at;
                    beat.length = rhythm.length;
                    beat.noteValue = rhythm.value;
                    beat.dots = rhythm.dots;
                    beat.tupletNum = rhythm.tupletNum;
                    beat.tupletDen = rhythm.tupletDen;
                    beat.velocity = dynamicVelocity(beatNode.childText("Dynamic"));
                    for (std::string_view noteId : words(beatNode.childText("Notes"))) {
                        auto cached = noteCache.find(noteId);
                        if (cached == noteCache.end()) {
                            const auto n = notes.find(noteId);
                            if (n == notes.end())
                                continue;
                            cached = noteCache.emplace(noteId, readNote(*n->second, track)).first;
                        }
                        beat.notes.push_back(cached->second);
                    }
                    // Grace notes sound briefly at the beat and take no time in the bar.
                    if (beatNode.child("GraceNotes")) {
                        beat.length = std::min<Tick>(beat.length, kPpq / 16);
                        out.push_back(std::move(beat));
                        continue;
                    }
                    at += beat.length;
                    out.push_back(std::move(beat));
                }
            }
        }
    }

    // Play order and the Song's timeline.
    score.playOrder = playOrder(score.bars);
    Song song;
    song.title = score.title;
    song.tempo = TempoMap(kPpq);
    Tick played = 0;
    double bpm = score.tempos.empty() ? 120.0 : score.tempos.front().bpm;
    TimeSignature lastTime{-1, 0, 0};
    double lastBpm = -1.0;
    for (const int m : score.playOrder) {
        const MasterBar &bar = score.bars[std::size_t(m)];
        const Tick length = Tick(kPpq) * 4 * bar.time.numerator / bar.time.denominator;
        score.playStarts.push_back(played);
        if (bar.time.numerator != lastTime.numerator || bar.time.denominator != lastTime.denominator) {
            song.tempo.addTimeSignature(played, bar.time.numerator, bar.time.denominator);
            lastTime = bar.time;
        }
        // The tempo in force at the bar's start (as written), then changes inside it.
        for (const TempoMark &mark : score.tempos)
            if (mark.bar < m || (mark.bar == m && mark.position <= 0.0))
                bpm = mark.bpm;
        if (bpm != lastBpm) {
            song.tempo.addTempo(played, bpm);
            lastBpm = bpm;
        }
        for (const TempoMark &mark : score.tempos)
            if (mark.bar == m && mark.position > 0.0) {
                song.tempo.addTempo(played + Tick(std::llround(mark.position * double(length))), mark.bpm);
                lastBpm = mark.bpm;
            }
        played += length;
    }
    song.length = played;
    for (const ScoreTrack &st : score.tracks) {
        Track track = trackEvents(score, st, played);
        if (track.noteCount > 0) // keep tracks lined up with the score's: empty ones stay, silent
            song.tracks.push_back(std::move(track));
        else
            song.tracks.push_back(Track{st.name, st.instrument, 0, st.program, st.drums, {}, 0, -1, -1});
    }
    return {std::move(score), std::move(song), {}};
}

GpFileResult readGpFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {std::nullopt, std::nullopt, "can't open " + path};
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size < 0 || std::size_t(size) > kMaxFileBytes)
        return {std::nullopt, std::nullopt, "the tab file is too large"};
    file.seekg(0);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
    if (!file)
        return {std::nullopt, std::nullopt, "can't read " + path};
    return readGpFile(std::span<const std::uint8_t>(data));
}

} // namespace krate::song
