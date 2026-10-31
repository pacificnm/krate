#include "konnekt/app/Translate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <set>

#include <QJsonArray>

#include "konnekt/dsp/Modules.h"

namespace konnekt {

namespace {

// ---- The patch as a graph ----------------------------------------------------------------

struct Node
{
    QString type;
    const ModuleInfo *info = nullptr;
    QJsonObject knobs;
    double knob(const char *key) const
    {
        if (const QJsonValue v = knobs.value(QLatin1String(key)); v.isDouble())
            return v.toDouble();
        for (const KnobInfo &k : info->knobs)
            if (std::string_view(k.key) == key)
                return k.defaultValue;
        return 0.0;
    }
};

struct Jack
{
    int slot = -1;
    QString key;
    bool valid() const { return slot >= 0; }
};

struct Graph
{
    std::map<int, Node> nodes;
    std::map<std::pair<int, QString>, Jack> into; // (slot, input) -> what's plugged in
    int voices = 4;

    explicit Graph(const QJsonObject &patch)
    {
        voices = patch.value(u"voices").toInt(4);
        for (const QJsonValue v : patch.value(u"modules").toArray()) {
            const QJsonObject o = v.toObject();
            const ModuleInfo *info = findModule(o.value(u"type").toString().toStdString());
            if (info)
                nodes[o.value(u"slot").toInt()] = {o.value(u"type").toString(), info, o.value(u"knobs").toObject()};
        }
        for (const QJsonValue v : patch.value(u"cables").toArray()) {
            const QJsonObject o = v.toObject();
            const int from = o.value(u"from").toInt(-1), to = o.value(u"to").toInt(-1);
            if (nodes.count(from) && nodes.count(to))
                into[{to, o.value(u"in").toString()}] = {from, o.value(u"out").toString()};
        }
    }
    const Node *node(int slot) const
    {
        const auto it = nodes.find(slot);
        return it == nodes.end() ? nullptr : &it->second;
    }
    QString type(int slot) const { return node(slot) ? node(slot)->type : QString(); }
    Jack source(int slot, const char *input) const
    {
        const auto it = into.find({slot, QString::fromLatin1(input)});
        return it == into.end() ? Jack{} : it->second;
    }
    QString name(int slot) const { return node(slot) ? QString::fromUtf8(node(slot)->info->name) : QString(); }
};

// ---- What the patch is ---------------------------------------------------------------------

struct Mod // something moving a parameter
{
    int slot = -1;      // an envelope or LFO
    double depth = 0.0; // in the parameter's units (octaves, width...)
    bool viaVca = false; // an LFO through a VCA (a vibrato that fades in)
    bool inverted = false;
};

struct Osc
{
    int slot = -1;
    std::map<QString, double> waves; // output -> level
    double octave = 0, tune = 0, width = 0.5;
    Mod pitch, pwm;     // FM (octaves), PWM (width)
    int syncTo = -1;    // the oscillator it's synced to
    int linFmFrom = -1; // the oscillator shaking its frequency
    QString syncWave, linFmWave; // the outputs they come from
    double linFm = 0.0;
};

struct Voice
{
    std::vector<Osc> oscs;
    double noise = 0.0;
    QString noiseColor;
    int ringA = -1, ringB = -1;
    double ringMix = 1.0;
    int filter = -1;
    QString filterOut;
    Mod cutoffEnv, cutoffLfo;
    bool keyTrack = false;
    int ampEnv = -1;
    bool ampGate = false;
    double vcaLevel = 0.0;
    Mod tremolo;
    int drive = -1, chorus = -1, delay = -1, reverb = -1;
    double glide = 0.0; // seconds
    bool fromSequencer = false;
    QStringList lost;
    std::set<int> used;
};

Osc *findOsc(Voice &v, int slot)
{
    for (Osc &o : v.oscs)
        if (o.slot == slot)
            return &o;
    return nullptr;
}

// An input's modulation source: an envelope, an LFO (maybe through a VCA, as a
// fading-in vibrato does), or an envelope's inverted output turned back up by
// an attenuverter (a sweep up).
Mod modulation(const Graph &g, Voice &v, Jack j, double depth, const QString &what)
{
    if (!j.valid())
        return {};
    const QString t = g.type(j.slot);
    if (t == u"envelope" || t == u"lfo") {
        v.used.insert(j.slot);
        return {j.slot, depth, false, j.key == u"inverted"};
    }
    if (t == u"vca") { // an LFO through a VCA: its depth set by another source
        const Jack in = g.source(j.slot, "in");
        if (in.valid() && g.type(in.slot) == u"lfo") {
            v.used.insert({j.slot, in.slot});
            if (const Jack cv = g.source(j.slot, "cv"); cv.valid())
                v.used.insert(cv.slot);
            return {in.slot, depth, true, false};
        }
    }
    if (t == u"attenuverter") {
        const Jack in = g.source(j.slot, "a");
        if (in.valid() && g.type(in.slot) == u"envelope") {
            v.used.insert({j.slot, in.slot});
            const double scale = g.node(j.slot)->knob("scale_a");
            const bool inverted = (in.key == u"inverted") != (scale < 0);
            return {in.slot, depth * std::fabs(scale), false, inverted};
        }
    }
    if (t == u"midi_in")
        return {}; // velocity, wheel...: the synths have their own
    v.lost << QStringLiteral("%1 from %2 (only envelopes and LFOs translate)").arg(what, g.name(j.slot));
    return {};
}

void walk(const Graph &g, Voice &v, Jack j, double gain, int depth = 0)
{
    if (!j.valid() || depth > 24)
        return;
    const int s = j.slot;
    const Node *n = g.node(s);
    const QString t = n->type;
    v.used.insert(s);
    const auto next = [&](const char *input, double gainHere = 1.0) { walk(g, v, g.source(s, input), gain * gainHere, depth + 1); };
    if (t == u"reverb" || t == u"delay") {
        (t == u"reverb" ? v.reverb : v.delay) = s;
        next("left");
    } else if (t == u"chorus") {
        v.chorus = s;
        next("in");
    } else if (t == u"drive") {
        v.drive = s;
        next("in");
    } else if (t == u"crusher") {
        v.lost << QStringLiteral("Crusher (no bit crushing on the synths)");
        next("in");
    } else if (t == u"scope") {
        next("in");
    } else if (t == u"vca") {
        const Jack cv = g.source(s, "cv");
        const QString cvType = cv.valid() ? g.type(cv.slot) : QString();
        if (v.ampEnv < 0 && !v.ampGate && cvType == u"envelope") {
            v.ampEnv = cv.slot;
            v.used.insert(cv.slot);
        } else if (v.ampEnv < 0 && !v.ampGate && cvType == u"midi_in" && cv.key == u"gate") {
            v.ampGate = true;
        } else if (cvType == u"lfo") {
            v.tremolo = {cv.slot, n->knob("cv_amount"), false, false};
            v.used.insert(cv.slot);
        } else if (cv.valid()) {
            v.lost << QStringLiteral("VCA opened by %1").arg(g.name(cv.slot));
        }
        if (!cv.valid())
            v.vcaLevel = n->knob("level");
        next("in");
    } else if (t == u"ladder" || t == u"ota_filter" || t == u"svf") {
        if (v.filter < 0) {
            v.filter = s;
            v.filterOut = j.key;
        } else
            v.lost << QStringLiteral("a second filter (%1)").arg(g.name(s));
        next("in");
    } else if (t == u"mixer") {
        for (int i = 1; i <= 4; ++i) {
            const QByteArray in = QByteArray("in") + QByteArray::number(i), level = QByteArray("level") + QByteArray::number(i);
            next(in.constData(), n->knob(level.constData()));
        }
    } else if (t == u"oscillator") {
        Osc *o = findOsc(v, s);
        if (!o) {
            v.oscs.push_back({});
            o = &v.oscs.back();
            o->slot = s;
        }
        o->waves[j.key] += gain;
    } else if (t == u"noise") {
        v.noise += gain;
        v.noiseColor = j.key;
    } else if (t == u"ring_mod") {
        const Jack a = g.source(s, "a"), b = g.source(s, "b");
        if (a.valid() && b.valid() && g.type(a.slot) == u"oscillator" && g.type(b.slot) == u"oscillator") {
            v.ringA = a.slot;
            v.ringB = b.slot;
            v.ringMix = n->knob("mix");
            walk(g, v, a, gain, depth + 1); // the oscillators themselves
            walk(g, v, b, gain * n->knob("mix"), depth + 1);
        } else {
            v.lost << QStringLiteral("Ring Mod of something other than two oscillators");
            next("a");
        }
    } else {
        v.lost << QStringLiteral("%1 in the sound's path").arg(g.name(s));
    }
}

Voice analyse(const Graph &g)
{
    Voice v;
    int output = -1;
    for (const auto &[slot, n] : g.nodes)
        if (n.type == u"output")
            output = slot;
    if (output < 0) {
        v.lost << QStringLiteral("no Output: nothing is heard");
        return v;
    }
    v.used.insert(output);
    walk(g, v, g.source(output, "left"), 1.0);
    if (!g.source(output, "left").valid())
        walk(g, v, g.source(output, "right"), 1.0);

    // Each oscillator: its tuning, what moves its pitch and width, sync, FM.
    for (Osc &o : v.oscs) {
        const Node *n = g.node(o.slot);
        o.octave = n->knob("octave");
        o.tune = n->knob("tune");
        o.width = n->knob("width");
        const Jack pitch = g.source(o.slot, "pitch");
        if (pitch.valid()) {
            const QString t = g.type(pitch.slot);
            if (t == u"slew") {
                v.used.insert(pitch.slot);
                v.glide = std::max(v.glide, 0.5 * (g.node(pitch.slot)->knob("rise") + g.node(pitch.slot)->knob("fall")));
            } else if (t == u"sequencer") {
                v.fromSequencer = true;
                v.used.insert(pitch.slot);
            }
        }
        o.pitch = modulation(g, v, g.source(o.slot, "fm"), n->knob("fm_amount"), QStringLiteral("Pitch modulation"));
        o.pwm = modulation(g, v, g.source(o.slot, "pwm"), n->knob("pwm_amount"), QStringLiteral("Pulse-width modulation"));
        if (const Jack sync = g.source(o.slot, "sync"); sync.valid() && g.type(sync.slot) == u"oscillator") {
            o.syncTo = sync.slot;
            o.syncWave = sync.key;
        }
        if (const Jack fm = g.source(o.slot, "lin_fm"); fm.valid()) {
            Jack src = fm;
            if (g.type(src.slot) == u"vca") { // a modulator whose depth an envelope sets
                v.used.insert(src.slot);
                if (const Jack cv = g.source(src.slot, "cv"); cv.valid())
                    v.used.insert(cv.slot);
                src = g.source(src.slot, "in");
            }
            if (src.valid() && g.type(src.slot) == u"oscillator") {
                o.linFmFrom = src.slot;
                o.linFmWave = src.key;
                o.linFm = n->knob("lin_fm_amount");
                v.used.insert(src.slot);
            } else
                v.lost << QStringLiteral("Lin FM from something other than an oscillator");
        }
    }
    // FM modulators and sync masters that aren't heard still belong to the voice.
    for (std::size_t i = 0; i < v.oscs.size(); ++i)
        for (const auto &[extra, wave] : {std::pair{v.oscs[i].linFmFrom, v.oscs[i].linFmWave}, std::pair{v.oscs[i].syncTo, v.oscs[i].syncWave}})
            if (extra >= 0 && !findOsc(v, extra)) {
                Osc o;
                o.slot = extra;
                o.waves[wave] = 0.0; // not heard, but this is its wave
                const Node *n = g.node(extra);
                o.octave = n->knob("octave");
                o.tune = n->knob("tune");
                o.width = n->knob("width");
                v.oscs.push_back(o);
            }

    if (v.filter >= 0) {
        const Node *f = g.node(v.filter);
        const double cv = f->knob("cv_amount");
        const Jack cut = g.source(v.filter, "cutoff");
        if (cut.valid() && g.type(cut.slot) == u"mixer") { // an envelope and an LFO mixed
            v.used.insert(cut.slot);
            for (int i = 1; i <= 4; ++i) {
                const QByteArray in = QByteArray("in") + QByteArray::number(i), level = QByteArray("level") + QByteArray::number(i);
                const Jack src = g.source(cut.slot, in.constData());
                if (!src.valid())
                    continue;
                const Mod m = modulation(g, v, src, cv * g.node(cut.slot)->knob(level.constData()), QStringLiteral("Filter modulation"));
                if (m.slot >= 0)
                    (g.type(m.slot) == u"envelope" ? v.cutoffEnv : v.cutoffLfo) = m;
            }
        } else {
            const Mod m = modulation(g, v, cut, cv, QStringLiteral("Filter modulation"));
            if (m.slot >= 0)
                (g.type(m.slot) == u"envelope" ? v.cutoffEnv : v.cutoffLfo) = m;
        }
        v.keyTrack = g.source(v.filter, "key").valid();
    }

    // What's left over: modules the voice doesn't use.
    for (const auto &[slot, n] : g.nodes)
        if (!v.used.count(slot) && n.type != u"midi_in" && n.type != u"scope") {
            if (n.type == u"clock" || n.type == u"sequencer")
                v.lost << QStringLiteral("%1 (the synth is played from its keys; it has its own arpeggiator or sequencer)").arg(g.name(slot));
            else
                v.lost << QStringLiteral("%1 (not part of the sound's path the synth can follow)").arg(g.name(slot));
        }
    if (v.fromSequencer && !v.lost.join(u' ').contains(u"Sequencer"))
        v.lost << QStringLiteral("the Sequencer's notes");
    return v;
}

// ---- Setting a synth's knobs ----------------------------------------------------------

double clamp01(double x) { return std::clamp(x, 0.0, 1.0); }
double detent(int index, int positions) { return positions > 1 ? double(std::clamp(index, 0, positions - 1)) / double(positions - 1) : 0.0; }
double cutoff20k(double hz) { return clamp01(std::log(std::max(hz, 20.0) / 20.0) / std::log(1000.0)); }
double env10k(double s) { return clamp01(std::log10(std::max(s, 0.001) / 0.001) / 4.0); }
double env20k(double s) { return clamp01(std::log(std::max(s, 0.0005) / 0.0005) / std::log(20000.0)); }
QString seconds(double s) { return s < 1.0 ? QStringLiteral("%1 ms").arg(s * 1000.0, 0, 'f', 0) : QStringLiteral("%1 s").arg(s, 0, 'f', 2); }
QString hz(double f) { return f >= 1000.0 ? QStringLiteral("%1 kHz").arg(f / 1000.0, 0, 'f', 2) : QStringLiteral("%1 Hz").arg(f, 0, 'f', 0); }

// The synths' delays follow their tempo: the division nearest the time at 120 BPM.
int delayDivision(double s)
{
    static const double beats[] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
    const double b = s * 2.0;
    int best = 0;
    for (int i = 1; i < 7; ++i)
        if (std::fabs(std::log(beats[i] / b)) < std::fabs(std::log(beats[best] / b)))
            best = i;
    return best;
}
const char *kDivisionNames[] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};

struct Out
{
    Translation t;
    std::set<QString> keys;
    void set(const QString &key, double v)
    {
        if (keys.insert(key).second)
            t.params.push_back({key, clamp01(v)});
        else
            for (auto &[k, value] : t.params)
                if (k == key)
                    value = clamp01(v);
    }
    void exact(const QString &s) { t.exact << s; }
    void approx(const QString &s) { t.approx << s; }
    void lost(const QString &s) { t.lost << s; }
};

struct Env
{
    double a = 0.001, d = 0.3, s = 1.0, r = 0.05;
};
Env envOf(const Graph &g, int slot)
{
    const Node *n = g.node(slot);
    return n ? Env{n->knob("attack"), n->knob("decay"), n->knob("sustain"), n->knob("release")} : Env{0.001, 0.01, 1.0, 0.005};
}
QString envText(const Env &e) { return QStringLiteral("A %1, D %2, S %3 %, R %4").arg(seconds(e.a), seconds(e.d)).arg(e.s * 100.0, 0, 'f', 0).arg(seconds(e.r)); }

// The oscillators in the order the synths want them: the one heard most (or
// the FM carrier, or the sync slave's master) first.
std::vector<const Osc *> ordered(const Voice &v)
{
    std::vector<const Osc *> out;
    for (const Osc &o : v.oscs)
        out.push_back(&o);
    const auto loudness = [](const Osc *o) {
        double l = 0;
        for (const auto &[w, level] : o->waves)
            l += level;
        return l;
    };
    std::stable_sort(out.begin(), out.end(), [&](const Osc *a, const Osc *b) {
        if (a->linFmFrom == b->slot || b->syncTo == a->slot)
            return true; // a is the carrier, or b's sync master: a first
        if (b->linFmFrom == a->slot || a->syncTo == b->slot)
            return false;
        return loudness(a) > loudness(b);
    });
    return out;
}
double level(const Osc *o)
{
    double l = 0;
    for (const auto &[w, value] : o->waves)
        l += value;
    return l;
}
// The wave heard most from an oscillator ("saw", "pulse"...), a square for a pulse at 50 %.
QString mainWave(const Osc *o)
{
    QString best = QStringLiteral("saw");
    double most = -1;
    for (const auto &[w, value] : o->waves)
        if (value > most) {
            most = value;
            best = w;
        }
    if (best == u"pulse" && std::fabs(o->width - 0.5) < 0.04)
        return QStringLiteral("square");
    return best;
}

// Shared: effects, and what the voice lost.
// chorusModes: the synth's CHORUS positions (Off first); chorusMap: Konnekt's
// I, II, Ensemble -> those positions. Empty: the synth has no chorus.
void effects(const Graph &g, const Voice &v, Out &o, const QStringList &chorusModes, std::array<int, 3> chorusMap, const QString &synth)
{
    if (v.chorus >= 0) {
        if (chorusModes.isEmpty())
            o.lost(QStringLiteral("Chorus (%1 has none)").arg(synth));
        else {
            const int mode = std::clamp(int(std::lround(g.node(v.chorus)->knob("mode"))), 0, 2);
            const int to = chorusMap[std::size_t(mode)];
            o.set(QStringLiteral("chorus"), detent(to, int(chorusModes.size())));
            const QString names[] = {QStringLiteral("I"), QStringLiteral("II"), QStringLiteral("Ensemble")};
            if (names[mode] == chorusModes[to])
                o.exact(QStringLiteral("Chorus %1").arg(names[mode]));
            else
                o.approx(QStringLiteral("Chorus %1 → %2").arg(names[mode], chorusModes[to]));
        }
    } else if (!chorusModes.isEmpty())
        o.set(QStringLiteral("chorus"), 0.0);
    if (v.delay >= 0) {
        const Node *n = g.node(v.delay);
        const int div = delayDivision(n->knob("time"));
        o.set(QStringLiteral("delay_level"), n->knob("mix"));
        o.set(QStringLiteral("delay_time"), detent(div, 7));
        o.set(QStringLiteral("delay_fb"), n->knob("feedback") / 0.9);
        o.approx(QStringLiteral("Delay %1 → %2 (it follows the synth's tempo; %2 at 120 BPM)").arg(seconds(n->knob("time")), QString::fromLatin1(kDivisionNames[div])));
    } else
        o.set(QStringLiteral("delay_level"), 0.0);
    if (v.reverb >= 0) {
        const Node *n = g.node(v.reverb);
        o.set(QStringLiteral("reverb_level"), n->knob("mix"));
        o.set(QStringLiteral("reverb_size"), n->knob("size"));
        o.exact(QStringLiteral("Reverb"));
    } else
        o.set(QStringLiteral("reverb_level"), 0.0);
    for (const QString &l : v.lost)
        o.lost(l);
    if (v.oscs.empty() && v.noise <= 0)
        o.lost(QStringLiteral("no oscillator reaches the Output"));
}

// The LFO the synth's one LFO copies: the first used, for pitch, then filter, PWM, tremolo.
int mainLfo(const Graph &g, const Voice &v, const std::vector<const Osc *> &oscs, Out &o)
{
    std::vector<int> lfos;
    const auto add = [&](const Mod &m) {
        if (m.slot >= 0 && g.type(m.slot) == u"lfo" && std::find(lfos.begin(), lfos.end(), m.slot) == lfos.end())
            lfos.push_back(m.slot);
    };
    for (const Osc *x : oscs) // (a missing second oscillator is null)
        if (x)
            add(x->pitch);
    add(v.cutoffLfo);
    for (const Osc *x : oscs)
        if (x)
            add(x->pwm);
    add(v.tremolo);
    if (lfos.size() > 1)
        o.approx(QStringLiteral("%1 LFOs → one (its rate and wave: the first's)").arg(lfos.size()));
    return lfos.empty() ? -1 : lfos.front();
}
bool isLfo(const Graph &g, const Mod &m) { return m.slot >= 0 && g.type(m.slot) == u"lfo"; }
bool isEnv(const Graph &g, const Mod &m) { return m.slot >= 0 && g.type(m.slot) == u"envelope"; }
// LFO waves: Triangle, Sine, Saw up, Saw down, Square, Random, Smooth.
int lfoWave(const Graph &g, int lfo) { return lfo >= 0 ? int(std::lround(g.node(lfo)->knob("wave"))) : 0; }
const char *kLfoWaves[] = {"Triangle", "Sine", "Saw up", "Saw down", "Square", "Random", "Smooth"};

// ---- Kalliope -------------------------------------------------------------------------------

void kalliope(const Graph &g, const Voice &v, Out &o)
{
    const auto oscs = ordered(v);
    const Osc *a = !oscs.empty() ? oscs[0] : nullptr;
    const Osc *b = oscs.size() > 1 ? oscs[1] : nullptr;
    if (oscs.size() > 2)
        o.lost(QStringLiteral("%1 oscillators: Kalliope has two (the quietest left out)").arg(oscs.size()));

    // VCO-1: Triangle, Saw, Pulse, Square; 16' 8' 4' 2'.
    if (a) {
        const QString w = mainWave(a);
        const int wave = w == u"triangle" || w == u"sine" ? 0 : w == u"saw" ? 1 : w == u"pulse" ? 2 : 3;
        o.set(QStringLiteral("vco1_wave"), detent(wave, 4));
        if (w == u"sine")
            o.approx(QStringLiteral("VCO-1 sine → triangle (VCO-1 has no sine)"));
        else
            o.exact(QStringLiteral("VCO-1: %1").arg(w));
        o.set(QStringLiteral("vco1_range"), detent(int(a->octave) + 1, 4));
        if (a->octave < -1 || a->octave > 2)
            o.approx(QStringLiteral("VCO-1 octave %1 → %2 (Kalliope's 16'..2'; its OCTAVE knob shifts the rest)").arg(a->octave).arg(std::clamp(int(a->octave), -1, 2)));
        if (std::fabs(a->tune) > 0.05)
            o.approx(QStringLiteral("VCO-1 tuned %1 st → 0 (only VCO-2 has fine tuning)").arg(a->tune, 0, 'f', 1));
    }
    // VCO-2: Sine, Saw, Pulse, Noise; Low 16' 8' 4' 2'.
    double mix = 0.0;
    if (b) {
        const QString w = mainWave(b);
        const int wave = w == u"sine" || w == u"triangle" ? 0 : w == u"saw" ? 1 : 2;
        o.set(QStringLiteral("vco2_wave"), detent(wave, 4));
        if (w == u"triangle")
            o.approx(QStringLiteral("VCO-2 triangle → sine"));
        else
            o.exact(QStringLiteral("VCO-2: %1").arg(w));
        const double rel = b->octave + b->tune / 12.0;
        const int oct = int(std::lround(std::floor(rel + 0.5 / 12.0)));
        const double cents = (rel - oct) * 1200.0;
        o.set(QStringLiteral("vco2_range"), detent(std::clamp(oct + 2, 1, 4), 5));
        o.set(QStringLiteral("vco2_fine"), 0.5 + std::clamp(cents, -50.0, 50.0) / 100.0);
        if (std::fabs(cents) > 50.5 || oct < -1 || oct > 2)
            o.approx(QStringLiteral("VCO-2 tuned %1 st → the nearest octave and ±50 cents").arg(b->octave * 12 + b->tune, 0, 'f', 1));
        const double la = a->linFmFrom == b->slot && level(b) <= 0 ? 1.0 : level(a), lb = level(b);
        mix = la + lb > 0 ? lb / (la + lb) : 0.0;
    } else if (v.noise > 0) {
        o.set(QStringLiteral("vco2_wave"), detent(3, 4));
        o.set(QStringLiteral("vco2_range"), detent(2, 5));
        mix = a ? v.noise / (level(a) + v.noise) : 1.0;
        o.exact(QStringLiteral("Noise (as VCO-2)"));
    }
    if (b && v.noise > 0)
        o.lost(QStringLiteral("Noise (Kalliope's noise is a VCO-2 wave, and VCO-2 is used)"));
    o.set(QStringLiteral("mix"), mix);
    o.set(QStringLiteral("sync"), b && b->syncTo == a->slot ? 1.0 : 0.0);
    if (b && b->syncTo == a->slot)
        o.exact(QStringLiteral("Sync"));
    // Cross mod: VCO-2 shakes VCO-1's frequency (Konnekt's LIN FM, the same depth).
    o.set(QStringLiteral("cross_mod"), a && b && a->linFmFrom == b->slot ? a->linFm : 0.0);
    if (a && b && a->linFmFrom == b->slot)
        o.exact(QStringLiteral("FM (VCO-2 → VCO-1, CROSS MOD)"));
    if (v.ringA >= 0)
        o.lost(QStringLiteral("Ring Mod (Kalliope has none)"));

    // Pulse width: manual, LFO or ENV-1.
    const Osc *pw = a && (mainWave(a) == u"pulse" || a->pwm.slot >= 0) ? a : b;
    if (pw && pw->pwm.slot >= 0) {
        o.set(QStringLiteral("pwm"), pw->pwm.depth / 0.45);
        o.set(QStringLiteral("pwm_src"), detent(isLfo(g, pw->pwm) ? 0 : 2, 3));
        o.exact(QStringLiteral("PWM"));
    } else {
        o.set(QStringLiteral("pwm"), pw ? std::fabs(0.5 - pw->width) / 0.45 : 0.0);
        o.set(QStringLiteral("pwm_src"), detent(1, 3));
    }

    // Pitch: the LFO (1 oct) and ENV-1 (2 oct), to VCO-1, both or VCO-2.
    const Mod *lfoPitch = nullptr, *envPitch = nullptr;
    bool onA = false, onB = false;
    for (const Osc *x : {a, b})
        if (x) {
            if (isLfo(g, x->pitch)) {
                lfoPitch = &x->pitch;
                (x == a ? onA : onB) = true;
            }
            if (isEnv(g, x->pitch)) {
                envPitch = &x->pitch;
                (x == a ? onA : onB) = true;
            }
        }
    o.set(QStringLiteral("mod_lfo"), lfoPitch ? lfoPitch->depth / 1.0 : 0.0);
    o.set(QStringLiteral("mod_env"), envPitch && !envPitch->inverted ? envPitch->depth / 2.0 : 0.0);
    o.set(QStringLiteral("mod_dest"), detent(onA && onB ? 1 : onB ? 2 : 0, 3));
    if (lfoPitch)
        (lfoPitch->viaVca ? o.t.approx : o.t.exact) << (lfoPitch->viaVca ? QStringLiteral("Vibrato that fades in → LFO DELAY") : QStringLiteral("Vibrato (LFO → pitch)"));
    if (lfoPitch && lfoPitch->viaVca)
        o.set(QStringLiteral("lfo_delay"), 0.12);
    else
        o.set(QStringLiteral("lfo_delay"), 0.0);
    if (envPitch)
        envPitch->inverted ? o.lost(QStringLiteral("a pitch sweep up (ENV-1 only sweeps down)")) : o.approx(QStringLiteral("Pitch envelope → ENV-1 (shared with the filter)"));

    // Filter: the ladder, -12 or -24; a high-pass before it.
    o.set(QStringLiteral("hpf"), 0.0);
    if (v.filter >= 0) {
        const Node *f = g.node(v.filter);
        const QString t = f->type;
        const double cut = f->knob("cutoff");
        if (t == u"svf" && v.filterOut == u"high") {
            o.set(QStringLiteral("hpf"), clamp01(std::log(std::max(cut, 20.0) / 20.0) / std::log(50.0)));
            o.set(QStringLiteral("cutoff"), 1.0);
            o.approx(QStringLiteral("High-pass %1 → HPF (gentle, one pole)").arg(hz(cut)));
        } else {
            o.set(QStringLiteral("cutoff"), cutoff20k(cut));
            const double res = f->knob("res") * (t == u"ota_filter" ? 4.4 / 4.2 : 1.0);
            o.set(QStringLiteral("res"), res);
            o.set(QStringLiteral("slope"), t == u"ladder" ? f->knob("slope") : (t == u"ota_filter" ? 1.0 : 0.0));
            o.set(QStringLiteral("drive"), t == u"ladder" ? (f->knob("drive") - 1.0) / 7.0 : 0.0);
            if (t == u"ladder")
                o.exact(QStringLiteral("Ladder filter %1").arg(hz(cut)));
            else
                o.approx(QStringLiteral("%1 %2 → Kalliope's ladder").arg(g.name(v.filter), t == u"svf" ? v.filterOut + QStringLiteral("-pass") : QString()));
        }
    } else {
        o.set(QStringLiteral("cutoff"), 1.0);
        o.set(QStringLiteral("res"), 0.0);
        o.set(QStringLiteral("drive"), 0.0);
    }
    o.set(QStringLiteral("key_follow"), v.keyTrack ? 1.0 : 0.0);
    o.set(QStringLiteral("vcf_lfo"), isLfo(g, v.cutoffLfo) ? v.cutoffLfo.depth / 4.0 : 0.0);

    // ENV-1: the filter's (or the pitch's); ENV-2: the VCA's.
    const int fenv = isEnv(g, v.cutoffEnv) ? v.cutoffEnv.slot : envPitch ? envPitch->slot : -1;
    const Env amp = v.ampEnv >= 0 ? envOf(g, v.ampEnv) : Env{0.001, 0.01, 1.0, 0.005};
    const Env e1 = fenv >= 0 ? envOf(g, fenv) : amp;
    o.set(QStringLiteral("vcf_env"), isEnv(g, v.cutoffEnv) ? v.cutoffEnv.depth / 8.0 : 0.0);
    o.set(QStringLiteral("vcf_env_src"), isEnv(g, v.cutoffEnv) && v.cutoffEnv.slot == v.ampEnv ? 1.0 : 0.0);
    if (isEnv(g, v.cutoffEnv))
        o.exact(QStringLiteral("Filter envelope (%1 oct)").arg(v.cutoffEnv.depth, 0, 'f', 1));
    if (v.cutoffEnv.inverted)
        o.lost(QStringLiteral("an upside-down filter envelope"));
    const QStringList e1Keys{QStringLiteral("env1_a"), QStringLiteral("env1_d"), QStringLiteral("env1_s"), QStringLiteral("env1_r")};
    const QStringList e2Keys{QStringLiteral("env2_a"), QStringLiteral("env2_d"), QStringLiteral("env2_s"), QStringLiteral("env2_r")};
    for (const auto &[keys, e] : {std::pair{e1Keys, e1}, std::pair{e2Keys, amp}}) {
        o.set(keys[0], env10k(e.a));
        o.set(keys[1], env10k(e.d));
        o.set(keys[2], e.s);
        o.set(keys[3], env10k(e.r));
    }
    if (v.ampEnv >= 0)
        o.exact(QStringLiteral("Amp envelope: %1").arg(envText(amp)));
    else if (v.ampGate)
        o.exact(QStringLiteral("On while the key is held (ENV-2 as a gate)"));
    else if (v.vcaLevel > 0 || v.oscs.size())
        o.approx(QStringLiteral("A sound that never stops → ENV-2 as a gate (Kalliope's notes end)"));

    // The LFO, tremolo, voices.
    const int lfo = mainLfo(g, v, {a, b}, o);
    const int waveIdx = lfoWave(g, lfo);
    // Kalliope's waves: Sine, Up saw, Down saw, Square, Random.
    const int kw[] = {0, 0, 1, 2, 3, 4, 4};
    o.set(QStringLiteral("lfo_wave"), detent(kw[waveIdx], 5));
    o.set(QStringLiteral("lfo_rate"), lfo >= 0 ? clamp01(std::log(g.node(lfo)->knob("rate") / 0.05) / std::log(600.0)) : 0.5);
    if (lfo >= 0 && (waveIdx == 0 || waveIdx == 6))
        o.approx(QStringLiteral("LFO %1 → %2").arg(QString::fromLatin1(kLfoWaves[waveIdx]), waveIdx == 0 ? QStringLiteral("Sine") : QStringLiteral("Random")));
    o.set(QStringLiteral("vca_lfo"), isLfo(g, v.tremolo) ? v.tremolo.depth : 0.0);
    if (isLfo(g, v.tremolo))
        o.exact(QStringLiteral("Tremolo"));
    o.set(QStringLiteral("key_mode"), detent(g.voices <= 1 ? 2 : 0, 3));
    o.exact(g.voices <= 1 ? QStringLiteral("One voice → Solo") : QStringLiteral("%1 voices → Poly").arg(g.voices));
    if (v.glide > 0.002)
        o.lost(QStringLiteral("Glide (Kalliope has no portamento)"));
    if (v.drive >= 0)
        o.approx(QStringLiteral("Drive after the filter → the filter's DRIVE"));
    if (v.drive >= 0 && v.filter >= 0 && g.type(v.filter) != u"ladder")
        o.set(QStringLiteral("drive"), g.node(v.drive)->knob("drive"));
    o.set(QStringLiteral("play_mode"), 0.0);
    o.set(QStringLiteral("vca_level"), 0.8);
    effects(g, v, o, {QStringLiteral("Off"), QStringLiteral("I"), QStringLiteral("II")}, {1, 2, 2}, QStringLiteral("Kalliope"));
}

// ---- Kobalt ---------------------------------------------------------------------------------

void kobalt(const Graph &g, const Voice &v, Out &o)
{
    const auto oscs = ordered(v);
    const Osc *a = !oscs.empty() ? oscs[0] : nullptr;
    // One oscillator: its saw and pulse; a second an octave or two down can be the sub.
    double saw = 0, pulse = 0, sub = 0;
    int subType = 0;
    if (a) {
        for (const auto &[w, l] : a->waves) {
            if (w == u"saw")
                saw += l;
            else if (w == u"pulse")
                pulse += l;
            else {
                saw += l;
                o.approx(QStringLiteral("%1 wave → saw (Kobalt has saw and pulse)").arg(w));
            }
        }
        o.set(QStringLiteral("vco_range"), detent(int(a->octave) + 1, 4));
        if (a->octave < -1 || a->octave > 2)
            o.approx(QStringLiteral("Octave %1 → %2 (16'..2'; the OCTAVE knob shifts the rest)").arg(a->octave).arg(std::clamp(int(a->octave), -1, 2)));
        if (std::fabs(a->tune) > 0.05)
            o.approx(QStringLiteral("Tuned %1 st → 0").arg(a->tune, 0, 'f', 1));
        o.exact(QStringLiteral("Oscillator: saw %1 %, pulse %2 %").arg(saw * 100, 0, 'f', 0).arg(pulse * 100, 0, 'f', 0));
    }
    for (std::size_t i = 1; i < oscs.size(); ++i) {
        const Osc *x = oscs[i];
        const double down = (a ? a->octave + a->tune / 12.0 : 0) - (x->octave + x->tune / 12.0);
        if (sub == 0 && (std::fabs(down - 1) < 0.05 || std::fabs(down - 2) < 0.05) && level(x) > 0) {
            sub = level(x);
            subType = std::fabs(down - 1) < 0.05 ? 0 : (mainWave(x) == u"pulse" ? 2 : 1);
            o.approx(QStringLiteral("A second oscillator %1 octave%2 down → the sub-oscillator (a square)").arg(down, 0, 'f', 0).arg(down > 1.5 ? "s" : ""));
        } else
            o.lost(QStringLiteral("A second oscillator (Kobalt has one, plus a sub an octave or two down)"));
    }
    o.set(QStringLiteral("saw_level"), std::min(1.0, saw));
    o.set(QStringLiteral("pulse_level"), std::min(1.0, pulse));
    o.set(QStringLiteral("sub_level"), std::min(1.0, sub));
    o.set(QStringLiteral("sub_type"), detent(subType, 3));
    o.set(QStringLiteral("noise_level"), std::min(1.0, v.noise));
    if (v.noise > 0)
        o.exact(QStringLiteral("Noise"));
    if (v.ringA >= 0)
        o.lost(QStringLiteral("Ring Mod"));
    if (a && (a->syncTo >= 0 || a->linFmFrom >= 0))
        o.lost(QStringLiteral("Sync and FM between oscillators (Kobalt has one oscillator)"));

    // Pulse width: manual, LFO or envelope.
    if (a && a->pwm.slot >= 0) {
        o.set(QStringLiteral("pw"), a->pwm.depth / 0.45);
        o.set(QStringLiteral("pwm_src"), detent(isLfo(g, a->pwm) ? 0 : 2, 3));
        o.exact(QStringLiteral("PWM"));
    } else {
        o.set(QStringLiteral("pw"), a ? std::fabs(0.5 - a->width) / 0.45 : 0.0);
        o.set(QStringLiteral("pwm_src"), detent(1, 3));
    }
    // Vibrato: v² octaves.
    if (a && isLfo(g, a->pitch)) {
        o.set(QStringLiteral("vco_mod"), std::sqrt(std::max(0.0, a->pitch.depth)));
        (a->pitch.viaVca ? o.t.approx : o.t.exact) << (a->pitch.viaVca ? QStringLiteral("Vibrato that fades in → steady vibrato (or the mod lever)") : QStringLiteral("Vibrato"));
    } else
        o.set(QStringLiteral("vco_mod"), 0.0);
    if (a && isEnv(g, a->pitch))
        o.lost(QStringLiteral("Pitch envelope (Kobalt's envelope doesn't reach the pitch)"));

    // Filter: Kobalt's four-pole OTA.
    if (v.filter >= 0) {
        const Node *f = g.node(v.filter);
        o.set(QStringLiteral("cutoff"), cutoff20k(f->knob("cutoff")));
        o.set(QStringLiteral("res"), f->knob("res") * (f->type == u"ladder" ? 4.2 / 4.4 : 1.0));
        if (f->type == u"ota_filter")
            o.exact(QStringLiteral("OTA filter %1").arg(hz(f->knob("cutoff"))));
        else
            o.approx(QStringLiteral("%1 → Kobalt's four-pole low-pass").arg(g.name(v.filter)));
        if (f->type == u"svf" && v.filterOut != u"low")
            o.lost(QStringLiteral("the %1-pass (Kobalt's filter is low-pass)").arg(v.filterOut));
    } else {
        o.set(QStringLiteral("cutoff"), 1.0);
        o.set(QStringLiteral("res"), 0.0);
    }
    o.set(QStringLiteral("key_follow"), v.keyTrack ? 1.0 : 0.0);
    o.set(QStringLiteral("vcf_env"), isEnv(g, v.cutoffEnv) ? v.cutoffEnv.depth / 8.0 : 0.0);
    o.set(QStringLiteral("vcf_mod"), isLfo(g, v.cutoffLfo) ? v.cutoffLfo.depth / 4.0 : 0.0);

    // One envelope for filter and VCA.
    const bool sameEnv = !isEnv(g, v.cutoffEnv) || v.cutoffEnv.slot == v.ampEnv || v.ampEnv < 0;
    const int envSlot = v.ampEnv >= 0 ? v.ampEnv : isEnv(g, v.cutoffEnv) ? v.cutoffEnv.slot : -1;
    const Env e = envSlot >= 0 ? envOf(g, envSlot) : Env{0.001, 0.01, 1.0, 0.005};
    o.set(QStringLiteral("env_a"), env10k(e.a));
    o.set(QStringLiteral("env_d"), env10k(e.d));
    o.set(QStringLiteral("env_s"), e.s);
    o.set(QStringLiteral("env_r"), env10k(e.r));
    o.set(QStringLiteral("vca_mode"), v.ampGate || (v.ampEnv < 0 && isEnv(g, v.cutoffEnv)) ? 1.0 : 0.0);
    o.set(QStringLiteral("env_trig"), 0.0);
    if (!sameEnv)
        o.approx(QStringLiteral("Separate filter and amp envelopes → one (Kobalt's): the amp's"));
    else if (envSlot >= 0)
        o.exact(QStringLiteral("Envelope: %1").arg(envText(e)));
    if (isEnv(g, v.cutoffEnv))
        o.exact(QStringLiteral("Filter envelope (%1 oct)").arg(v.cutoffEnv.depth, 0, 'f', 1));
    if (v.ampGate)
        o.exact(QStringLiteral("VCA on the gate"));

    // LFO: Triangle, Square, Random, Noise.
    const int lfo = mainLfo(g, v, {a}, o);
    const int waveIdx = lfoWave(g, lfo);
    const int kw[] = {0, 0, 0, 0, 1, 2, 2};
    o.set(QStringLiteral("lfo_wave"), detent(kw[waveIdx], 4));
    o.set(QStringLiteral("lfo_rate"), lfo >= 0 ? clamp01(std::log(g.node(lfo)->knob("rate") / 0.1) / std::log(300.0)) : 0.5);
    if (lfo >= 0 && kw[waveIdx] == 0 && waveIdx != 0)
        o.approx(QStringLiteral("LFO %1 → Triangle").arg(QString::fromLatin1(kLfoWaves[waveIdx])));
    if (isLfo(g, v.tremolo))
        o.lost(QStringLiteral("Tremolo (Kobalt's LFO doesn't reach the VCA)"));

    // Glide; one voice.
    if (v.glide > 0.002) {
        o.set(QStringLiteral("porta_mode"), detent(2, 3));
        o.set(QStringLiteral("porta_time"), clamp01(std::log(v.glide * 3.0 / 0.005) / std::log(400.0)));
        o.approx(QStringLiteral("Glide %1 → PORTAMENTO On").arg(seconds(v.glide)));
    } else
        o.set(QStringLiteral("porta_mode"), 0.0);
    if (g.voices > 1)
        o.approx(QStringLiteral("%1 voices → one (Kobalt is monophonic)").arg(g.voices));
    else
        o.exact(QStringLiteral("One voice"));
    if (v.drive >= 0)
        o.lost(QStringLiteral("Drive (Kobalt has none)"));
    o.set(QStringLiteral("play_mode"), 0.0);
    effects(g, v, o, {}, {0, 0, 0}, QStringLiteral("Kobalt"));
}

// ---- Krimson --------------------------------------------------------------------------------

void krimson(const Graph &g, const Voice &v, Out &o)
{
    const auto oscs = ordered(v);
    const Osc *a = !oscs.empty() ? oscs[0] : nullptr;
    const Osc *b = oscs.size() > 1 ? oscs[1] : nullptr;
    if (oscs.size() > 2)
        o.lost(QStringLiteral("%1 oscillators: Krimson has two (the quietest left out)").arg(oscs.size()));
    // OSC 1: Sine, Triangle, Saw, Pulse. OSC 2: Triangle, Saw, Pulse, Noise.
    if (a) {
        const QString w = mainWave(a);
        o.set(QStringLiteral("osc1_wave"), detent(w == u"sine" ? 0 : w == u"triangle" ? 1 : w == u"saw" ? 2 : 3, 4));
        o.exact(QStringLiteral("OSC 1: %1").arg(w));
        if (std::fabs(a->octave) > 0.01 || std::fabs(a->tune) > 0.05)
            o.approx(QStringLiteral("OSC 1 at %1 st → 0 (set Krimson's OCTAVE: it isn't stored in a program)").arg(a->octave * 12 + a->tune, 0, 'f', 1));
    }
    double mix = 0.0;
    if (b) {
        const QString w = mainWave(b);
        o.set(QStringLiteral("osc2_wave"), detent(w == u"triangle" || w == u"sine" ? 0 : w == u"saw" ? 1 : 2, 4));
        if (w == u"sine")
            o.approx(QStringLiteral("OSC 2 sine → triangle"));
        else
            o.exact(QStringLiteral("OSC 2: %1").arg(w));
        const double rel = (b->octave - a->octave) * 12.0 + b->tune - a->tune;
        const int semi = int(std::lround(rel));
        o.set(QStringLiteral("osc2_semi"), 0.5 + std::clamp(semi, -24, 24) / 48.0);
        o.set(QStringLiteral("osc2_fine"), 0.5 + std::clamp((rel - semi) * 100.0, -50.0, 50.0) / 100.0);
        if (semi < -24 || semi > 24)
            o.approx(QStringLiteral("OSC 2 %1 st from OSC 1 → ±24").arg(semi));
        if (semi >= -24 && semi <= 24)
            o.exact(QStringLiteral("OSC 2 at %1 st").arg(rel, 0, 'f', 2));
        const double la = level(a), lb = level(b);
        mix = la + lb > 0 ? lb / (la + lb) : 0.5;
    } else if (v.noise > 0) {
        o.set(QStringLiteral("osc2_wave"), detent(3, 4));
        mix = a ? v.noise / (level(a) + v.noise) : 1.0;
        o.exact(QStringLiteral("Noise (as OSC 2)"));
    }
    if (b && v.noise > 0)
        o.lost(QStringLiteral("Noise (Krimson's noise is an OSC 2 wave, and OSC 2 is used)"));
    o.set(QStringLiteral("osc2_kbd"), 1.0);
    o.set(QStringLiteral("osc_mix"), mix);
    const bool ring = v.ringA >= 0 && b && ((v.ringA == a->slot && v.ringB == b->slot) || (v.ringA == b->slot && v.ringB == a->slot));
    o.set(QStringLiteral("ring"), ring ? 1.0 : 0.0);
    if (ring)
        o.exact(QStringLiteral("Ring Mod"));
    else if (v.ringA >= 0)
        o.lost(QStringLiteral("Ring Mod"));
    o.set(QStringLiteral("sync"), b && b->syncTo == a->slot ? 1.0 : 0.0);
    if (b && b->syncTo == a->slot)
        o.exact(QStringLiteral("Sync"));
    // FM: OSC 2 → OSC 1, ×(1 ± 3·fm); Konnekt's LIN FM is ×(1 + 4·amount).
    o.set(QStringLiteral("fm"), a && b && a->linFmFrom == b->slot ? a->linFm * 4.0 / 3.0 : 0.0);
    if (a && b && a->linFmFrom == b->slot)
        o.exact(QStringLiteral("FM (OSC 2 → OSC 1)"));
    const Osc *pw = a && (mainWave(a) == u"pulse" || a->pwm.slot >= 0) ? a : b;
    o.set(QStringLiteral("pw"), pw ? std::fabs(0.5 - pw->width) / 0.45 : 0.0);

    // Mod envelope (attack, decay) → OSC 2 pitch ±48 st, FM or PW; LFO 1 → one destination.
    o.set(QStringLiteral("menv_amt"), 0.5);
    const Osc *penv = a && isEnv(g, a->pitch) ? a : b && isEnv(g, b->pitch) ? b : nullptr;
    if (penv) {
        const Env e = envOf(g, penv->pitch.slot);
        const double st = penv->pitch.depth * 12.0 * (penv->pitch.inverted ? -1 : 1);
        o.set(QStringLiteral("menv_a"), env20k(e.a));
        o.set(QStringLiteral("menv_d"), env20k(e.d));
        o.set(QStringLiteral("menv_amt"), 0.5 + std::clamp(st / 48.0, -1.0, 1.0) / 2.0);
        o.set(QStringLiteral("menv_dest"), 0.0);
        o.approx(QStringLiteral("Pitch envelope → the MOD ENV on OSC 2's pitch%1").arg(penv == a ? QStringLiteral(" (it was on OSC 1)") : QString()));
    }
    const Osc *vib = a && isLfo(g, a->pitch) ? a : b && isLfo(g, b->pitch) ? b : nullptr;
    const int lfo = mainLfo(g, v, {a, b}, o);
    const int waveIdx = lfoWave(g, lfo);
    // LFO 1 waves: Triangle, Saw, Square, S&H, Soft random.
    const int kw[] = {0, 0, 1, 1, 2, 3, 4};
    o.set(QStringLiteral("lfo1_wave"), detent(kw[waveIdx], 5));
    o.set(QStringLiteral("lfo1_rate"), lfo >= 0 ? clamp01(std::log(g.node(lfo)->knob("rate") / 0.05) / std::log(800.0)) : 0.5);
    o.set(QStringLiteral("lfo1_amt"), 0.0);
    if (vib) {
        o.set(QStringLiteral("lfo1_dest"), detent(vib == b && !(a && isLfo(g, a->pitch)) ? 1 : 0, 5));
        o.set(QStringLiteral("lfo1_amt"), std::sqrt(std::max(0.0, vib->pitch.depth)));
        (vib->pitch.viaVca ? o.t.approx : o.t.exact) << (vib->pitch.viaVca ? QStringLiteral("Vibrato that fades in → steady vibrato (or the wheel)") : QStringLiteral("Vibrato (LFO 1)"));
        if (isLfo(g, v.cutoffLfo))
            o.approx(QStringLiteral("LFO on the filter → LFO 2 (a triangle)"));
    } else if (isLfo(g, v.cutoffLfo)) {
        o.set(QStringLiteral("lfo1_dest"), detent(2, 5));
        o.set(QStringLiteral("lfo1_amt"), v.cutoffLfo.depth / 4.0);
        o.exact(QStringLiteral("LFO on the filter (LFO 1)"));
    } else if (pw && isLfo(g, pw->pwm)) {
        o.set(QStringLiteral("lfo1_dest"), detent(3, 5));
        o.set(QStringLiteral("lfo1_amt"), pw->pwm.depth / 0.45);
        o.approx(QStringLiteral("PWM → LFO 1 on the pulse width"));
    }
    if (lfo >= 0 && (waveIdx == 1 || waveIdx == 3))
        o.approx(QStringLiteral("LFO %1 → %2").arg(QString::fromLatin1(kLfoWaves[waveIdx]), waveIdx == 1 ? QStringLiteral("Triangle") : QStringLiteral("Saw")));
    // LFO 2 (a triangle): tremolo, or the filter when LFO 1 is on pitch.
    o.set(QStringLiteral("lfo2_amt"), 0.0);
    if (isLfo(g, v.tremolo)) {
        o.set(QStringLiteral("lfo2_dest"), 0.0);
        o.set(QStringLiteral("lfo2_amt"), v.tremolo.depth);
        o.set(QStringLiteral("lfo2_rate"), clamp01(std::log(g.node(v.tremolo.slot)->knob("rate") / 0.05) / std::log(800.0)));
        o.exact(QStringLiteral("Tremolo (LFO 2)"));
    } else if (vib && isLfo(g, v.cutoffLfo)) {
        o.set(QStringLiteral("lfo2_dest"), detent(1, 3));
        o.set(QStringLiteral("lfo2_amt"), v.cutoffLfo.depth / 2.0);
        o.set(QStringLiteral("lfo2_rate"), clamp01(std::log(g.node(v.cutoffLfo.slot)->knob("rate") / 0.05) / std::log(800.0)));
    }

    // Filter: LP 12, LP 24, Band-pass, High-pass, Notch + LP.
    if (v.filter >= 0) {
        const Node *f = g.node(v.filter);
        int type = 1;
        if (f->type == u"ladder")
            type = f->knob("slope") > 0.5 ? 1 : 0;
        else if (f->type == u"svf")
            type = v.filterOut == u"band" ? 2 : v.filterOut == u"high" ? 3 : 0;
        o.set(QStringLiteral("filter_type"), detent(type, 5));
        o.set(QStringLiteral("cutoff"), cutoff20k(f->knob("cutoff")));
        o.set(QStringLiteral("res"), f->knob("res"));
        o.set(QStringLiteral("drive"), f->type == u"ladder" ? (f->knob("drive") - 1.0) / 7.0 : 0.0);
        o.exact(QStringLiteral("%1 %2 → %3").arg(g.name(v.filter), hz(f->knob("cutoff")),
                                                   QStringList{QStringLiteral("LP 12"), QStringLiteral("LP 24"), QStringLiteral("Band-pass"), QStringLiteral("High-pass")}[type]));
    } else {
        o.set(QStringLiteral("cutoff"), 1.0);
        o.set(QStringLiteral("res"), 0.0);
    }
    o.set(QStringLiteral("kbd_track"), detent(v.keyTrack ? 3 : 0, 4));
    o.set(QStringLiteral("fenv_amt"), isEnv(g, v.cutoffEnv) ? v.cutoffEnv.depth / 8.0 : 0.0);
    const Env amp = v.ampEnv >= 0 ? envOf(g, v.ampEnv) : Env{0.0005, 0.01, 1.0, 0.003};
    const Env fe = isEnv(g, v.cutoffEnv) ? envOf(g, v.cutoffEnv.slot) : amp;
    const QStringList fk{QStringLiteral("fenv_a"), QStringLiteral("fenv_d"), QStringLiteral("fenv_s"), QStringLiteral("fenv_r")};
    const QStringList ak{QStringLiteral("aenv_a"), QStringLiteral("aenv_d"), QStringLiteral("aenv_s"), QStringLiteral("aenv_r")};
    for (const auto &[keys, e] : {std::pair{fk, fe}, std::pair{ak, amp}}) {
        o.set(keys[0], env20k(e.a));
        o.set(keys[1], env20k(e.d));
        o.set(keys[2], e.s);
        o.set(keys[3], env20k(e.r));
    }
    if (isEnv(g, v.cutoffEnv))
        o.exact(QStringLiteral("Filter envelope (%1 oct)").arg(v.cutoffEnv.depth, 0, 'f', 1));
    if (v.ampEnv >= 0)
        o.exact(QStringLiteral("Amp envelope: %1").arg(envText(amp)));
    else if (v.ampGate)
        o.exact(QStringLiteral("On while the key is held"));
    else if (!v.oscs.empty())
        o.approx(QStringLiteral("A sound that never stops → notes that end with the key"));

    // Voices, glide, drive.
    o.set(QStringLiteral("voice_mode"), detent(g.voices <= 1 ? (v.glide > 0.002 ? 2 : 1) : 0, 4));
    o.exact(g.voices <= 1 ? QStringLiteral("One voice → %1").arg(v.glide > 0.002 ? QStringLiteral("Legato") : QStringLiteral("Mono"))
                          : QStringLiteral("%1 voices → Poly").arg(g.voices));
    // Glide: Krimson's engine time constant is (2·v²)/3; Konnekt's slew is a time to arrive.
    o.set(QStringLiteral("glide"), v.glide > 0.002 ? std::sqrt(std::min(1.0, v.glide * 3.0 / 2.0 / 3.0)) : 0.0);
    if (v.glide > 0.002)
        o.approx(QStringLiteral("Glide %1").arg(seconds(v.glide)));
    o.set(QStringLiteral("fx_drive"), v.drive >= 0 ? g.node(v.drive)->knob("drive") : 0.0);
    if (v.drive >= 0)
        o.approx(QStringLiteral("Drive → FX DRIVE"));
    o.set(QStringLiteral("arp_on"), 0.0);
    effects(g, v, o, {QStringLiteral("Off"), QStringLiteral("Chorus"), QStringLiteral("Ensemble")}, {1, 1, 2}, QStringLiteral("Krimson"));
}

} // namespace

