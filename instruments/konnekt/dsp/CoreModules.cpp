// The core modules (docs/BRIEF.md#modules-v1): MIDI In, Oscillator, Envelope,
// LFO, VCA, Mixer, Attenuverter, Output. Each one's table is what the panel
// draws and the help bar says.

#include <algorithm>
#include <cmath>

#include "dsp/BlepOscillator.h"
#include "dsp/Envelope.h"
#include "dsp/Lfo.h"
#include "konnekt/dsp/Modules.h"

namespace konnekt::modules {

namespace {
using S = Signal;
constexpr Curve kExp = Curve::Exponential;
} // namespace

// --- MIDI In -------------------------------------------------------------------------

namespace {
constexpr KnobInfo kMidiKnobs[] = {
    {"bend_range", "Bend", 0.0f, 12.0f, 2.0f, Curve::Linear, 13, "st",
     "How far the pitch bend wheel moves PITCH, in semitones."},
};
constexpr JackInfo kMidiOutputs[] = {
    {"pitch", "Pitch", S::Pitch, "The note each voice plays, 1 per octave (0 = middle C), with the bend. Into an oscillator's PITCH."},
    {"gate", "Gate", S::Gate, "1 while the key is held, 0 when let go. Into an envelope's GATE."},
    {"velocity", "Velocity", S::Mod, "How hard the key was played, 0..1. Into a VCA or a filter's cutoff for dynamics."},
    {"pressure", "Pressure", S::Mod, "Aftertouch, 0..1: pressing harder into a held key."},
    {"mod_wheel", "Mod Wheel", S::Mod, "The mod wheel, 0..1 (all voices). Try it on an LFO's amount through a VCA."},
    {"bend", "Bend", S::Mod, "The pitch bend wheel, -1..1 (all voices), for bending something other than pitch."},
};

class MidiIn final : public Module
{
public:
    MidiIn() : Module(kMidiIn) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float bend = c.bend * knob(0) / 12.0f;
        for (int v = 0; v < c.voices; ++v) {
            const MidiVoice m = c.midi ? c.midi[v] : MidiVoice{};
            const float values[] = {m.pitch + bend, m.gate, m.velocity, m.pressure, c.modWheel, c.bend};
            for (int j = 0; j < 6; ++j)
                std::fill_n(c.out[std::size_t(j)][std::size_t(v)], c.frames, values[j]);
        }
    }
};
} // namespace

const ModuleInfo kMidiIn{"midi_in", "MIDI In", "In / Out",
                         "Your keyboard: each note gets a voice, and each voice its PITCH, GATE and VELOCITY. "
                         "Start every patch here: PITCH into an oscillator, GATE into an envelope.",
                         true, 2, kMidiKnobs, {}, kMidiOutputs, []() -> Module * { return new MidiIn; }};

// --- Oscillator ----------------------------------------------------------------------

