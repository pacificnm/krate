// The other modules (docs/BRIEF.md#modules-v1): Noise, the three filters,
// Sample & hold, Slew, Drive, Crusher, Ring mod, Clock, Sequencer, Chorus,
// Delay, Reverb, Scope.

#include <algorithm>
#include <cmath>
#include <numbers>

#include "dsp/Effects.h"
#include "dsp/Reverb.h"
#include "kalliope/dsp/Filters.h"
#include "kobalt/dsp/Filter.h"
#include "konnekt/dsp/Modules.h"

namespace konnekt::modules {

namespace {
using S = Signal;
constexpr Curve kExp = Curve::Exponential;

class Random
{
public:
    explicit Random(std::uint32_t seed) : m_state(seed) {}
    float next() noexcept // -1..1
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return float(std::int32_t(m_state)) / 2147483648.0f;
    }

private:
    std::uint32_t m_state;
};
} // namespace

// --- Noise ---------------------------------------------------------------------------

namespace {
constexpr JackInfo kNoiseOutputs[] = {
    {"white", "White", S::Audio, "Every frequency equally: a bright hiss. Into a filter for wind, snares, hats."},
    {"pink", "Pink", S::Audio, "Less treble, more weight: like rain or surf. Good for slow random modulation too."},
};

class Noise final : public Module
{
public:
    Noise() : Module(kNoise) {}
    void process(const ProcessContext &c) noexcept override
    {
        for (int v = 0; v < c.voices; ++v) {
            auto &b = m_pink[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const float w = m_random.next();
                // Pink: the sum of first-order filters (Kellet's economy method).
                b[0] = 0.99765f * b[0] + w * 0.0990460f;
                b[1] = 0.96300f * b[1] + w * 0.2965164f;
                b[2] = 0.57000f * b[2] + w * 1.0526913f;
                c.out[0][std::size_t(v)][i] = 0.5f * w;
                c.out[1][std::size_t(v)][i] = 0.12f * (b[0] + b[1] + b[2] + w * 0.1848f);
            }
        }
    }

private:
    Random m_random{0x2545F491u};
    std::array<std::array<float, 3>, kVoices> m_pink{};
};
} // namespace

const ModuleInfo kNoise{"noise", "Noise", "Sources",
                        "Random signal: hiss to filter into wind, breath and percussion, or to feed a Sample & hold "
                        "for random steps.",
                        true, 2, {}, {}, kNoiseOutputs, []() -> Module * { return new Noise; }};

// --- Filters -------------------------------------------------------------------------

namespace {
// The three filters share their controls: cutoff, resonance, how far the CUTOFF
// input moves it, and a KEY input that tracks pitch one to one.
#define KONNEKT_FILTER_KNOBS(resMax)                                                                                   \
    {"cutoff", "Cutoff", 20.0f, 20000.0f, 1000.0f, kExp, 0, "Hz", "Where it starts to cut: low is dark, high is open."},  \
        {"res", "Resonance", 0.0f, resMax, 0.0f, Curve::Linear, 0, "",                                                  \
         "A peak at the cutoff: a little makes it vocal, a lot whistles and rings (and at the top it sings on its own)."}, \
        {"cv_amount", "CV", 0.0f, 8.0f, 2.0f, Curve::Linear, 0, "oct",                                                  \
         "How far the CUTOFF input moves it, in octaves at full input. An envelope here is the classic filter sweep."}
constexpr JackInfo kFilterInputs[] = {
    {"in", "In", S::Audio, "The sound to filter: an oscillator, noise, a mix."},
    {"cutoff", "Cutoff", S::Mod, "Moves the cutoff, scaled by CV: an envelope for a sweep per note, an LFO for a wah.", -1, 2},
    {"key", "Key", S::Pitch, "Pitch, 1 octave per octave: the cutoff follows the notes, so high ones aren't duller. From MIDI In's PITCH."},
};

constexpr const char *kSlopes[] = {"-12 dB", "-24 dB"};
constexpr KnobInfo kLadderKnobs[] = {
    KONNEKT_FILTER_KNOBS(1.0f),
    {"drive", "Drive", 1.0f, 8.0f, 1.0f, Curve::Linear, 0, "", "How hard the sound goes in: warmth, then grit; lots of it tames the resonance."},
    {"slope", "Slope", 0.0f, 1.0f, 1.0f, Curve::Linear, 2, "", "-12 dB per octave (gentler, brighter) or -24 (steeper, the classic).", kSlopes},
};
constexpr JackInfo kLowOutput[] = {
    {"out", "Out", S::Audio, "The filtered sound."},
};

// The cutoff at sample i: the knob, moved by the CUTOFF input (in octaves, scaled) and KEY (1:1).
float cutoffAt(const ProcessContext &c, float baseHz, int i, const float *cv, float amount, const float *key)
{
    double octaves = std::log2(double(baseHz));
    if (cv)
        octaves += double(cv[i]) * amount;
    if (key)
        octaves += key[i];
    return float(std::clamp(std::exp2(octaves), 10.0, 0.45 * c.rate));
}

class Ladder final : public Module
{
public:
    Ladder() : Module(kLadder) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float k = 4.2f * knob(1), drive = knob(3);
        const bool slope24 = choice(4) == 1;
        for (int v = 0; v < c.voices; ++v) {
            kalliope::Ladder &f = m_filter[std::size_t(v)];
            const float *x = c.in[0][std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                f.set(cutoffAt(c, knob(0), i, c.in[1][std::size_t(v)], knob(2), c.in[2][std::size_t(v)]), k, c.rate, drive);
                c.out[0][std::size_t(v)][i] = f.process(x ? x[i] : 0.0f, slope24);
            }
        }
    }

private:
    std::array<kalliope::Ladder, kVoices> m_filter{};
};

