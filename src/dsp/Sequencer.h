#pragma once

#include <array>
#include <cstdint>

namespace k303 {

enum class Gate : std::uint8_t { Note, Tie, Rest };

struct Step
{
    std::uint8_t note = 0;     // 0..12 = C..C
    std::int8_t octave = 0;    // -1 down, 0 normal, +1 up
    bool accent = false;
    bool slide = false;        // slide FROM this step INTO the next
    Gate gate = Gate::Note;

    bool operator==(const Step &) const = default;
};

struct Pattern
{
    std::array<Step, 16> steps{};
    std::uint8_t length = 16;  // 1..16
};

// docs/SEQUENCER.md: C1 (24) at octave -1 up to C4 (60) at octave +1, note 12.
constexpr int stepMidiNote(const Step &step)
{
    return 24 + 12 * (step.octave + 1) + step.note;
}

// Built-in riffs that fill the first pattern slots of a new install.
// Factory pattern 0 is also the demo pattern used by the offline renderer.
inline constexpr int kFactoryPatternCount = 4;
Pattern factoryPattern(int index); // 0..kFactoryPatternCount-1
inline Pattern demoPattern() { return factoryPattern(0); }

struct SequencerEvent
{
    enum class Kind : std::uint8_t { None, NoteOn, SlideTo, GateOff };
    Kind kind = Kind::None;
    int note = 0;
    bool accent = false;
};

// 16-step sequencer timed by sample count (docs/SEQUENCER.md). Call tick()
// once per sample at the base rate; it returns at most one event, which takes
// effect on that sample. Realtime-safe.
class Sequencer
{
public:
    static constexpr double kGateLength = 0.5; // fraction of a step

    // Order matches the PLAY MODE selector.
    enum class PlayMode : std::uint8_t {
        Forward,
        Reverse,
        PingPong,   // Fwd+Rev: bounces without repeating the end steps
        Invert,     // forward, pitches mirrored around C: C stays, D# <-> A, G <-> F
        Random,
    };

    void setSampleRate(double sampleRate) noexcept;
    void setTempo(double bpm) noexcept;
    void setStepsPerBeat(int steps) noexcept;      // 4 = 16ths, 8 = 32nds, 6 = 16th triplets
    void setPlayMode(PlayMode mode) noexcept { m_playMode = mode; }
    void setPattern(const Pattern &pattern) noexcept;
    void setStep(int index, const Step &step) noexcept;  // ignored if out of range
    void setLength(int length) noexcept;                  // clamped to 1..16
    const Pattern &pattern() const noexcept { return m_pattern; }

    void start() noexcept;
    // Returns GateOff if a note was sounding.
    SequencerEvent stop() noexcept;

    bool running() const noexcept { return m_running; }
    int currentStep() const noexcept { return m_running ? m_step : -1; }
    double samplesPerStep() const noexcept { return m_samplesPerStep; }
    // True while the current step starts a beat (for the beat LED).
    bool onBeat() const noexcept { return m_running && m_stepCount % m_stepsPerBeat == 0; }

    SequencerEvent tick() noexcept;

private:
    void updateSamplesPerStep() noexcept;
    SequencerEvent startStep() noexcept;
    bool continuesInto(int nextStep) const noexcept;
    int firstStep() noexcept;
    void chooseNextStep() noexcept;  // sets m_next (and m_nextDirection)
    int nextStepIndex() const noexcept { return m_next < m_pattern.length ? m_next : 0; }
    std::uint32_t random() noexcept; // xorshift32: realtime-safe

    Pattern m_pattern = demoPattern();
    double m_sampleRate = 48000.0;
    double m_bpm = 120.0;
    double m_samplesPerStep = 6000.0;
    double m_phase = 0.0;      // samples into the current step
    int m_step = -1;
    int m_next = 0;
    int m_direction = 1;       // ping-pong: +1 / -1
    int m_nextDirection = 1;
    int m_stepsPerBeat = 4;
    long m_stepCount = -1;     // steps played since start
    PlayMode m_playMode = PlayMode::Forward;
    std::uint32_t m_rng = 0x9E3779B9u;
    int m_prevStep = -1;
    bool m_running = false;
    bool m_sounding = false;   // gate is high
    bool m_gateChecked = false;
};

} // namespace k303