namespace {
constexpr const char *kOctaves[] = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
constexpr KnobInfo kOscKnobs[] = {
    {"octave", "Octave", -3.0f, 3.0f, 0.0f, Curve::Linear, 7, "oct", "Shifts the pitch by whole octaves.", kOctaves},
    {"tune", "Tune", -12.0f, 12.0f, 0.0f, Curve::Linear, 0, "st", "Shifts the pitch up to an octave either way. A few cents off another oscillator makes them beat: thicker."},
    {"width", "Width", 0.05f, 0.95f, 0.5f, Curve::Linear, 0, "", "The PULSE output's width: 50 % is a square (hollow); narrower is thinner and nasal."},
    {"fm_amount", "FM", 0.0f, 4.0f, 0.0f, Curve::Linear, 0, "oct", "How far the FM input moves the pitch (octaves at full input). An LFO here is vibrato; an envelope, a pitch sweep."},
    {"lin_fm_amount", "Lin FM", 0.0f, 1.0f, 0.0f, Curve::Linear, 0, "", "How much the LIN FM input moves the frequency. Another oscillator here makes bells and clangs (FM synthesis)."},
    {"pwm_amount", "PWM", 0.0f, 0.45f, 0.0f, Curve::Linear, 0, "", "How far the PWM input moves the pulse width. A slow LFO here is the classic swirling PWM sound."},
};
constexpr JackInfo kOscInputs[] = {
    {"pitch", "Pitch", S::Pitch, "The note to play, 1 per octave. From MIDI In's PITCH (or a sequencer).", -1, -1},
    {"fm", "FM", S::Mod, "Moves the pitch, scaled by FM: vibrato from an LFO, sweeps from an envelope.", -1, 3},
    {"lin_fm", "Lin FM", S::Audio, "Moves the frequency itself, scaled by LIN FM: another oscillator here gives FM synthesis.", -1, 4},
    {"pwm", "PWM", S::Mod, "Moves the pulse width, scaled by PWM.", -1, 5},
    {"sync", "Sync", S::Audio, "Hard sync: each time this signal rises through 0 the oscillator starts its cycle again. Plug another oscillator in, tune this one higher: the tearing sync sound.", -1, -1},
};
constexpr JackInfo kOscOutputs[] = {
    {"saw", "Saw", S::Audio, "Bright and buzzy: every harmonic. Brass, strings, basses."},
    {"pulse", "Pulse", S::Audio, "Hollow to nasal, with WIDTH: odd harmonics at 50 %."},
    {"triangle", "Triangle", S::Audio, "Soft, a little flute-like: few harmonics."},
    {"sine", "Sine", S::Audio, "Pure: just the note. For subs, and as an FM modulator."},
};

class Oscillator final : public Module
{
public:
    Oscillator() : Module(kOscillator)
    {
        for (auto &voice : m_osc) {
            voice[0].setWave(krate::Wave::Saw);
            voice[1].setWave(krate::Wave::Pulse);
            voice[2].setWave(krate::Wave::Triangle);
            voice[3].setWave(krate::Wave::Sine);
        }
    }
    void process(const ProcessContext &c) noexcept override
    {
        const float base = knob(0) + knob(1) / 12.0f;
        const float width = knob(2);
        for (int v = 0; v < c.voices; ++v) {
            auto &osc = m_osc[std::size_t(v)];
            float &prevSync = m_prevSync[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const double octaves = base + input(c, 0, v, i) + input(c, 1, v, i);
                double increment = pitchHz(octaves) / c.rate;
                if (plugged(c, 2))
                    increment *= 1.0 + 4.0 * double(input(c, 2, v, i));
                increment = std::clamp(increment, 0.0, 0.45);
                const double pw = std::clamp(double(width + input(c, 3, v, i)), 0.05, 0.95);
                double syncT = -1.0;
                if (plugged(c, 4)) {
                    const float s = c.in[4][std::size_t(v)][i];
                    if (prevSync < 0.0f && s >= 0.0f)
                        syncT = double(s) / double(s - prevSync); // how long ago it crossed, in samples
                    prevSync = s;
                }
                for (int w = 0; w < 4; ++w)
                    c.out[std::size_t(w)][std::size_t(v)][i] = osc[std::size_t(w)].next(increment, pw, syncT);
            }
        }
    }

private:
    std::array<std::array<krate::Oscillator, 4>, kVoices> m_osc;
    std::array<float, kVoices> m_prevSync{};
};
} // namespace

const ModuleInfo kOscillator{"oscillator", "Oscillator", "Sources",
                             "Makes the raw tone: four waves at once from one pitch. Choose a wave by which output you "
                             "plug in; it then wants a filter to shape it and a VCA to start and stop it.",
                             true, 4, kOscKnobs, kOscInputs, kOscOutputs, []() -> Module * { return new Oscillator; }};

// --- Envelope ------------------------------------------------------------------------

