#include "song/TabLayout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

namespace krate::song {

namespace {

using Kind = TabItem::Kind;
using Tone = TabItem::Tone;

constexpr double kPadLeft = 10.0;
constexpr double kPadRight = 8.0;
constexpr double kTimeSigWidth = 18.0;
constexpr double kRepeatWidth = 8.0;

const char *noteName(int pitch)
{
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[((pitch % 12) + 12) % 12];
}

// How wide a beat that lasts `ticks` is drawn: longer is wider, but not in proportion.
double slotWidth(Tick ticks, int ppq)
{
    return 15.0 + 24.0 * std::sqrt(std::max(0.0, double(ticks) / ppq));
}

std::string bendLabel(double semitones)
{
    // Tab names bends in whole tones: a semitone is ½, a whole tone "full".
    const double tones = semitones / 2.0;
    if (std::abs(tones - 1.0) < 0.05)
        return "full";
    const int quarters = int(std::lround(tones * 4.0));
    const int whole = quarters / 4, rest = quarters % 4;
    static const char *fractions[] = {"", "¼", "½", "¾"};
    return (whole > 0 ? std::to_string(whole) : std::string()) + fractions[rest];
}

// One bar's beat starts (all voices) and how wide its content is unjustified.
struct BarPlan
{
    int bar = 0;
    Tick length = 0;
    std::vector<Tick> ticks;
    std::vector<double> widths; // per slot (to the next start or the bar's end)
    double fixed = 0;           // pads, time signature, repeat marks
    bool timeSig = false;
    double slots() const
    {
        double w = 0;
        for (double v : widths)
            w += v;
        return w;
    }
};

// --- Notation ----------------------------------------------------------------

// A pitch as a staff step (C0 = 0, D0 = 1 ... a step per letter) and a sharp.
struct Spelled
{
    int step = 0;
    bool sharp = false;
};

Spelled spell(int pitch)
{
    static constexpr int kLetter[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static constexpr bool kSharp[12] = {false, true, false, true, false, false, true, false, true, false, true, false};
    const int pc = ((pitch % 12) + 12) % 12;
    return {(pitch / 12 - 1) * 7 + kLetter[pc], kSharp[pc]};
}

// Where a drum-kit note sits on a percussion staff (treble steps), and its head.
struct DrumPlace
{
    int step = 35;
    bool cross = false; // cymbals and hi-hats
    bool open = false;  // an open hi-hat: "o" over it
};

DrumPlace drumPlace(int note)
{
    switch (note) {
    case 35: case 36: return {31, false, false};                 // kick: F4
    case 37: return {35, true, false};                           // side stick on the snare
    case 38: case 39: case 40: return {35, false, false};        // snare, clap: C5
    case 41: case 43: return {33, false, false};                 // floor tom: A4
    case 45: case 47: return {36, false, false};                 // mid tom: D5
    case 48: case 50: return {37, false, false};                 // high tom: E5
    case 42: return {39, true, false};                           // closed hi-hat: G5
    case 46: return {39, true, true};                            // open hi-hat
    case 44: return {29, true, false};                           // pedal hi-hat: D4
    case 49: case 52: case 55: case 57: return {40, true, false}; // crash, china, splash: A5
    case 51: case 59: return {38, true, false};                  // ride: F5
    case 53: return {38, false, false};                          // ride bell
    default: return {35, false, false};
    }
}

const char *const kTrebleClef = "\U0001D11E";
const char *const kBassClef = "\U0001D122";
const char *const kPercussionClef = "\U0001D125";
const char *const kSharpSign = "\u266F";
const char *const kNaturalSign = "\u266E";

const char *restGlyph(int noteValue)
{
    switch (noteValue) {
    case 1: return "\U0001D13B";
    case 2: return "\U0001D13C";
    case 4: return "\U0001D13D";
    case 8: return "\U0001D13E";
    case 16: return "\U0001D13F";
    case 32: return "\U0001D140";
    default: return "\U0001D141";
    }
}

class Builder
{
public:
    Builder(const Score &score, const ScoreTrack &track, const TabStyle &style, bool notes)
        : m_score(score)
        , m_track(track)
        , m_style(style)
        , m_strings(int(track.tuning.size()))
        , m_notes(notes)
    {
        if (!notes)
            return;
        if (track.drums) {
            m_clef = kPercussionClef;
            m_bottom = 30;
            return;
        }
        // Guitar and bass are written an octave above how they sound.
        std::string type = track.type + " " + track.instrument;
        std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (type.find("guitar") != std::string::npos || (type.find("bass") != std::string::npos && type.find("bassoon") == std::string::npos))
            m_transpose = 12;
        // Treble or bass clef, from where most of the notes are.
        std::vector<int> pitches;
        for (const ScoreBar &bar : track.bars)
            for (const auto &voice : bar.voices)
                for (const ScoreBeat &beat : voice)
                    for (const ScoreNote &n : beat.notes)
                        pitches.push_back(n.pitch + m_transpose);
        if (!pitches.empty()) {
            std::nth_element(pitches.begin(), pitches.begin() + std::ptrdiff_t(pitches.size() / 2), pitches.end());
            if (pitches[pitches.size() / 2] < 55) {
                m_clef = kBassClef;
                m_bottom = 18; // G2
            }
        }
    }

    // Notation: the y of a staff step, and the gap between lines (or strings).
    double stepY(int step) const { return system->staffBottom - double(step - m_bottom) * m_style.noteGap / 2.0; }
    double rowGap() const { return m_notes ? m_style.noteGap : m_style.lineGap; }

    TabSystem *system = nullptr;

    double stringY(int s) const { return system->staffTop + double(m_strings - 1 - std::clamp(s, 0, m_strings - 1)) * m_style.lineGap; }

    void line(double x1, double y1, double x2, double y2, double width = 1.0, Tone tone = Tone::Ink)
    {
        TabItem i;
        i.kind = Kind::Line;
        i.x1 = x1;
        i.y1 = y1;
        i.x2 = x2;
        i.y2 = y2;
        i.width = width;
        i.tone = tone;
        system->items.push_back(std::move(i));
    }
    void text(double x, double y, std::string t, double size, bool knockout = false, bool bold = false, Tone tone = Tone::Ink)
    {
        TabItem i;
        i.kind = Kind::Text;
        i.x1 = x;
        i.y1 = y;
        i.text = std::move(t);
        i.size = size;
        i.knockout = knockout;
        i.bold = bold;
        i.tone = tone;
        system->items.push_back(std::move(i));
    }
    void glyph(double x, double y, const char *text, double size, Tone tone = Tone::Ink)
    {
        TabItem i;
        i.kind = Kind::Text;
        i.x1 = x;
        i.y1 = y;
        i.text = text;
        i.size = size;
        i.music = true;
        i.tone = tone;
        system->items.push_back(std::move(i));
    }
    void head(double x, double y, bool filled)
    {
        TabItem i;
        i.kind = Kind::Note;
        i.x1 = x;
        i.y1 = y;
        i.x2 = m_style.noteGap * 0.62;
        i.y2 = m_style.noteGap * 0.45;
        i.filled = filled;
        system->items.push_back(std::move(i));
    }
    void dot(double x, double y, double r, Tone tone = Tone::Ink)
    {
        TabItem i;
        i.kind = Kind::Dot;
        i.x1 = x;
        i.y1 = y;
        i.x2 = r;
        i.tone = tone;
        system->items.push_back(std::move(i));
    }

    // The previous note on each string (for hammer-ons, pull-offs and slides), across bars.
    struct Last
    {
        bool valid = false;
        double x = 0;
        int fret = 0;
        std::uint32_t techniques = 0;
        int system = -1;
    };
    std::map<int, Last> last; // voice * 64 + string
    int systemIndex = 0;

    void drawBar(const TabBarBox &box, const MasterBar &master, bool timeSig, int barNumber)
    {
        const double top = system->staffTop, bottom = system->staffBottom;
        // Bar number, endings, repeats.
        text(box.x0 + 8.0, top - 26.0, std::to_string(barNumber), m_style.smallSize, false, false, Tone::Faint);
        if (master.endings != 0) {
            std::string label;
            for (int p = 0; p < 32; ++p)
                if (master.endings & (1u << p))
                    label += (label.empty() ? "" : ",") + std::to_string(p + 1);
            line(box.x0 + 2, top - 18.0, box.x1 - 2, top - 18.0, 1.0);
            line(box.x0 + 2, top - 18.0, box.x0 + 2, top - 12.0, 1.0);
            text(box.x0 + 14.0, top - 26.0, label + ".", m_style.smallSize, false, true);
        }
        if (master.repeatStart) {
            line(box.x0 + 1.5, top, box.x0 + 1.5, bottom, 3.0);
            const double mid = (top + bottom) / 2.0;
            dot(box.x0 + 6.0, mid - rowGap() / 2.0, 1.8);
            dot(box.x0 + 6.0, mid + rowGap() / 2.0, 1.8);
        }
        if (master.repeatEnd) {
            line(box.x1 - 1.5, top, box.x1 - 1.5, bottom, 3.0);
            const double mid = (top + bottom) / 2.0;
            dot(box.x1 - 6.0, mid - rowGap() / 2.0, 1.8);
            dot(box.x1 - 6.0, mid + rowGap() / 2.0, 1.8);
            if (master.repeatCount > 2)
                text(box.x1 - 10.0, top - 10.0, "x" + std::to_string(master.repeatCount), m_style.smallSize, false, true);
        }
        line(box.x1, top, box.x1, bottom, 1.0); // bar line
        if (timeSig) {
            const double x = box.x0 + (master.repeatStart ? kRepeatWidth : 0.0) + 10.0;
            const double mid = (top + bottom) / 2.0;
            const double offset = m_notes ? rowGap() : rowGap() * 0.9;
            text(x, mid - offset, std::to_string(master.time.numerator), m_style.fretSize + 4, true, true);
            text(x, mid + offset, std::to_string(master.time.denominator), m_style.fretSize + 4, true, true);
        }

        const ScoreBar &bar = m_track.bars[std::size_t(box.bar)];
        const auto xAt = [&](Tick t) {
            for (std::size_t i = 0; i < box.ticks.size(); ++i)
                if (box.ticks[i] == t)
                    return box.xs[i];
            return box.contentX0 + (box.contentX1 - box.contentX0) * double(t) / double(std::max<Tick>(1, box.length));
        };
        if (m_notes) {
            drawNotes(bar, xAt);
            return;
        }
        for (std::size_t v = 0; v < bar.voices.size(); ++v) {
            const auto &beats = bar.voices[v];
            for (const ScoreBeat &beat : beats) {
                const double x = xAt(beat.start);
                bool palmMute = false, accent = false;
                double bendUp = 0.0;
                for (const ScoreNote &note : beat.notes) {
                    const double y = stringY(note.string);
                    std::string label = std::to_string(note.fret);
                    if (note.techniques & DeadNote)
                        label = "x";
                    else if (note.techniques & Harmonic)
                        label = "<" + label + ">";
                    else if (note.techniques & (TieDestination | GhostNote))
                        label = "(" + label + ")";
                    text(x, y, label, m_style.fretSize, true, false,
                         (note.techniques & TieDestination) ? Tone::Faint : Tone::Ink);
                    const double half = 3.5 * double(label.size());
                    if (note.techniques & Vibrato)
                        text(x + half + 6.0, y - 1.0, "~", m_style.fretSize, false, true);
                    palmMute = palmMute || (note.techniques & PalmMute);
                    accent = accent || (note.techniques & Accent);
                    if ((note.techniques & Bend) && !note.bend.empty()) {
                        double peak = 0.0;
                        for (const BendPoint &p : note.bend)
                            peak = std::max(peak, p.semitones);
                        bendUp = std::max(bendUp, peak);
                        // An arrow from the fret up over the staff.
                        const double x0 = x + half + 2.0, yTop = system->staffTop - 10.0;
                        line(x0, y - 3.0, x0 + 8.0, y - 8.0, 1.2, Tone::Accent);
                        line(x0 + 8.0, y - 8.0, x0 + 10.0, yTop, 1.2, Tone::Accent);
                        line(x0 + 10.0, yTop, x0 + 7.0, yTop + 4.0, 1.2, Tone::Accent);
                        line(x0 + 10.0, yTop, x0 + 13.0, yTop + 4.0, 1.2, Tone::Accent);
                        if (peak > 0.0)
                            text(x0 + 10.0, yTop - 7.0, bendLabel(peak), m_style.smallSize, false, true, Tone::Accent);
                    }
                    // A hammer-on, pull-off or slide from the previous note on this string.
                    Last &prev = last[int(v) * 64 + note.string];
                    if (prev.valid && prev.system == systemIndex && prev.x < x) {
                        if ((note.techniques & HammerTarget) && (prev.techniques & HammerOn)) {
                            const double mx = (prev.x + x) / 2.0, ya = y - 9.0;
                            line(prev.x + 3.0, y - 6.0, mx, ya - 2.0, 1.0, Tone::Faint);
                            line(mx, ya - 2.0, x - 3.0, y - 6.0, 1.0, Tone::Faint);
                            text(mx, ya - 7.0, note.fret >= prev.fret ? "h" : "p", m_style.smallSize, false, false, Tone::Faint);
                        } else if (prev.techniques & Slide) {
                            const double rise = note.fret > prev.fret ? -4.0 : note.fret < prev.fret ? 4.0 : 0.0;
                            line(prev.x + 8.0, y - rise, x - 8.0, y + rise, 1.2);
                        }
                    }
                    prev = {true, x, note.fret, note.techniques, systemIndex};
                }
                if (palmMute)
                    text(x, system->staffTop - 10.0, "P.M.", m_style.smallSize - 1, false, false, Tone::Faint);
                if (accent && !palmMute)
                    text(x, system->staffTop - 10.0, ">", m_style.smallSize, false, true);
                (void)bendUp;
            }
        }
        if (!bar.voices.empty())
            drawRhythm(bar.voices[0], xAt);
    }

    // Stems, flags, beams, dots and tuplet numbers under the staff, for the first voice.
    template<typename XAt>
    void drawRhythm(const std::vector<ScoreBeat> &beats, XAt xAt)
    {
        const double top = system->staffBottom + 8.0, bottom = top + 16.0;
        const auto beamsOf = [](const ScoreBeat &b) {
            int n = 0;
            for (int v = b.noteValue; v > 4; v /= 2)
                ++n;
            return n;
        };
        const int ppq = m_score.ppq;
        for (std::size_t i = 0; i < beats.size(); ++i) {
            const ScoreBeat &b = beats[i];
            const double x = xAt(b.start);
            if (b.notes.empty()) { // a rest: a short bar for long rests, a small zigzag for short ones
                if (b.noteValue <= 2)
                    line(x - 4.0, top + 6.0, x + 4.0, top + 6.0, 3.0, Tone::Faint);
                else {
                    line(x - 2.0, top + 2.0, x + 2.0, top + 6.0, 1.2, Tone::Faint);
                    line(x + 2.0, top + 6.0, x - 2.0, top + 9.0, 1.2, Tone::Faint);
                    line(x - 2.0, top + 9.0, x + 2.0, top + 13.0, 1.2, Tone::Faint);
                }
                continue;
            }
            if (b.noteValue >= 2)
                line(x, b.noteValue == 2 ? top + 8.0 : top, x, bottom, 1.0);
            if (b.dots > 0)
                dot(x + 4.0, bottom - 2.0, 1.3);
            const int beams = beamsOf(b);
            if (beams == 0)
                continue;
            // Beam to the next beat in the same quarter note; otherwise a flag.
            const bool withNext = i + 1 < beats.size() && !beats[i + 1].notes.empty() && beamsOf(beats[i + 1]) > 0
                                  && beats[i + 1].start / ppq == b.start / ppq;
            const bool withPrev = i > 0 && !beats[i - 1].notes.empty() && beamsOf(beats[i - 1]) > 0
                                  && beats[i - 1].start / ppq == b.start / ppq;
            for (int k = 0; k < beams; ++k) {
                const double y = bottom - 3.0 * k;
                if (withNext && beamsOf(beats[i + 1]) > k)
                    line(x, y, xAt(beats[i + 1].start), y, 2.0);
                else if (!withPrev || beamsOf(beats[i - 1]) <= k)
                    line(x, y, x + (withPrev ? -6.0 : 6.0), y, 2.0);
            }
            if (b.tupletNum > 0 && (i == 0 || beats[i - 1].tupletNum != b.tupletNum || withPrev == false))
                text(x + 6.0, bottom + 8.0, std::to_string(b.tupletNum), m_style.smallSize - 1, false, false, Tone::Faint);
        }
    }

    void drawHeader(double x0)
    {
        if (m_notes) {
            const double g = m_style.noteGap;
            if (m_clef == kTrebleClef)
                glyph(x0 + 14.0, stepY(m_bottom + 2) - g * 0.9, m_clef, g * 4.6); // the curl round G
            else if (m_clef == kBassClef)
                glyph(x0 + 14.0, stepY(m_bottom + 6) + g * 0.5, m_clef, g * 4.0); // the dots round F
            else
                glyph(x0 + 12.0, stepY(m_bottom + 4), m_clef, g * 4.0);
            return;
        }
        for (int s = 0; s < m_strings; ++s) {
            text(x0 + 8.0, stringY(s), noteName(m_track.tuning[std::size_t(s)] + m_track.capo), m_style.smallSize, false, false,
                 Tone::Faint);
        }
    }

    // One bar's notes: heads by pitch (or drum place), accidentals, ledger
    // lines, ties, dots, stems, flags, beams, tuplets and rests.
    template<typename XAt>
    void drawNotes(const ScoreBar &bar, XAt xAt)
    {
        const double g = m_style.noteGap, rx = g * 0.62;
        const int middle = m_bottom + 4, top = m_bottom + 8;
        const bool drums = m_track.drums;
        int voicesWithNotes = 0;
        for (const auto &voice : bar.voices)
            for (const ScoreBeat &b : voice)
                if (!b.notes.empty()) {
                    ++voicesWithNotes;
                    break;
                }
        std::map<int, bool> sharpened; // accidentals last written in this bar, by step
        const auto beamsOf = [](const ScoreBeat &b) {
            int n = 0;
            for (int v = b.noteValue; v > 4; v /= 2)
                ++n;
            return n;
        };

        struct Info
        {
            double x = 0;
            std::vector<int> steps; // sorted
            int beams = 0;
            bool rest = true, stem = false;
        };
        for (std::size_t v = 0; v < bar.voices.size(); ++v) {
            const auto &beats = bar.voices[v];
            const int forced = voicesWithNotes > 1 || drums ? (v == 0 ? 1 : -1) : 0; // 1 up, -1 down, 0 by pitch
            std::vector<Info> info(beats.size());
            for (std::size_t i = 0; i < beats.size(); ++i) {
                const ScoreBeat &b = beats[i];
                Info &in = info[i];
                in.x = xAt(b.start);
                in.beams = beamsOf(b);
                if (b.notes.empty()) { // a rest
                    double y = stepY(middle);
                    if (b.noteValue == 1)
                        y = stepY(m_bottom + 6) + g * 0.5;
                    else if (b.noteValue == 2)
                        y = stepY(middle) - g * 0.5;
                    if (voicesWithNotes > 1) // two voices: each voice's rests keep to its side
                        y += forced * -2.0 * g;
                    glyph(in.x, y, restGlyph(b.noteValue), g * 3.4);
                    continue;
                }
                in.rest = false;
                in.stem = b.noteValue >= 2;
                int previous = -1000;
                bool shifted = false;
                std::vector<std::pair<int, const ScoreNote *>> placed;
                for (const ScoreNote &n : b.notes) {
                    const int step = drums ? drumPlace(n.pitch).step : spell(n.pitch + m_transpose).step;
                    placed.emplace_back(step, &n);
                }
                std::sort(placed.begin(), placed.end(), [](const auto &a, const auto &c) { return a.first < c.first; });
                for (const auto &[step, n] : placed) {
                    in.steps.push_back(step);
                    // A second above the last note goes to the other side of the stem.
                    const bool shift = step - previous == 1 && !shifted;
                    shifted = shift;
                    previous = step;
                    const double hx = in.x + (shift ? 2.0 * rx - 1.0 : 0.0);
                    const double y = stepY(step);
                    // Ledger lines.
                    for (int s = m_bottom - 2; s >= step; s -= 2)
                        line(hx - rx - 3.0, stepY(s), hx + rx + 3.0, stepY(s), 1.0);
                    for (int s = top + 2; s <= step; s += 2)
                        line(hx - rx - 3.0, stepY(s), hx + rx + 3.0, stepY(s), 1.0);
                    if (drums) {
                        const DrumPlace place = drumPlace(n->pitch);
                        if (place.cross) {
                            line(hx - rx, y - rx, hx + rx, y + rx, 1.4);
                            line(hx - rx, y + rx, hx + rx, y - rx, 1.4);
                        } else {
                            head(hx, y, b.noteValue >= 4);
                        }
                        if (place.open)
                            text(hx, stepY(top + 3), "o", m_style.smallSize, false, false);
                    } else {
                        const Spelled sp = spell(n->pitch + m_transpose);
                        const auto known = sharpened.find(step);
                        const bool wasSharp = known != sharpened.end() && known->second;
                        if (sp.sharp && !wasSharp)
                            glyph(in.x - rx - 7.0, y, kSharpSign, g * 2.4);
                        else if (!sp.sharp && wasSharp)
                            glyph(in.x - rx - 7.0, y, kNaturalSign, g * 2.4);
                        sharpened[step] = sp.sharp;
                        head(hx, y, b.noteValue >= 4);
                    }
                    if (n->techniques & TieDestination) // a short tie from the left
                        line(hx - rx - 9.0, y + g * 0.6, hx - rx - 1.0, y + g * 0.6, 1.0, Tone::Faint);
                    for (int d = 0; d < b.dots; ++d) { // in the space, not on a line
                        const bool onLine = ((step - m_bottom) % 2 + 2) % 2 == 0;
                        dot(hx + rx + 4.0 + 4.0 * d, y - (onLine ? g / 2.0 : 0.0), 1.4);
                    }
                }
            }

            // Stems, flags and beams: beats in the same quarter note beam together.
            const int ppq = m_score.ppq;
            for (std::size_t i = 0; i < beats.size();) {
                if (info[i].rest || !info[i].stem) {
                    ++i;
                    continue;
                }
                std::size_t j = i + 1;
                if (info[i].beams > 0)
                    while (j < beats.size() && !info[j].rest && info[j].beams > 0 && beats[j].start / ppq == beats[i].start / ppq)
                        ++j;
                // Direction: forced by the voice, or away from the middle line.
                int sum = 0, count = 0;
                for (std::size_t k = i; k < j; ++k)
                    for (int s : info[k].steps) {
                        sum += s;
                        ++count;
                    }
                const bool up = forced != 0 ? forced > 0 : (count > 0 && double(sum) / count < middle);
                double tip = up ? 1e9 : -1e9;
                for (std::size_t k = i; k < j; ++k) {
                    const double reach = up ? stepY(info[k].steps.back()) - 3.5 * g : stepY(info[k].steps.front()) + 3.5 * g;
                    tip = up ? std::min(tip, reach) : std::max(tip, reach);
                }
                if (j - i == 1) // alone: its own length
                    tip = up ? std::min(tip, stepY(middle) - 0.5 * g) : std::max(tip, stepY(middle) + 0.5 * g);
                const auto stemX = [&](std::size_t k) { return info[k].x + (up ? rx - 0.6 : -rx + 0.6); };
                for (std::size_t k = i; k < j; ++k) {
                    const double from = up ? stepY(info[k].steps.front()) : stepY(info[k].steps.back());
                    line(stemX(k), from, stemX(k), tip, 1.1);
                }
                const double step = up ? 4.0 : -4.0;
                if (j - i == 1) { // flags
                    for (int b = 0; b < info[i].beams; ++b) {
                        const double y = tip + step * b;
                        line(stemX(i), y, stemX(i) + 7.0, y + (up ? 9.0 : -9.0), 1.6);
                    }
                } else { // beams, and part-beams where only one side has them
                    for (std::size_t k = i; k < j; ++k)
                        for (int b = 0; b < info[k].beams; ++b) {
                            const double y = tip + step * b;
                            if (k + 1 < j && info[k + 1].beams > b)
                                line(stemX(k), y, stemX(k + 1), y, 2.6);
                            else if (!(k > i && info[k - 1].beams > b))
                                line(stemX(k), y, stemX(k) + (k + 1 < j ? 6.0 : -6.0), y, 2.6);
                        }
                    if (beats[i].tupletNum > 0)
                        text((stemX(i) + stemX(j - 1)) / 2.0, tip + (up ? -8.0 : 8.0), std::to_string(beats[i].tupletNum),
                             m_style.smallSize - 1, false, false, Tone::Faint);
                }
                i = j;
            }
        }
    }

private:
    const Score &m_score;
    const ScoreTrack &m_track;
    const TabStyle &m_style;
    int m_strings;
    bool m_notes = false;
    const char *m_clef = kTrebleClef;
    int m_bottom = 30; // the bottom line's step: E4 (treble), G2 (bass), E4 (percussion)
    int m_transpose = 0;
};

} // namespace

namespace {

TabLayoutResult layoutStaff(const Score &score, int track, double width, const TabStyle &style, bool notes)
{
    TabLayoutResult out;
    if (track < 0 || track >= int(score.tracks.size()))
        return out;
    const ScoreTrack &st = score.tracks[std::size_t(track)];
    if (score.bars.empty() || width < 120.0)
        return out;
    if (!notes && (st.drums || st.tuning.empty()))
        return out;
    out.strings = notes ? 5 : int(st.tuning.size());
    out.barPlace.assign(score.bars.size(), {-1, -1});
    const double headerWidth = notes ? 42.0 : style.headerWidth;

    // Each bar's beat starts and natural width.
    std::vector<BarPlan> plans;
    for (int m = 0; m < int(score.bars.size()); ++m) {
        const MasterBar &master = score.bars[std::size_t(m)];
        BarPlan plan;
        plan.bar = m;
        plan.length = Tick(score.ppq) * 4 * master.time.numerator / master.time.denominator;
        std::set<Tick> starts{0};
        if (std::size_t(m) < st.bars.size())
            for (const auto &voice : st.bars[std::size_t(m)].voices)
                for (const ScoreBeat &b : voice)
                    if (b.start < plan.length)
                        starts.insert(b.start);
        plan.ticks.assign(starts.begin(), starts.end());
        for (std::size_t i = 0; i < plan.ticks.size(); ++i) {
            const Tick next = i + 1 < plan.ticks.size() ? plan.ticks[i + 1] : plan.length;
            plan.widths.push_back(slotWidth(next - plan.ticks[i], score.ppq) + (notes ? 6.0 : 0.0)); // room for accidentals
        }
        plan.timeSig = m == 0 || master.time.numerator != score.bars[std::size_t(m - 1)].time.numerator
                       || master.time.denominator != score.bars[std::size_t(m - 1)].time.denominator;
        plan.fixed = kPadLeft + kPadRight + (plan.timeSig ? kTimeSigWidth : 0.0) + (master.repeatStart ? kRepeatWidth : 0.0)
                     + (master.repeatEnd ? kRepeatWidth : 0.0);
        plans.push_back(std::move(plan));
    }

    // Pack bars into systems, then stretch each full system to the width.
    // Notation needs room above and below the staff for ledger lines and stems.
    const double staffHeight = double(out.strings - 1) * (notes ? style.noteGap : style.lineGap);
    const double top = notes ? style.topSpace + 2.0 * style.noteGap : style.topSpace;
    const double below = notes ? 5.0 * style.noteGap : style.rhythmSpace;
    const double systemHeight = top + staffHeight + below + style.systemGap;
    const double usable = width - 2.0; // a pixel each side for the end bar lines
    std::size_t i = 0;
    double y = 0.0;
    while (i < plans.size()) {
        std::size_t j = i;
        double used = headerWidth;
        while (j < plans.size() && (j == i || used + plans[j].fixed + plans[j].slots() <= usable)) {
            used += plans[j].fixed + plans[j].slots();
            ++j;
        }
        const bool lastSystem = j == plans.size();
        double slotTotal = 0.0, fixedTotal = 0.0;
        for (std::size_t k = i; k < j; ++k) {
            slotTotal += plans[k].slots();
            fixedTotal += plans[k].fixed;
        }
        const double spare = usable - headerWidth - fixedTotal - slotTotal;
        // Stretch to the edge; the last system only up to a little.
        double scale = slotTotal > 0.0 ? 1.0 + spare / slotTotal : 1.0;
        if (lastSystem)
            scale = std::min(scale, 1.4);
        scale = std::max(scale, 0.5);

        TabSystem system;
        system.top = y;
        system.height = systemHeight;
        system.staffTop = y + top;
        system.staffBottom = system.staffTop + staffHeight;
        Builder b(score, st, style, notes);
        b.system = &system;
        b.systemIndex = int(out.systems.size());
        double x = 1.0 + headerWidth;
        for (std::size_t k = i; k < j; ++k) {
            const BarPlan &plan = plans[k];
            const MasterBar &master = score.bars[std::size_t(plan.bar)];
            TabBarBox box;
            box.bar = plan.bar;
            box.length = plan.length;
            box.x0 = x;
            box.contentX0 = x + kPadLeft + (plan.timeSig ? kTimeSigWidth : 0.0) + (master.repeatStart ? kRepeatWidth : 0.0);
            double cx = box.contentX0;
            box.ticks = plan.ticks;
            for (std::size_t n = 0; n < plan.ticks.size(); ++n) {
                box.xs.push_back(cx + 4.0);
                cx += plan.widths[n] * scale;
            }
            box.contentX1 = cx;
            box.x1 = cx + kPadRight + (master.repeatEnd ? kRepeatWidth : 0.0);
            x = box.x1;
            out.barPlace[std::size_t(plan.bar)] = {int(out.systems.size()), int(system.bars.size())};
            system.bars.push_back(std::move(box));
        }
        // Staff lines, the tuning, the opening bar line, then the bars.
        const double right = system.bars.back().x1;
        for (int s = 0; s < out.strings; ++s) {
            const double ly = notes ? system.staffTop + s * style.noteGap : b.stringY(s);
            b.line(1.0, ly, right, ly, 1.0, notes ? Tone::Ink : Tone::Faint);
        }
        b.drawHeader(1.0);
        b.line(1.0, system.staffTop, 1.0, system.staffBottom, 1.0);
        for (const TabBarBox &box : system.bars)
            b.drawBar(box, score.bars[std::size_t(box.bar)], plans[std::size_t(box.bar)].timeSig, box.bar + 1);
        out.systems.push_back(std::move(system));
        y += systemHeight;
        i = j;
    }
    out.height = y;
    return out;
}

} // namespace

TabLayoutResult layoutTab(const Score &score, int track, double width, const TabStyle &style)
{
    return layoutStaff(score, track, width, style, false);
}

TabLayoutResult layoutNotation(const Score &score, int track, double width, const TabStyle &style)
{
    return layoutStaff(score, track, width, style, true);
}

TabCursor cursorAt(const Score &score, const TabLayoutResult &layout, Tick songTick)
{
    TabCursor cursor;
    if (layout.systems.empty() || score.playStarts.empty() || songTick < 0)
        return cursor;
    const auto after = std::upper_bound(score.playStarts.begin(), score.playStarts.end(), songTick);
    const std::size_t k = std::size_t(after - score.playStarts.begin()) - 1;
    const int bar = score.playOrder[k];
    if (bar < 0 || bar >= int(layout.barPlace.size()) || layout.barPlace[std::size_t(bar)].first < 0)
        return cursor;
    const auto [s, index] = layout.barPlace[std::size_t(bar)];
    const TabBarBox &box = layout.systems[std::size_t(s)].bars[std::size_t(index)];
    const Tick into = songTick - score.playStarts[k];
    if (into > box.length) // past the end of the song
        return cursor;
    // Between the beats either side, in proportion.
    double x = box.contentX1;
    for (std::size_t n = 0; n < box.ticks.size(); ++n) {
        const Tick t0 = box.ticks[n];
        const Tick t1 = n + 1 < box.ticks.size() ? box.ticks[n + 1] : box.length;
        if (into >= t0 && into < t1) {
            const double x1 = n + 1 < box.xs.size() ? box.xs[n + 1] : box.contentX1;
            x = box.xs[n] + (x1 - box.xs[n]) * double(into - t0) / double(std::max<Tick>(1, t1 - t0));
            break;
        }
    }
    cursor.system = s;
    cursor.x = x;
    cursor.bar = bar;
    return cursor;
}

int barAt(const TabLayoutResult &layout, double x, double y)
{
    for (const TabSystem &s : layout.systems) {
        if (y < s.top || y >= s.top + s.height)
            continue;
        for (const TabBarBox &box : s.bars)
            if (x >= box.x0 && x < box.x1)
                return box.bar;
        return -1;
    }
    return -1;
}

} // namespace krate::song
