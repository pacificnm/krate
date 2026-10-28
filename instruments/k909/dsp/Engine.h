#pragma once

#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "core/SpscQueue.h"
#include "dsp/Params.h"
#include "dsp/Voices.h"

namespace k909 {

// GUI -> audio thread (docs/SEQUENCER.md#editing-gui--audio). D2: audition only;
// pattern edits join it in D3-D4.
struct Command
{
    enum class Kind : std::uint8_t { Trigger };
    Kind kind = Kind::Trigger;
    std::uint8_t drum = 0;     // DrumId
    bool accent = false;
    float velocity = 1.0f;
};

// K909's realtime core (docs/DSP.md, docs/SEQUENCER.md): eleven voices played
// by MIDI notes (General MIDI drum numbers, docs/BRIEF.md#midi-notes), plus
// MIDI Learn for the knobs. The sequencer arrives in D3.
class Engine final : public krate::IInstrument
{
public:
    Engine() = default;

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float normalized) noexcept override { m_params.set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { m_params.setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }
    void process(const krate::AudioBlock &block) noexcept override;

    float param(ParamId id) const noexcept { return m_params.get(int(id)); }

    // Audio thread (or offline, between blocks): play a voice now, with the
    // current knob settings. velocity 0..1; accent adds Total Accent.
    void trigger(DrumId drum, float velocity, bool accent) noexcept;
    bool sounding(DrumId drum) const noexcept { return m_voices.sounding(drum); }

    // GUI thread: queue a command for the next block. False if the queue is full.
    bool post(const Command &command) noexcept { return m_commands.push(command); }

    // The voice a MIDI note plays, or DrumId::Count for none.
    static DrumId drumForNote(int note) noexcept;

private:
    void handleMidi(const krate::MidiEvent &event) noexcept;
    void apply(const Command &command) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values
    Voices m_voices;
    krate::SpscQueue<Command, 256> m_commands;
    double m_sampleRate = 48000.0;
    double m_volume = -1.0;          // smoothed output gain; < 0 = not yet primed
    double m_volumeSmoothing = 0.0;  // per-sample one-pole coefficient
};

} // namespace k909