namespace {
constexpr KnobInfo kEnvKnobs[] = {
    {"attack", "Attack", 0.001f, 10.0f, 0.005f, kExp, 0, "s", "How long it takes to rise to full when the gate opens."},
    {"decay", "Decay", 0.001f, 10.0f, 0.3f, kExp, 0, "s", "How long it takes to fall from full to SUSTAIN."},
    {"sustain", "Sustain", 0.0f, 1.0f, 0.6f, Curve::Linear, 0, "", "The level it holds while the gate stays open."},
    {"release", "Release", 0.001f, 10.0f, 0.3f, kExp, 0, "s", "How long it takes to fall to 0 after the gate closes."},
};
constexpr JackInfo kEnvInputs[] = {
    {"gate", "Gate", S::Gate, "Opening it starts the envelope (attack, decay, sustain); closing it releases. From MIDI In's GATE, or a sequencer's."},
    {"retrig", "Retrig", S::Gate, "A trigger here starts the attack again while the gate is held: rhythmic plucks from a clock or an LFO."},
};
constexpr JackInfo kEnvOutputs[] = {
    {"env", "Env", S::Mod, "The envelope, 0..1. Into a VCA's CV to shape the volume of each note, or a filter's cutoff for a sweep."},
    {"inverted", "Inverted", S::Mod, "1 minus the envelope: falls when the other rises."},
};

class EnvelopeModule final : public Module
{
public:
    EnvelopeModule() : Module(kEnvelope) {}
    void prepare(double rate) noexcept override
    {
        for (auto &e : m_env) {
            e.setSampleRate(rate);
            e.kill();
        }
    }
    void process(const ProcessContext &c) noexcept override
    {
        const krate::Envelope::Times times{knob(0), knob(1), knob(2), knob(3)};
        for (int v = 0; v < c.voices; ++v) {
            krate::Envelope &e = m_env[std::size_t(v)];
            e.setTimes(times);
            bool &gate = m_gate[std::size_t(v)], &retrig = m_retrig[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const bool g = input(c, 0, v, i) > 0.5f, r = input(c, 1, v, i) > 0.5f;
                if (g && !gate)
                    e.gateOn();
                else if (!g && gate)
                    e.gateOff();
                else if (g && r && !retrig)
                    e.gateOn();
                gate = g;
                retrig = r;
                const float level = float(e.next());
                c.out[0][std::size_t(v)][i] = level;
                c.out[1][std::size_t(v)][i] = 1.0f - level;
            }
        }
    }

private:
    std::array<krate::Envelope, kVoices> m_env;
    std::array<bool, kVoices> m_gate{}, m_retrig{};
};
} // namespace

const ModuleInfo kEnvelope{"envelope", "Envelope", "Envelopes",
                           "A shape over time, started by a gate: up (attack), down a little (decay), held (sustain), "
                           "away when the key is let go (release). What makes a note start and stop.",
                           true, 3, kEnvKnobs, kEnvInputs, kEnvOutputs, []() -> Module * { return new EnvelopeModule; }};

// --- LFO -----------------------------------------------------------------------------

namespace {
constexpr const char *kLfoWaves[] = {"Triangle", "Sine", "Saw up", "Saw down", "Square", "Random", "Smooth"};
constexpr KnobInfo kLfoKnobs[] = {
    {"rate", "Rate", 0.02f, 50.0f, 2.0f, kExp, 0, "Hz", "How fast it moves: from a slow drift (0.1 Hz) to a fast flutter."},
    {"wave", "Wave", 0.0f, 6.0f, 0.0f, Curve::Linear, 7, "", "Its shape: smooth (triangle, sine), ramps (saws), on/off (square), steps (random) or glides (smooth).", kLfoWaves},
    {"rate_amount", "Rate Mod", 0.0f, 4.0f, 0.0f, Curve::Linear, 0, "oct", "How far the RATE input speeds it up or slows it down."},
};
constexpr JackInfo kLfoInputs[] = {
    {"rate", "Rate", S::Mod, "Speeds it up or slows it down, scaled by RATE MOD.", -1, 2},
    {"reset", "Reset", S::Gate, "Starts its cycle again on a trigger: plug MIDI In's GATE in so every note starts the wobble the same way."},
};
constexpr JackInfo kLfoOutputs[] = {
    {"out", "Out", S::Mod, "The wave, -1..1: into an oscillator's FM for vibrato, a filter's cutoff for a wah, PWM for a swirl."},
    {"unipolar", "0..1", S::Mod, "The same wave, 0..1: for things that shouldn't go below their knob, like a VCA (tremolo)."},
};

class LfoModule final : public Module
{
public:
    LfoModule() : Module(kLfo) {}
    void process(const ProcessContext &c) noexcept override
    {
        static constexpr krate::Lfo::Wave kWaves[] = {krate::Lfo::Wave::Triangle, krate::Lfo::Wave::Sine,
                                                       krate::Lfo::Wave::UpSaw,    krate::Lfo::Wave::DownSaw,
                                                       krate::Lfo::Wave::Square,   krate::Lfo::Wave::Random,
                                                       krate::Lfo::Wave::SoftRandom};
        const auto wave = kWaves[choice(1)];
        const double rate = knob(0);
        for (int v = 0; v < c.voices; ++v) {
            krate::Lfo &lfo = m_lfo[std::size_t(v)];
            bool &reset = m_reset[std::size_t(v)];
            for (int i = 0; i < c.frames; ++i) {
                const bool r = input(c, 1, v, i) > 0.5f;
                if (r && !reset)
                    lfo.reset();
                reset = r;
                const double hz = rate * std::exp2(double(input(c, 0, v, i)));
                const float y = float(lfo.next(hz, wave, c.rate));
                c.out[0][std::size_t(v)][i] = y;
                c.out[1][std::size_t(v)][i] = 0.5f + 0.5f * y;
            }
        }
    }

private:
    std::array<krate::Lfo, kVoices> m_lfo;
    std::array<bool, kVoices> m_reset{};
};
} // namespace