constexpr KnobInfo kOtaKnobs[] = {KONNEKT_FILTER_KNOBS(1.0f)};
class OtaFilter final : public Module
{
public:
    OtaFilter() : Module(kOtaFilter) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float k = 4.4f * knob(1);
        for (int v = 0; v < c.voices; ++v) {
            kobalt::Filter &f = m_filter[std::size_t(v)];
            const float *x = c.in[0][std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                f.set(cutoffAt(c, knob(0), i, c.in[1][std::size_t(v)], knob(2), c.in[2][std::size_t(v)]), k, c.rate);
                c.out[0][std::size_t(v)][i] = f.process(x ? x[i] : 0.0f);
            }
        }
    }

private:
    std::array<kobalt::Filter, kVoices> m_filter{};
};

constexpr KnobInfo kSvfKnobs[] = {KONNEKT_FILTER_KNOBS(1.0f)};
constexpr JackInfo kSvfOutputs[] = {
    {"low", "Low", S::Audio, "Low-pass: what's below the cutoff passes."},
    {"band", "Band", S::Audio, "Band-pass: only what's around the cutoff: thin, vocal, telephone."},
    {"high", "High", S::Audio, "High-pass: what's above the cutoff passes: thin, airy."},
};
class Svf final : public Module
{
public:
    Svf() : Module(kSvf) {}
    void process(const ProcessContext &c) noexcept override
    {
        const double k = 2.0 * (1.0 - 0.98 * double(knob(1)));
        for (int v = 0; v < c.voices; ++v) {
            double &ic1 = m_s[std::size_t(v)][0], &ic2 = m_s[std::size_t(v)][1];
            const float *x = c.in[0][std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const double fc = cutoffAt(c, knob(0), i, c.in[1][std::size_t(v)], knob(2), c.in[2][std::size_t(v)]);
                const double g = std::tan(std::numbers::pi * fc / c.rate);
                const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const double in = x ? x[i] : 0.0;
                const double v3 = in - ic2;
                const double v1 = a1 * ic1 + a2 * v3;
                const double v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0 * v1 - ic1;
                ic2 = 2.0 * v2 - ic2;
                c.out[0][std::size_t(v)][i] = float(v2);
                c.out[1][std::size_t(v)][i] = float(v1);
                c.out[2][std::size_t(v)][i] = float(in - k * v1 - v2);
            }
        }
    }

private:
    std::array<std::array<double, 2>, kVoices> m_s{};
};
#undef KONNEKT_FILTER_KNOBS
} // namespace

const ModuleInfo kLadder{"ladder", "Ladder Filter", "Filters",
                         "A transistor-ladder low-pass, as in Kalliope: smooth and warm, -12 or -24 dB, resonance up "
                         "to self-oscillation, drive for grit. Takes the brightness off a raw oscillator.",
                         true, 3, kLadderKnobs, kFilterInputs, kLowOutput, []() -> Module * { return new Ladder; }};
