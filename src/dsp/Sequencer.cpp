#include "dsp/Sequencer.h"

#include <algorithm>

namespace k303 {

Pattern factoryPattern(int index)
{
    // {note 0..12, octave, accent, slide, gate}
    constexpr Step N{0, 0, false, false, Gate::Note};    // plain C2
    constexpr Step tie{0, 0, false, false, Gate::Tie};
    constexpr Step rest{0, 0, false, false, Gate::Rest};

    Pattern p;
    switch (index) {
    default:
    case 0: // Demo: busy root-note line with octave jumps and slides
        p.steps = {{
            {0, 0, true, false, Gate::Note},   // C2  accent
            N,                                 // C2
            {0, 1, false, true, Gate::Note},   // C3  slide ->
            N,                                 // C2
            {3, 0, true, false, Gate::Note},   // D#2 accent
            rest,
            N,                                 // C2
            {7, 0, false, true, Gate::Note},   // G2  slide ->
            {10, 0, false, false, Gate::Note}, // A#2
            {0, 0, true, false, Gate::Note},   // C2  accent
            {0, 1, false, false, Gate::Note},  // C3
            tie,
            {5, 0, false, true, Gate::Note},   // F2  slide ->
            {7, 0, true, false, Gate::Note},   // G2  accent
            N,                                 // C2
            {3, 1, true, true, Gate::Note},    // D#3 accent, slide -> (wraps to step 1)
        }};
        p.length = 16;
        break;
    case 1: // Rolling: steady 16ths climbing through the minor scale
        p.steps = {{
            {0, 0, true, false, Gate::Note},   // C2  accent
            {0, 1, false, false, Gate::Note},  // C3
            N,                                 // C2
            {3, 0, false, true, Gate::Note},   // D#2 slide ->
            {5, 0, false, false, Gate::Note},  // F2
            {0, 0, true, false, Gate::Note},   // C2  accent
            rest,
            {7, 0, false, true, Gate::Note},   // G2  slide ->
            {8, 0, false, false, Gate::Note},  // G#2
            N,                                 // C2
            {0, 1, true, true, Gate::Note},    // C3  accent, slide ->
            {10, 0, false, false, Gate::Note}, // A#2
            N,                                 // C2
            tie,
            {3, 1, true, false, Gate::Note},   // D#3 accent
            {0, 0, false, true, Gate::Note},   // C2  slide -> (wraps)
        }};
        p.length = 16;
        break;
    case 2: // Twelve: a 12-step loop that drifts against the 4/4 beat
        p.steps = {{
            {0, 0, true, false, Gate::Note},   // C2  accent
            rest,
            N,                                 // C2
            {0, 1, false, true, Gate::Note},   // C3  slide ->
            {10, 0, false, false, Gate::Note}, // A#2
            {7, 0, true, false, Gate::Note},   // G2  accent
            rest,
            {5, 0, false, true, Gate::Note},   // F2  slide ->
            {7, 0, false, false, Gate::Note},  // G2
            {0, 0, true, false, Gate::Note},   // C2  accent
            tie,
            {3, 0, false, true, Gate::Note},   // D#2 slide -> (wraps)
            rest, rest, rest, rest,
        }};
        p.length = 12;
        break;
    case 3: // High: squelchy upper-octave line, slide-heavy
        p.steps = {{
            {0, 1, true, true, Gate::Note},    // C3  accent, slide ->
            {7, 0, false, false, Gate::Note},  // G2
            {10, 0, false, true, Gate::Note},  // A#2 slide ->
            {0, 1, false, false, Gate::Note},  // C3
            {3, 1, true, false, Gate::Note},   // D#3 accent
            {0, 1, false, true, Gate::Note},   // C3  slide ->
            {10, 0, false, false, Gate::Note}, // A#2
            {7, 0, false, false, Gate::Note},  // G2
            {5, 0, true, true, Gate::Note},    // F2  accent, slide ->
            {7, 0, false, false, Gate::Note},  // G2
            tie,
            N,                                 // C2
            {0, 1, true, false, Gate::Note},   // C3  accent
            rest,
            {7, 0, false, true, Gate::Note},   // G2  slide ->
            {10, 0, false, false, Gate::Note}, // A#2
        }};
        p.length = 16;
        break;
    }
    return p;
}

void Sequencer::setSampleRate(double sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    updateSamplesPerStep();
}

void Sequencer::setTempo(double bpm) noexcept
{
    if (bpm == m_bpm)
        return;
    m_bpm = bpm;
    updateSamplesPerStep();
}

void Sequencer::updateSamplesPerStep() noexcept
{
    const double sps = m_sampleRate * 60.0 / (m_bpm * 4.0);
    // Keep the same fraction of the current step, so a tempo change never skips or repeats one.
    if (m_samplesPerStep > 0.0)
        m_phase *= sps / m_samplesPerStep;
    m_samplesPerStep = sps;
}

void Sequencer::setPattern(const Pattern &pattern) noexcept
{
    m_pattern = pattern;
    m_pattern.length = std::clamp<std::uint8_t>(pattern.length, 1, 16);
}

void Sequencer::setStep(int index, const Step &step) noexcept
{
    if (index >= 0 && index < int(m_pattern.steps.size()))
        m_pattern.steps[std::size_t(index)] = step;
}

void Sequencer::setLength(int length) noexcept
{
    m_pattern.length = std::uint8_t(std::clamp(length, 1, 16));
}

void Sequencer::start() noexcept
{
    m_running = true;
    m_step = -1;
    m_prevStep = -1;
    m_phase = 0.0;
    m_sounding = false;
    m_gateChecked = false;
}

SequencerEvent Sequencer::stop() noexcept
{
    m_running = false;
    m_step = -1;
    SequencerEvent ev;
    if (m_sounding) {
        m_sounding = false;
        ev.kind = SequencerEvent::Kind::GateOff;
    }
    return ev;
}

SequencerEvent Sequencer::tick() noexcept
{
    SequencerEvent ev;
    if (!m_running)
        return ev;

    if (m_step < 0 || m_phase >= m_samplesPerStep) {
        if (m_step >= 0)
            m_phase -= m_samplesPerStep; // carry the fraction: no drift
        m_prevStep = m_step;
        m_step = m_step < 0 ? 0 : nextStepIndex();
        m_gateChecked = false;
        ev = startStep();
    } else if (!m_gateChecked && m_phase >= kGateLength * m_samplesPerStep) {
        m_gateChecked = true;
        if (m_sounding && !continuesInto(nextStepIndex())) {
            m_sounding = false;
            ev.kind = SequencerEvent::Kind::GateOff;
        }
    }

    m_phase += 1.0;
    return ev;
}

SequencerEvent Sequencer::startStep() noexcept
{
    SequencerEvent ev;
    const Step &step = m_pattern.steps[m_step];

    switch (step.gate) {
    case Gate::Rest:
        // Only reached with the gate high if the pattern changed under us.
        if (m_sounding) {
            m_sounding = false;
            ev.kind = SequencerEvent::Kind::GateOff;
        }
        break;
    case Gate::Tie:
        // Extends a sounding note; after a rest it is a rest.
        break;
    case Gate::Note: {
        const bool slid = m_sounding && m_prevStep >= 0 && m_pattern.steps[m_prevStep].slide;
        ev.kind = slid ? SequencerEvent::Kind::SlideTo : SequencerEvent::Kind::NoteOn;
        ev.note = stepMidiNote(step);
        ev.accent = step.accent;
        m_sounding = true;
        break;
    }
    }
    return ev;
}

bool Sequencer::continuesInto(int nextStep) const noexcept
{
    const Step &next = m_pattern.steps[nextStep];
    if (next.gate == Gate::Tie)
        return true;
    return m_pattern.steps[m_step].slide && next.gate == Gate::Note;
}

} // namespace k303