const ModuleInfo kLfo{"lfo", "LFO", "Modulation",
                      "A low-frequency oscillator: too slow to hear, it moves other things. Vibrato, wah, tremolo, "
                      "swirling pulse width, random burbles: plug it into what you want to move.",
                      true, 3, kLfoKnobs, kLfoInputs, kLfoOutputs, []() -> Module * { return new LfoModule; }};

// --- VCA -----------------------------------------------------------------------------

namespace {
constexpr const char *kResponses[] = {"Linear", "Exponential"};
constexpr KnobInfo kVcaKnobs[] = {
    {"level", "Level", 0.0f, 1.0f, 0.0f, Curve::Linear, 0, "", "How open it is with no control: at 0 it's closed until the CV opens it (an envelope per note); turn it up to hear the input all the time."},
    {"cv_amount", "CV", 0.0f, 1.0f, 1.0f, Curve::Linear, 0, "", "How far the CV input opens it."},
    {"response", "Response", 0.0f, 1.0f, 0.0f, Curve::Linear, 2, "", "Linear: level follows the CV. Exponential: quiet parts quieter, more natural fades.", kResponses},
};
constexpr JackInfo kVcaInputs[] = {
    {"in", "In", S::Audio, "The sound (or any signal) to make louder or quieter."},
    {"cv", "CV", S::Mod, "What opens it: an envelope for notes, an LFO for tremolo, velocity for dynamics.", 0, 1},
};
constexpr JackInfo kVcaOutputs[] = {
    {"out", "Out", S::Audio, "IN times the gain: silence when closed."},
};

class Vca final : public Module
{
public:
    Vca() : Module(kVca) {}
    void process(const ProcessContext &c) noexcept override
    {
        const bool exponential = choice(2) == 1;
        for (int v = 0; v < c.voices; ++v)
            for (int i = 0; i < c.frames; ++i) {
                float g = std::clamp(input(c, 1, v, i), 0.0f, 1.0f);
                if (exponential)
                    g = g * g * g;
                c.out[0][std::size_t(v)][i] = input(c, 0, v, i) * g;
            }
    }
};
} // namespace

const ModuleInfo kVca{"vca", "VCA", "Utilities",
                      "A voltage-controlled amplifier: a volume knob something else turns. An envelope into its CV "
                      "is what makes a note start and stop.",
                      true, 2, kVcaKnobs, kVcaInputs, kVcaOutputs, []() -> Module * { return new Vca; }};

// --- Mixer ---------------------------------------------------------------------------

namespace {
constexpr KnobInfo kMixerKnobs[] = {
    {"level1", "1", 0.0f, 1.0f, 0.7f, Curve::Linear, 0, "", "Input 1's level."},
    {"level2", "2", 0.0f, 1.0f, 0.7f, Curve::Linear, 0, "", "Input 2's level."},
    {"level3", "3", 0.0f, 1.0f, 0.7f, Curve::Linear, 0, "", "Input 3's level."},
    {"level4", "4", 0.0f, 1.0f, 0.7f, Curve::Linear, 0, "", "Input 4's level."},
};
constexpr JackInfo kMixerInputs[] = {
    {"in1", "1", S::Audio, "A signal to mix in, at level 1."},
    {"in2", "2", S::Audio, "A signal to mix in, at level 2."},
    {"in3", "3", S::Audio, "A signal to mix in, at level 3."},
    {"in4", "4", S::Audio, "A signal to mix in, at level 4."},
};
constexpr JackInfo kMixerOutputs[] = {
    {"out", "Out", S::Audio, "The four inputs added up at their levels."},
};

class Mixer final : public Module
{
public:
    Mixer() : Module(kMixer) {}
    void process(const ProcessContext &c) noexcept override
    {
        for (int v = 0; v < c.voices; ++v)
            for (int i = 0; i < c.frames; ++i) {
                float sum = 0.0f;
                for (int j = 0; j < 4; ++j)
                    if (const float *x = c.in[std::size_t(j)][std::size_t(v)])
                        sum += x[i] * knob(j);
                c.out[0][std::size_t(v)][i] = sum;
            }
    }
};
} // namespace