const ModuleInfo kOtaFilter{"ota_filter", "OTA Filter", "Filters",
                            "A four-pole low-pass as in Kobalt: every stage saturates, so a loud resonant note buzzes "
                            "and rubbers instead of getting louder.",
                            true, 3, kOtaKnobs, kFilterInputs, kLowOutput, []() -> Module * { return new OtaFilter; }};
const ModuleInfo kSvf{"svf", "State-Variable Filter", "Filters",
                      "Low-, band- and high-pass at once from one filter: compare them by moving the cable between "
                      "its outputs.",
                      true, 3, kSvfKnobs, kFilterInputs, kSvfOutputs, []() -> Module * { return new Svf; }};

// --- Sample & hold -----------------------------------------------------------------------

namespace {
constexpr JackInfo kShInputs[] = {
    {"in", "In", S::Mod, "What to sample. Unplugged: its own noise, for random steps."},
    {"trigger", "Trigger", S::Gate, "Each rising trigger takes a new sample: a clock or an LFO's square."},
};
constexpr JackInfo kShOutputs[] = {
    {"out", "Out", S::Mod, "The value taken at the last trigger, held until the next: stepped, the 'computer' burble into a filter's cutoff."},
};
class SampleHold final : public Module
{
public:
    SampleHold() : Module(kSampleHold) {}
    void process(const ProcessContext &c) noexcept override
    {
        for (int v = 0; v < c.voices; ++v) {
            float &held = m_held[std::size_t(v)];
            bool &trig = m_trig[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const float in = c.in[0][std::size_t(v)] ? c.in[0][std::size_t(v)][i] : m_random.next();
                const bool t = input(c, 1, v, i) > 0.5f;
                if (t && !trig)
                    held = in;
                trig = t;
                c.out[0][std::size_t(v)][i] = held;
            }
        }
    }

private:
    Random m_random{0x6C8E9CF5u};
    std::array<float, kVoices> m_held{};
    std::array<bool, kVoices> m_trig{};
};
} // namespace

const ModuleInfo kSampleHold{"sample_hold", "Sample & Hold", "Modulation",
                             "Takes a snapshot of a signal on each trigger and holds it: turns a smooth or noisy signal "
                             "into steps.",
                             true, 2, {}, kShInputs, kShOutputs, []() -> Module * { return new SampleHold; }};

// --- Slew ----------------------------------------------------------------------------------

namespace {
constexpr KnobInfo kSlewKnobs[] = {
    {"rise", "Rise", 0.0005f, 5.0f, 0.1f, kExp, 0, "s", "How long it takes to climb to a higher value."},
    {"fall", "Fall", 0.0005f, 5.0f, 0.1f, kExp, 0, "s", "How long it takes to sink to a lower one."},
};
constexpr JackInfo kSlewInputs[] = {
    {"in", "In", S::Mod, "A signal that jumps: a pitch from MIDI In (for glide), a sequencer, stepped random."},
};
constexpr JackInfo kSlewOutputs[] = {
    {"out", "Out", S::Mod, "The same, smoothed: pitch glides between notes (portamento), steps become curves."},
};
class Slew final : public Module
{
public:
    Slew() : Module(kSlew) {}
    void process(const ProcessContext &c) noexcept override
    {
        const double rise = 1.0 - std::exp(-1.0 / (double(knob(0)) * c.rate / 4.6));
        const double fall = 1.0 - std::exp(-1.0 / (double(knob(1)) * c.rate / 4.6));
        for (int v = 0; v < c.voices; ++v) {
            double &y = m_y[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const double x = input(c, 0, v, i);
                y += (x - y) * (x > y ? rise : fall);
                c.out[0][std::size_t(v)][i] = float(y);
            }
        }
    }

private:
    std::array<double, kVoices> m_y{};
};
} // namespace

const ModuleInfo kSlew{"slew", "Slew", "Modulation",
                       "Smooths jumps into glides: put it between MIDI In's PITCH and an oscillator for portamento.",
                       true, 2, kSlewKnobs, kSlewInputs, kSlewOutputs, []() -> Module * { return new Slew; }};

// --- Drive -----------------------------------------------------------------------------------

