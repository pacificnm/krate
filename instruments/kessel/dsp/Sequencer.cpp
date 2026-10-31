#include "kessel/dsp/Sequencer.h"

#include <algorithm>
#include <string_view>

namespace kessel {

namespace {

// One string of 16 steps per track: '.' off, 'x' on, 'X' accent, 'f' flam.
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
                                               : t.steps[i] == 'f' ? StepState::Flam
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
    // ---- Industrial (for the Industrial, EBM, Gated 80s, Crushed and Machine Shop kits)
    case 4: { // Stomp: four on the floor, a booming downbeat, metal and clank on the offbeats
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X...X...X...X..."},
            {DrumId::Sd, "....X.......X..."},
            {DrumId::Ch, "x.xxx.xxx.xxx.xx"},
            {DrumId::Me, "..x.......x....."},
            {DrumId::Ck, "......x.......x."},
            {DrumId::Bm, "X..............."},
        };
        return makePattern(t);
    }
    case 5: { // Drive: the kick on every eighth, snare and clap together, sixteenth hats
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X.x.X.x.X.x.X.x."},
            {DrumId::Sd, "....X.......X..."},
            {DrumId::Cp, "....x.......x..."},
            {DrumId::Ch, "XxxxXxxxXxxxXxxx"},
            {DrumId::Nz, "...............x"},
        };
        return makePattern(t);
    }
    case 6: { // Anvil: a broken kick under struck metal
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X..x..X...x..x.."},
            {DrumId::Sd, "....X.......X..."},
            {DrumId::Me, "x.......x...x..."},
            {DrumId::Ck, "..x...x...x...x."},
            {DrumId::Oh, "..x...x...x...x."},
        };
        return makePattern(t);
    }
    case 7: { // Machine Gun: a snare roll into the bar, noise stabs, rim ticks
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X...X...X...X..."},
            {DrumId::Sd, "....X..x....XxxX"},
            {DrumId::Ch, "x.x.x.x.x.x.x.x."},
            {DrumId::Nz, "..X.......X....."},
            {DrumId::Rs, "...x..x....x..x."},
        };
        return makePattern(t);
    }
    case 8: { // Half-Time: heavy and slow, the boom under the kick
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X.........x....."},
            {DrumId::Sd, "........X......."},
            {DrumId::Bm, "X..............."},
            {DrumId::Me, "....x.......x..."},
            {DrumId::Ch, "x.x.x.x.x.x.x.x."},
            {DrumId::Nz, "............x..x"},
        };
        return makePattern(t);
    }
    case 9: { // Clatter: clank running through, rim and metal accents
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X..x...xX..x...."},
            {DrumId::Sd, "....X.......X..."},
            {DrumId::Ck, "x.xx.x.xx.x.xx.x"},
            {DrumId::Rs, "..x...x...x...x."},
            {DrumId::Me, "..............X."},
        };
        return makePattern(t);
    }
    case 10: { // Gated Floor: for the gated reverb, with a tom fill
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "X...X...X...X..."},
            {DrumId::Sd, "....X.......X..."},
            {DrumId::Mt, ".............x.."},
            {DrumId::Lt, "..............x."},
            {DrumId::Ch, "x.x.x.x.x.x.x.x."},
            {DrumId::Oh, "..x...x...x...x."},
        };
        return makePattern(t);
    }
    case 11: { // Breakdown: the boom, struck metal on the beat, a crash and a noise wash
        constexpr TrackSteps t[] = {
            {DrumId::Bd, "x..............."},
            {DrumId::Bm, "X.......x......."},
            {DrumId::Me, "x...x...x...x..."},
            {DrumId::Nz, "........X......."},
            {DrumId::Cr, "X..............."},
            {DrumId::Ck, "....x.......x..x"},
        };
        return makePattern(t);
    }
    }
}

Sequencer::Sequencer()
{
    applyTimings();
}

void Sequencer::applyTimings() noexcept
{
    m_pattern.length = std::uint8_t(std::clamp<int>(m_pattern.length, 1, 16));
    const int steps = m_pattern.stepsPerBeat;
    if (steps != 3 && steps != 4 && steps != 6 && steps != 8)
        m_pattern.stepsPerBeat = 4;
    m_pattern.order = std::uint8_t(std::min<int>(m_pattern.order, 3));
    m_clock.setLength(m_pattern.length);
    m_clock.setStepsPerBeat(m_pattern.stepsPerBeat);
    m_clock.setOrder(krate::StepClock::Order(m_pattern.order));
}

void Sequencer::setPattern(const Pattern &pattern, int slot) noexcept
{
    m_pattern = pattern;
    m_slot = slot;
    m_queuedSlot = -1;
    applyTimings();
}