QString targetName(SendTarget target)
{
    switch (target) {
    case SendTarget::Kalliope: return QStringLiteral("Kalliope");
    case SendTarget::Kobalt: return QStringLiteral("Kobalt");
    case SendTarget::Krimson: return QStringLiteral("Krimson");
    }
    return {};
}

QString targetId(SendTarget target) { return targetName(target).toLower(); }

Translation translate(const QJsonObject &patch, SendTarget target)
{
    const Graph g(patch);
    const Voice v = analyse(g);
    Out o;
    o.t.target = target;
    switch (target) {
    case SendTarget::Kalliope: kalliope(g, v, o); break;
    case SendTarget::Kobalt: kobalt(g, v, o); break;
    case SendTarget::Krimson: krimson(g, v, o); break;
    }
    o.t.lost.removeDuplicates();
    o.t.approx.removeDuplicates();
    // Exact counts fully, an approximation half; a loss counts twice against.
    const double total = double(o.t.exact.size() + o.t.approx.size() + 2 * o.t.lost.size());
    o.t.score = total > 0 ? int(std::lround(100.0 * (o.t.exact.size() + 0.5 * o.t.approx.size()) / total)) : 0;
    if (v.oscs.empty() && v.noise <= 0)
        o.t.score = 0;
    return o.t;
}

} // namespace konnekt