namespace {
constexpr KnobInfo kDriveKnobs[] = {
    {"drive", "Drive", 0.0f, 1.0f, 0.3f, Curve::Linear, 0, "", "How hard it pushes: warm, then crunchy, then fuzz."},
    {"fold", "Fold", 0.0f, 1.0f, 0.0f, Curve::Linear, 0, "", "Folds the loud peaks back over themselves: metallic, harsh, many new harmonics."},
    {"level", "Level", 0.0f, 1.0f, 0.7f, Curve::Linear, 0, "", "Its output level (driving harder gets louder)."},
};
constexpr JackInfo kAudioIn[] = {
    {"in", "In", S::Audio, "The sound to process."},
};
constexpr JackInfo kAudioOut[] = {
    {"out", "Out", S::Audio, "The processed sound."},
};
class Drive final : public Module
{
public:
    Drive() : Module(kDrive) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float gain = 1.0f + 15.0f * knob(0) * knob(0), fold = knob(1), level = knob(2);
        const float norm = 1.0f / std::tanh(gain);
        for (int v = 0; v < c.voices; ++v) {
            const float *x = c.in[0][std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const float in = x ? x[i] : 0.0f;
                const float clipped = std::tanh(in * gain) * norm;
                const float folded = std::sin(in * gain * 0.5f * float(std::numbers::pi));
                c.out[0][std::size_t(v)][i] = level * ((1.0f - fold) * clipped + fold * folded);
            }
        }
    }
};

constexpr const char *kBitLabels[] = {"Off", "16", "15", "14", "13", "12", "11", "10", "9", "8", "7", "6", "5", "4", "3", "2"};
constexpr const char *kRateLabels[] = {"1", "1/2", "1/4", "1/8", "1/16", "1/32"};
constexpr KnobInfo kCrusherKnobs[] = {
    {"bits", "Bits", 0.0f, 15.0f, 0.0f, Curve::Linear, 16, "", "Fewer bits, coarser steps: grit, then a buzzing square at 2.", kBitLabels},
    {"rate", "Rate", 0.0f, 5.0f, 0.0f, Curve::Linear, 6, "", "Holds each sample for longer: lo-fi, with a whining alias.", kRateLabels},
};
class Crusher final : public Module
{
public:
    Crusher() : Module(kCrusher) {}
    void process(const ProcessContext &c) noexcept override
    {
        const int b = choice(0), hold = 1 << choice(1);
        const float q = b == 0 ? 0.0f : float(1 << (16 - b));
        for (int v = 0; v < c.voices; ++v) {
            const float *x = c.in[0][std::size_t(v)];
            int &count = m_count[std::size_t(v)];
            float &held = m_held[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                if (count == 0)
                    held = x ? x[i] : 0.0f;
                count = (count + 1) % hold;
                // n bits: 2^n levels from -1 up to just under +1, as a converter has.
                c.out[0][std::size_t(v)][i] = q > 0.0f ? std::clamp(std::floor(held * q + 0.5f), -q, q - 1.0f) / q : held;
            }
        }
    }

private:
    std::array<int, kVoices> m_count{};
    std::array<float, kVoices> m_held{};
};

constexpr KnobInfo kRingKnobs[] = {
    {"mix", "Mix", 0.0f, 1.0f, 1.0f, Curve::Linear, 0, "", "How much of the product: 0 is just A, 1 all ring modulation."},
};
constexpr JackInfo kRingInputs[] = {
    {"a", "A", S::Audio, "One signal: a voice."},
    {"b", "B", S::Audio, "The other: another oscillator for bells and clangs, an LFO for tremolo."},
};
class RingMod final : public Module
{
public:
    RingMod() : Module(kRingMod) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float mix = knob(0);
        for (int v = 0; v < c.voices; ++v) {
            const float *a = c.in[0][std::size_t(v)], *b = c.in[1][std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const float x = a ? a[i] : 0.0f, y = b ? b[i] : 0.0f;
                c.out[0][std::size_t(v)][i] = (1.0f - mix) * x + mix * x * y;
            }
        }
    }
};
} // namespace

const ModuleInfo kDrive{"drive", "Drive", "Effects",
                        "Distortion: rounds off the loud parts (warmth, crunch, fuzz) and with FOLD wraps them over "
                        "(harsh, metallic).",
                        true, 2, kDriveKnobs, kAudioIn, kAudioOut, []() -> Module * { return new Drive; }};
const ModuleInfo kCrusher{"crusher", "Crusher", "Effects",
                          "Digital damage: fewer bits and a lower sample rate, as on the oldest samplers.",
                          true, 2, kCrusherKnobs, kAudioIn, kAudioOut, []() -> Module * { return new Crusher; }};