void Sequencer::queuePattern(const Pattern &pattern, int slot) noexcept
{
    if (!running()) {
        setPattern(pattern, slot);
        return;
    }
    if (slot == m_slot) { // back to the playing one: just cancel the change
        m_queuedSlot = -1;
        return;
    }
    m_queued = pattern;
    m_queuedSlot = slot;
}

void Sequencer::replacePattern(const Pattern &pattern, int slot) noexcept
{
    if (Pattern *p = target(slot)) {
        *p = pattern;
        if (p == &m_pattern)
            applyTimings();
    }
}

Pattern *Sequencer::target(int slot) noexcept
{
    if (slot == m_slot)
        return &m_pattern;
    if (slot == m_queuedSlot)
        return &m_queued;
    return nullptr;
}

void Sequencer::setStep(int slot, int track, int step, StepState state) noexcept
{
    Pattern *p = target(slot);
    if (p && track >= 0 && track < DrumCount && step >= 0 && step < 16)
        p->tracks[std::size_t(track)][std::size_t(step)] = state;
}

void Sequencer::setStepDetail(int slot, int track, int step, std::uint8_t velocity, std::uint8_t tune,
                              std::uint8_t decay) noexcept
{
    Pattern *p = target(slot);
    if (!p || track < 0 || track >= DrumCount || step < 0 || step >= 16)
        return;
    const auto t = std::size_t(track), s = std::size_t(step);
    p->velocity[t][s] = std::min<std::uint8_t>(velocity, 127);
    p->tune[t][s] = tune;
    p->decay[t][s] = decay;
}

void Sequencer::setLength(int slot, int length) noexcept
{
    if (Pattern *p = target(slot)) {
        p->length = std::uint8_t(std::clamp(length, 1, 16));
        if (p == &m_pattern)
            applyTimings();
    }
}

void Sequencer::setStepsPerBeat(int slot, int steps) noexcept
{
    if (steps != 3 && steps != 4 && steps != 6 && steps != 8)
        return;
    if (Pattern *p = target(slot)) {
        p->stepsPerBeat = std::uint8_t(steps);
        if (p == &m_pattern)
            applyTimings();
    }
}

void Sequencer::setOrder(int slot, int order) noexcept
{
    if (Pattern *p = target(slot)) {
        p->order = std::uint8_t(std::clamp(order, 0, 3));
        if (p == &m_pattern)
            applyTimings();
    }
}

void Sequencer::setShuffle(int amount) noexcept
{
    m_shuffle = std::clamp(amount, 0, kMaxShuffle);
    // The clock's gate point marks where a shuffled step fires.
    m_clock.setGateLength(kMaxShuffleDelay * m_shuffle / kMaxShuffle);
}

void Sequencer::startAt(double beat) noexcept
{
    start();
    m_clock.startAt(beat);
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
        const auto track = std::size_t(t), at = std::size_t(step);
        const StepState s = m_pattern.tracks[track][at];
        if (s == StepState::Off)
            continue;
        h.on |= std::uint16_t(1u << t);
        if (s == StepState::Accent)
            h.accent |= std::uint16_t(1u << t);
        if (s == StepState::Flam)
            h.flam |= std::uint16_t(1u << t);
        h.velocity[track] = m_pattern.velocity[track][at];
        h.tune[track] = m_pattern.tune[track][at];
        h.decay[track] = m_pattern.decay[track][at];
    }
    // A closed hat on the same step wins: the open hat would only be choked.
    constexpr auto ch = std::uint16_t(1u << int(DrumId::Ch));
    constexpr auto oh = std::uint16_t(1u << int(DrumId::Oh));
    if (h.on & ch) {
        h.on &= std::uint16_t(~oh);
        h.accent &= std::uint16_t(~oh);
        h.flam &= std::uint16_t(~oh);
    }
    return h;
}

Hits Sequencer::tick(double beat) noexcept
{
    const auto t = m_clock.tick(beat);
    if (t.stepStarted) {
        // A new pass: a queued pattern takes over from its first step.
        if (m_queuedSlot >= 0 && m_clock.stepCount() % m_pattern.length == 0) {
            m_pattern = m_queued;
            m_slot = m_queuedSlot;
            m_queuedSlot = -1;
            applyTimings();
            m_clock.restartPass();
        }
        // A shuffled step that never reached its delay (tempo jump) still plays.
        Hits late = m_pendingStep >= 0 ? hitsFor(m_pendingStep) : Hits{};
        m_pendingStep = -1;
        const bool offBeat = (m_clock.stepCount() & 1) != 0;
        if (offBeat && m_shuffle > 0) {
            m_pendingStep = m_clock.step();
            return late;
        }
        late.merge(hitsFor(m_clock.step()));
        return late;
    }
    if (t.gatePoint && m_pendingStep >= 0) {
        const Hits h = hitsFor(m_pendingStep);
        m_pendingStep = -1;
        return h;
    }
    return {};
}

} // namespace Kessel
