#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <span>

namespace konnekt {

// Konnekt's sizes (docs/BRIEF.md#scope-v1).
inline constexpr int kVoices = 8;      // per-voice modules run this many times (VOICES of them sound)
inline constexpr int kBlock = 16;      // samples processed at a time: a loop's cable is this late
inline constexpr int kMaxModules = 64;
inline constexpr int kMaxCables = 256;
inline constexpr int kMaxJacks = 12;   // inputs, and outputs, per module
inline constexpr int kMaxKnobs = 16;

// What a jack carries (docs/BRIEF.md#scope-v1): it sets the jack's colour and
// how its trace is drawn; any output can go to any input all the same.
enum class Signal : std::uint8_t {
    Audio, // ±1
    Pitch, // 1 per octave, 0 = C4
    Gate,  // 0 or 1 (on above 0.5); triggers are short gates
    Mod,   // modulation, ±1 (or 0..1)
};

enum class Curve : std::uint8_t { Linear, Exponential };

// A knob: its value is kept as a position 0..1 (what MIDI Learn and the panel
// move); value() turns it into its units. A knob with detents is a selector
// whose positions are labels[0..detents-1].
struct KnobInfo
{
    const char *key;   // stable: saved in patches
    const char *name;  // on the panel
    float min, max, defaultValue; // in its units
    Curve curve = Curve::Linear;
    int detents = 0;   // 0 = continuous
    const char *unit = "";
    const char *help = "";
    std::span<const char *const> labels = {};

    float value(float position) const noexcept
    {
        if (detents > 1)
            position = std::round(position * float(detents - 1)) / float(detents - 1);
        return curve == Curve::Exponential ? min * std::pow(max / min, position) : min + (max - min) * position;
    }
    float position(float v) const noexcept
    {
        const float p = curve == Curve::Exponential ? std::log(v / min) / std::log(max / min) : (v - min) / (max - min);
        return std::fmin(1.0f, std::fmax(0.0f, p));
    }
    int index(float position) const noexcept { return detents > 1 ? int(std::lround(position * float(detents - 1))) : 0; }
};

// A jack. An input with a `knob` reads that knob when unplugged (and adds it
// when plugged, as an offset); one with an `amount` knob scales what it gets.
struct JackInfo
{
    const char *key;
    const char *name;
    Signal signal;
    const char *help = "";
    int knob = -1;
    int amount = -1;
};

class Module;

struct ModuleInfo
{
    const char *type;     // stable: saved in patches
    const char *name;
    const char *category; // Sources, Filters, Envelopes, Modulation, Utilities, Effects, Sequencing, In / Out
    const char *help;
    bool perVoice;        // runs once per voice (else once)
    int width;            // on the rack, in columns
    std::span<const KnobInfo> knobs;
    std::span<const JackInfo> inputs;
    std::span<const JackInfo> outputs;
    Module *(*create)();
};

// What a module gets each block from the engine.
struct MidiVoice
{
    float pitch = 0.0f;     // octaves from C4, with the bend
    float gate = 0.0f;
    float velocity = 0.0f;
    float pressure = 0.0f;
};
struct ProcessContext
{
    int voices = 1;          // channels to process (1 for a one-voice module)
    int frames = kBlock;
    double rate = 48000.0;
    // in[jack][voice]: the cable's samples, or nullptr when unplugged.
    std::array<std::array<const float *, kVoices>, kMaxJacks> in{};
    std::array<std::array<float *, kVoices>, kMaxJacks> out{};
    // The MIDI In module's: each voice, and the controls all voices share.
    const MidiVoice *midi = nullptr;
    float bend = 0.0f, modWheel = 0.0f; // -1..1, 0..1
    float *output[2] = {nullptr, nullptr}; // the Output module's: Konnekt's own output, added to
};

// One module. Knobs are positions 0..1, written by the panel or MIDI Learn,
// read by the audio thread each block. Made and freed on the GUI thread; the
// engine only processes it.
class Module
{
public:
    explicit Module(const ModuleInfo &info) : m_info(info)
    {
        for (std::size_t k = 0; k < info.knobs.size() && k < std::size_t(kMaxKnobs); ++k)
            m_knobs[k].store(info.knobs[k].position(info.knobs[k].defaultValue), std::memory_order_relaxed);
    }
    virtual ~Module() = default;

    const ModuleInfo &info() const noexcept { return m_info; }
    // Before processing (and when the sample rate changes): no allocation.
    virtual void prepare(double rate) noexcept { (void)rate; }
    virtual void process(const ProcessContext &context) noexcept = 0;

    float knobPosition(int k) const noexcept { return m_knobs[std::size_t(k)].load(std::memory_order_relaxed); }
    void setKnobPosition(int k, float p) noexcept
    {
        if (k >= 0 && k < kMaxKnobs)
            m_knobs[std::size_t(k)].store(std::fmin(1.0f, std::fmax(0.0f, p)), std::memory_order_relaxed);
    }
    // The knob in its units, and a selector's position.
    float knob(int k) const noexcept { return m_info.knobs[std::size_t(k)].value(knobPosition(k)); }
    int choice(int k) const noexcept { return m_info.knobs[std::size_t(k)].index(knobPosition(k)); }

protected:
    // Input `jack` of `voice` at sample `i`: what's plugged in (scaled by its
    // amount knob) plus its own knob, or just the knob, or 0.
    float input(const ProcessContext &c, int jack, int voice, int i) const noexcept
    {
        const JackInfo &j = m_info.inputs[std::size_t(jack)];
        const float *x = c.in[std::size_t(jack)][std::size_t(voice)];
        float v = j.knob >= 0 ? knob(j.knob) : 0.0f;
        if (x)
            v += j.amount >= 0 ? x[i] * knob(j.amount) : x[i];
        return v;
    }
    bool plugged(const ProcessContext &c, int jack) const noexcept { return c.in[std::size_t(jack)][0] != nullptr; }

private:
    const ModuleInfo &m_info;
    std::array<std::atomic<float>, kMaxKnobs> m_knobs{};
};

// Pitch (octaves from C4) to Hz.
inline double pitchHz(double octaves) noexcept { return 261.6255653005986 * std::exp2(octaves); }

} // namespace konnekt