const ModuleInfo kRingMod{"ring_mod", "Ring Mod", "Utilities",
                          "Multiplies two signals: two oscillators give bell and robot tones (their sum and "
                          "difference frequencies), an audio signal by an LFO gives tremolo.",
                          true, 2, kRingKnobs, kRingInputs, kAudioOut, []() -> Module * { return new RingMod; }};

// --- Clock -------------------------------------------------------------------------------------

namespace {
constexpr const char *kRunLabels[] = {"Stop", "Run"};
constexpr KnobInfo kClockKnobs[] = {
    {"tempo", "Tempo", 40.0f, 240.0f, 120.0f, Curve::Linear, 0, "BPM", "Beats per minute."},
    {"run", "Run", 0.0f, 1.0f, 1.0f, Curve::Linear, 2, "", "Start and stop the clock.", kRunLabels},
};
constexpr JackInfo kClockInputs[] = {
    {"reset", "Reset", S::Gate, "Starts again from the top of the bar on a trigger."},
};
constexpr JackInfo kClockOutputs[] = {
    {"bar", "Bar", S::Gate, "A trigger every bar (4 beats)."},
    {"beat", "Beat", S::Gate, "A trigger every beat (a quarter note)."},
    {"eighth", "1/8", S::Gate, "A trigger every eighth note."},
    {"sixteenth", "1/16", S::Gate, "A trigger every sixteenth note: drive a sequencer from here."},
};
class Clock final : public Module
{
public:
    Clock() : Module(kClock) {}
    void process(const ProcessContext &c) noexcept override
    {
        const bool run = choice(1) == 1;
        const double step = run ? double(knob(0)) / 60.0 / c.rate : 0.0; // beats per sample
        static constexpr double kPerBeat[] = {0.25, 1.0, 2.0, 4.0};
        for (int i = 0; i < c.frames; ++i) {
            const bool r = input(c, 0, 0, i) > 0.5f;
            if (r && !m_reset)
                m_beat = 0.0;
            m_reset = r;
            for (int j = 0; j < 4; ++j) {
                const double ticks = m_beat * kPerBeat[j];
                // High for the first 40 % of each tick (a gate long enough to see and to play).
                c.out[std::size_t(j)][0][i] = run && (ticks - std::floor(ticks)) < 0.4 ? 1.0f : 0.0f;
            }
            m_beat += step;
        }
    }

private:
    double m_beat = 0.0;
    bool m_reset = false;
};
} // namespace

const ModuleInfo kClock{"clock", "Clock", "Sequencing",
                        "Ticks in time: bars, beats, eighths, sixteenths, as gates. Drives a sequencer, retriggers "
                        "envelopes, steps a Sample & hold.",
                        false, 2, kClockKnobs, kClockInputs, kClockOutputs, []() -> Module * { return new Clock; }};

// --- Sequencer ---------------------------------------------------------------------------------

