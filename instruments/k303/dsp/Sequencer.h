#pragma once

#include <array>
#include <cstdint>

#include "core/StepClock.h"

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

// k303's 16-step sequencer (docs/SEQUENCER.md): the note, slide, tie, accent
// and gate rules. Timing and step order come from krate::StepClock, driven by
// the transport's beat position. Call tick() once per sample while running;
// it returns at most one event, which takes effect on that sample. Realtime-safe.
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

    Sequencer();

    void setPattern(const Pattern &pattern) noexcept;
    void setStep(int index, const Step &step) noexcept;  // ignored if out of range
    void setLength(int length) noexcept;                  // clamped to 1..16
    const Pattern &pattern() const noexcept { return m_pattern; }

    void setStepsPerBeat(int steps) noexcept { m_clock.setStepsPerBeat(steps); } // 4, 8 or 6
    void setPlayMode(PlayMode mode) noexcept;

    void startAt(double beat) noexcept; // running, the first step on that beat (joining a playing transport)
    void start() noexcept;
    // Returns GateOff if a note was sounding.
    SequencerEvent stop() noexcept;

    bool running() const noexcept { return m_clock.running(); }
    int currentStep() const noexcept { return m_clock.step(); }
    int stepsPerBeat() const noexcept { return m_clock.stepsPerBeat(); }

    // Once per sample while running, with that sample's beat position.
    SequencerEvent tick(double beat) noexcept;

private:
    SequencerEvent startStep() noexcept;
    bool continuesInto(int nextStep) const noexcept;

    Pattern m_pattern = demoPattern();
    krate::StepClock m_clock;
    PlayMode m_playMode = PlayMode::Forward;
    bool m_sounding = false;   // gate is high
};

} // namespace k303