const ModuleInfo kMixer{"mixer", "Mixer", "Utilities",
                        "Adds signals together: two oscillators into one filter, several modulations into one input.",
                        true, 2, kMixerKnobs, kMixerInputs, kMixerOutputs, []() -> Module * { return new Mixer; }};

// --- Attenuverter ----------------------------------------------------------------------

namespace {
constexpr KnobInfo kAttKnobs[] = {
    {"scale_a", "Scale A", -1.0f, 1.0f, 1.0f, Curve::Linear, 0, "", "A's input times this: less of it, none (middle), or turned upside down (left)."},
    {"offset_a", "Offset A", -2.0f, 2.0f, 0.0f, Curve::Linear, 0, "", "Added to A. With nothing plugged in, OUT A is just this: a steady voltage (to set a pitch, open a VCA...)."},
    {"scale_b", "Scale B", -1.0f, 1.0f, 1.0f, Curve::Linear, 0, "", "B's input times this."},
    {"offset_b", "Offset B", -2.0f, 2.0f, 0.0f, Curve::Linear, 0, "", "Added to B."},
};
constexpr JackInfo kAttInputs[] = {
    {"a", "A", S::Mod, "A signal to scale and offset."},
    {"b", "B", S::Mod, "Another signal to scale and offset."},
};
constexpr JackInfo kAttOutputs[] = {
    {"out_a", "Out A", S::Mod, "A times SCALE A, plus OFFSET A."},
    {"out_b", "Out B", S::Mod, "B times SCALE B, plus OFFSET B."},
};

class Attenuverter final : public Module
{
public:
    Attenuverter() : Module(kAttenuverter) {}
    void process(const ProcessContext &c) noexcept override
    {
        for (int ch = 0; ch < 2; ++ch) {
            const float scale = knob(2 * ch), offset = knob(2 * ch + 1);
            for (int v = 0; v < c.voices; ++v) {
                const float *x = c.in[std::size_t(ch)][std::size_t(v)];
                float *y = c.out[std::size_t(ch)][std::size_t(v)];
                for (int i = 0; i < c.frames; ++i)
                    y[i] = (x ? x[i] * scale : 0.0f) + offset;
            }
        }
    }
};
} // namespace

const ModuleInfo kAttenuverter{"attenuverter", "Attenuverter", "Utilities",
                               "Scales a signal (less, none, or upside down) and shifts it: the basic tool for getting "
                               "a modulation to the right size. Unplugged, its OFFSET is a steady voltage.",
                               true, 2, kAttKnobs, kAttInputs, kAttOutputs, []() -> Module * { return new Attenuverter; }};

// --- Output --------------------------------------------------------------------------

namespace {
constexpr KnobInfo kOutKnobs[] = {
    {"volume", "Volume", 0.0f, 1.0f, 0.7f, Curve::Linear, 0, "", "How loud it all is."},
};
constexpr JackInfo kOutInputs[] = {
    {"left", "Left", S::Audio, "To the left speaker (and the right, if RIGHT is empty). The voices are mixed here."},
    {"right", "Right", S::Audio, "To the right speaker."},
};

class Output final : public Module
{
public:
    Output() : Module(kOutput) {}
    void process(const ProcessContext &c) noexcept override
    {
        const float gain = knob(0) * knob(0);
        const float *l = c.in[0][0], *r = c.in[1][0] ? c.in[1][0] : l;
        if (!l)
            l = r;
        if (!l || !c.output[0])
            return;
        for (int i = 0; i < c.frames; ++i) {
            c.output[0][i] += gain * l[i];
            c.output[1][i] += gain * r[i];
        }
    }
};
} // namespace

const ModuleInfo kOutput{"output", "Output", "In / Out",
                         "Where the sound leaves Konnekt for your speakers. Nothing is heard until something is "
                         "plugged in here.",
                         false, 2, kOutKnobs, kOutInputs, {}, []() -> Module * { return new Output; }};

} // namespace konnekt::modules

namespace konnekt {

std::span<const ModuleInfo *const> moduleTypes()
{
    static const ModuleInfo *const kTypes[] = {&modules::kMidiIn,  &modules::kOscillator,   &modules::kEnvelope,
                                               &modules::kLfo,     &modules::kVca,          &modules::kMixer,
                                               &modules::kAttenuverter, &modules::kOutput};
    return kTypes;
}

const ModuleInfo *findModule(std::string_view type)
{
    for (const ModuleInfo *m : moduleTypes())
        if (type == m->type)
            return m;
    return nullptr;
}

} // namespace konnekt