namespace {
constexpr const char *kOnOff[] = {"Off", "On"};
#define KONNEKT_STEP(n)                                                                                                \
    {"pitch" #n, #n, -12.0f, 12.0f, 0.0f, Curve::Linear, 25, "st", "Step " #n "'s note, in semitones from the root."}
#define KONNEKT_GATE(n)                                                                                                \
    {"gate" #n, "On " #n, 0.0f, 1.0f, 1.0f, Curve::Linear, 2, "", "Whether step " #n " plays (off: a rest).", kOnOff}
constexpr KnobInfo kSeqKnobs[] = {
    KONNEKT_STEP(1), KONNEKT_STEP(2), KONNEKT_STEP(3), KONNEKT_STEP(4),
    KONNEKT_STEP(5), KONNEKT_STEP(6), KONNEKT_STEP(7), KONNEKT_STEP(8),
    KONNEKT_GATE(1), KONNEKT_GATE(2), KONNEKT_GATE(3), KONNEKT_GATE(4),
    KONNEKT_GATE(5), KONNEKT_GATE(6), KONNEKT_GATE(7), KONNEKT_GATE(8),
    {"length", "Length", 1.0f, 8.0f, 8.0f, Curve::Linear, 8, "", "How many steps before it starts again."},
    {"gate_length", "Gate", 0.05f, 1.0f, 0.5f, Curve::Linear, 0, "", "How much of each step the gate stays open: short is staccato, full is legato."},
};
#undef KONNEKT_STEP
#undef KONNEKT_GATE
constexpr JackInfo kSeqInputs[] = {
    {"clock", "Clock", S::Gate, "Each rising tick moves to the next step: a Clock's 1/16 or 1/8."},
    {"reset", "Reset", S::Gate, "Back to step 1 on a trigger: a Clock's BAR keeps it in line."},
    {"transpose", "Transpose", S::Pitch, "Added to the pitch: MIDI In's PITCH here plays the pattern from the key you hold."},
};
constexpr JackInfo kSeqOutputs[] = {
    {"pitch", "Pitch", S::Pitch, "The step's note (1 per octave): into an oscillator's PITCH."},
    {"gate", "Gate", S::Gate, "Open for GATE of each step that's on: into an envelope's GATE."},
    {"step", "Step", S::Mod, "Which step is playing, 0..1 across the steps: a staircase to modulate other things with."},
};
class Sequencer final : public Module
{
public:
    Sequencer() : Module(kSequencer) {}
    void process(const ProcessContext &c) noexcept override
    {
        const int length = int(std::lround(knob(16)));
        const float gateLength = knob(17);
        for (int i = 0; i < c.frames; ++i) {
            const bool clock = input(c, 0, 0, i) > 0.5f, reset = input(c, 1, 0, i) > 0.5f;
            if (reset && !m_reset)
                m_step = -1; // the next tick is step 1
            m_reset = reset;
            if (clock && !m_clock) {
                m_step = (m_step + 1) % std::max(1, length);
                m_period = std::max(m_sinceTick, 1);
                m_sinceTick = 0;
            }
            m_clock = clock;
            ++m_sinceTick;
            const int s = std::max(0, m_step);
            const bool on = m_step >= 0 && choice(8 + s) == 1 && m_sinceTick <= int(gateLength * float(m_period) + 0.5f);
            c.out[0][0][i] = knob(s) / 12.0f + input(c, 2, 0, i);
            c.out[1][0][i] = on ? 1.0f : 0.0f;
            c.out[2][0][i] = float(s) / float(std::max(1, length - 1));
        }
    }

private:
    int m_step = -1;
    bool m_clock = false, m_reset = false;
    int m_sinceTick = 0, m_period = 12000;
};
} // namespace

const ModuleInfo kSequencer{"sequencer", "Sequencer", "Sequencing",
                            "Eight steps, each a note and on or off, played one per clock tick: a bass part or a riff "
                            "that plays itself. Its PITCH and GATE replace MIDI In's.",
                            false, 8, kSeqKnobs, kSeqInputs, kSeqOutputs, []() -> Module * { return new Sequencer; }};

// --- Stereo effects --------------------------------------------------------------------------

namespace {
constexpr JackInfo kStereoIn[] = {
    {"left", "Left", S::Audio, "The sound in (the voices are mixed here). Mono: plug just this."},
    {"right", "Right", S::Audio, "The right side, for a stereo sound; empty: the left's."},
};
constexpr JackInfo kStereoOut[] = {
    {"left", "Left", S::Audio, "The left side out: into the Output's LEFT."},
    {"right", "Right", S::Audio, "The right side out: into the Output's RIGHT."},
};

constexpr const char *kChorusModes[] = {"I", "II", "Ensemble"};
constexpr KnobInfo kChorusKnobs[] = {
    {"mode", "Mode", 0.0f, 2.0f, 0.0f, Curve::Linear, 3, "", "I: slow and gentle. II: faster, deeper. Ensemble: lush, strings.", kChorusModes},
};
constexpr JackInfo kMonoIn[] = {
    {"in", "In", S::Audio, "The sound in (the voices are mixed here)."},
};
class ChorusModule final : public Module
{
public:
    ChorusModule() : Module(kChorus) {}
    void prepare(double rate) noexcept override { m_chorus.prepare(rate); }
    void process(const ProcessContext &c) noexcept override
    {
        const float *x = c.in[0][0] ? c.in[0][0] : m_silence.data();
        m_chorus.process(x, c.out[0][0], c.out[1][0], std::uint32_t(c.frames), 1 + choice(0));
    }

private:
    krate::Chorus m_chorus;
    std::array<float, kBlock> m_silence{};
};

constexpr KnobInfo kDelayKnobs[] = {
    {"time", "Time", 0.01f, 2.0f, 0.375f, kExp, 0, "s", "How long until the echo: short doubles, long repeats."},
    {"feedback", "Feedback", 0.0f, 0.9f, 0.4f, Curve::Linear, 0, "", "How many echoes: each one feeds the next."},
    {"mix", "Mix", 0.0f, 1.0f, 0.35f, Curve::Linear, 0, "", "How loud the echoes are against the sound."},
};
class DelayModule final : public Module
{
public:
    DelayModule() : Module(kDelay) {}
    void prepare(double rate) noexcept override
    {
        m_delay.prepare(rate);
        m_delay.clear();
    }
    void process(const ProcessContext &c) noexcept override
    {
        for (int ch = 0; ch < 2; ++ch) {
            const float *x = c.in[std::size_t(ch)][0] ? c.in[std::size_t(ch)][0] : c.in[0][0];
            for (int i = 0; i < c.frames; ++i)
                c.out[std::size_t(ch)][0][i] = x ? x[i] : 0.0f;
        }
        m_delay.process(c.out[0][0], c.out[1][0], std::uint32_t(c.frames), knob(0), knob(2), knob(1));
    }

private:
    krate::Delay m_delay{2.0, 192000.0};
};

constexpr KnobInfo kReverbKnobs[] = {
    {"size", "Size", 0.0f, 1.0f, 0.5f, Curve::Linear, 0, "", "The room: a small room (0.4 s) to a huge hall (8 s)."},
    {"mix", "Mix", 0.0f, 1.0f, 0.3f, Curve::Linear, 0, "", "How much of the room you hear."},
};
class ReverbModule final : public Module
{
public:
    ReverbModule() : Module(kReverb) {}
    void prepare(double rate) noexcept override
    {
        m_reverb.prepare(rate);
        m_reverb.clear();
    }
    void process(const ProcessContext &c) noexcept override
    {
        for (int ch = 0; ch < 2; ++ch) {
            const float *x = c.in[std::size_t(ch)][0] ? c.in[std::size_t(ch)][0] : c.in[0][0];
            for (int i = 0; i < c.frames; ++i)
                c.out[std::size_t(ch)][0][i] = x ? x[i] : 0.0f;
        }
        m_reverb.process(c.out[0][0], c.out[1][0], std::uint32_t(c.frames), knob(0), knob(1));
    }

private:
    krate::Reverb m_reverb{192000.0};
};

// --- Scope ------------------------------------------------------------------------------------

constexpr const char *kScopeTimes[] = {"5 ms", "20 ms", "80 ms"};
constexpr KnobInfo kScopeKnobs[] = {
    {"time", "Time", 0.0f, 2.0f, 1.0f, Curve::Linear, 3, "", "How much time the screen shows across: 5 ms for a waveform's shape, 80 ms for its movement.", kScopeTimes},
};
constexpr JackInfo kScopeInputs[] = {
    {"in", "In", S::Audio, "What to look at. Plug in anything: a wave, an envelope, an LFO."},
};
constexpr JackInfo kScopeOutputs[] = {
    {"thru", "Thru", S::Audio, "What comes in, unchanged: put the scope in the middle of a cable."},
};
class Scope final : public Module
{
public:
    Scope() : Module(kScope) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float *x = c.in[0][0];
        for (int i = 0; i < c.frames; ++i)
            c.out[0][0][i] = x ? x[i] : 0.0f;
    }
};
} // namespace

const ModuleInfo kChorus{"chorus", "Chorus", "Effects",
                         "Thickens a sound with slowly wavering copies, and spreads it to stereo.",
                         false, 2, kChorusKnobs, kMonoIn, kStereoOut, []() -> Module * { return new ChorusModule; }};
const ModuleInfo kDelay{"delay", "Delay", "Effects",
                        "Echoes, bouncing left and right; each one a little darker.",
                        false, 2, kDelayKnobs, kStereoIn, kStereoOut, []() -> Module * { return new DelayModule; }};
const ModuleInfo kReverb{"reverb", "Reverb", "Effects",
                         "Puts the sound in a room, from a small one to a cathedral.",
                         false, 2, kReverbKnobs, kStereoIn, kStereoOut, []() -> Module * { return new ReverbModule; }};
const ModuleInfo kScope{"scope", "Scope", "In / Out",
                        "A big screen for a signal: see the shape of a wave, an envelope, an LFO as it plays.",
                        false, 4, kScopeKnobs, kScopeInputs, kScopeOutputs, []() -> Module * { return new Scope; }};

} // namespace konnekt::modules
