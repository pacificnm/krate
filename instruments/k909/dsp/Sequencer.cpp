#include "dsp/Sequencer.h"

#include <algorithm>
#include <string_view>

namespace k909 {

namespace {

// One string of 16 steps per track: '.' off, 'x' on, 'X' accent.
struct TrackSteps
{
    DrumId drum;
    std::string_view steps;
};

template<std::size_t N>
Pattern makePattern(const TrackSteps (&tracks)[N], int length = 16)
{
    Pattern p;
    for (const TrackSteps &t : tracks)
        for (std::size_t i = 0; i < 16 && i < t.steps.size(); ++i)
            p.tracks[std::size_t(t.drum)][i] = t.steps[i] == 'X'   ? StepState::Accent
                                               : t.steps[i] == 'x' ? StepState::On
                                                                   : StepState::Off;
    p.length = std::uint8_t(length);
    return p;
}

} // namespace

Pattern factoryPattern(int index)
{
    switch (index) {
    default:
    case 0: { // Floor: four on the floor, offbeat open hats, clap on 2 and 4
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X...x...X...x..."},
            {DrumId::Cp, "....X.......X..."},
            {DrumId::Ch, "x.x.x.x.x.x.x.x."},
            {DrumId::Oh, "..x...x...x...x."},
            {DrumId::Rs, "...x.....x......"},
        };
        return makePattern(t);
    }
    case 1: { // Broken: syncopated kick, snare with ghost notes, busy hats
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X.....x...X..x.."},
            {DrumId::Sd, "....X..x....X..x"},
            {DrumId::Ch, "xxXxxxXxxxXxxxXx"},
            {DrumId::Oh, "..............x."},
            {DrumId::Lt, "...........x...."},
        };
        return makePattern(t);
    }
    case 2: { // Groove: made for Shuffle, ride and toms
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X..x..x...X....."},
            {DrumId::Sd, "....X.......X..."},
            {DrumId::Rd, "X.x.x.x.X.x.x.x."},
            {DrumId::Mt, "..............x."},
            {DrumId::Ht, ".............x.."},
            {DrumId::Rs, "......x......x.."},
        };
        return makePattern(t);
    }
    case 3: { // Fill: a one-bar tom and snare fill into a crash
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X.......x.x....."},
            {DrumId::Sd, "....X.x.....xxxX"},
            {DrumId::Ht, "........X.x....."},
            {DrumId::Mt, "..........X.x..."},
            {DrumId::Lt, "............x.x."},
            {DrumId::Cr, "X..............."},
            {DrumId::Ch, "x.x.x.x........."},
        };
        return makePattern(t);
    }
    }
}

Sequencer::Sequencer()
{
    m_clock.setLength(m_pattern.length);
    m_clock.setStepsPerBeat(m_pattern.stepsPerBeat);
}

void Sequencer::setPattern(const Pattern &pattern) noexcept
{
    m_pattern = pattern;
    setLength(pattern.length);
    setStepsPerBeat(pattern.stepsPerBeat);
}

void Sequencer::setStep(int track, int step, StepState state) noexcept
{
    if (track >= 0 && track < DrumCount && step >= 0 && step < 16)
        m_pattern.tracks[std::size_t(track)][std::size_t(step)] = state;
}

void Sequencer::setLength(int length) noexcept
{
    m_pattern.length = std::uint8_t(std::clamp(length, 1, 16));
    m_clock.setLength(m_pattern.length);
}

void Sequencer::setStepsPerBeat(int steps) noexcept
{
    if (steps != 3 && steps != 4 && steps != 6 && steps != 8)
        steps = 4;
    m_pattern.stepsPerBeat = std::uint8_t(steps);
    m_clock.setStepsPerBeat(steps);
}

void Sequencer::setShuffle(int amount) noexcept
{
    m_shuffle = std::clamp(amount, 0, kMaxShuffle);
    // The clock's gate point marks where a shuffled step fires.
    m_clock.setGateLength(kMaxShuffleDelay * m_shuffle / kMaxShuffle);
}

void Sequencer::start() noexcept
{
    m_clock.start();
    m_pendingStep = -1;
}

void Sequencer::stop() noexcept
{
    m_clock.stop();
    m_pendingStep = -1;
}

Hits Sequencer::hitsFor(int step) const noexcept
{
    Hits h;
    if (step < 0 || step >= 16)
        return h;
    for (int t = 0; t < DrumCount; ++t) {
        const StepState s = m_pattern.tracks[std::size_t(t)][std::size_t(step)];
        if (s != StepState::Off)
            h.on |= std::uint16_t(1u << t);
        if (s == StepState::Accent)
            h.accent |= std::uint16_t(1u << t);
    }
    // A closed hat on the same step wins: the open hat would only be choked.
    constexpr auto ch = std::uint16_t(1u << int(DrumId::Ch));
    constexpr auto oh = std::uint16_t(1u << int(DrumId::Oh));
    if (h.on & ch) {
        h.on &= std::uint16_t(~oh);
        h.accent &= std::uint16_t(~oh);
    }
    return h;
}

Hits Sequencer::tick(double beat) noexcept
{
    const auto t = m_clock.tick(beat);
    if (t.stepStarted) {
        // A shuffled step that never reached its delay (tempo jump) still plays.
        Hits late = m_pendingStep >= 0 ? hitsFor(m_pendingStep) : Hits{};
        m_pendingStep = -1;
        const bool offBeat = (m_clock.stepCount() & 1) != 0;
        if (offBeat && m_shuffle > 0) {
            m_pendingStep = m_clock.step();
            return late;
        }
        const Hits now = hitsFor(m_clock.step());
        return {std::uint16_t(late.on | now.on), std::uint16_t(late.accent | now.accent)};
    }
    if (t.gatePoint && m_pendingStep >= 0) {
        const Hits h = hitsFor(m_pendingStep);
        m_pendingStep = -1;
        return h;
    }
    return {};
}

} // namespace k909
