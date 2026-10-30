#include <QtTest>

#include <numbers>

#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/Instrument.h"
#include "rack/Rack.h"

using namespace krate;
using Kind = Rack::Mapping::Kind;

namespace {

std::string fmt(float v) { return std::to_string(v); }

// Two different parameter tables, so follow-focus has to resolve by key.
constexpr ParamDescriptor kParamsA[] = {
    {"cutoff", "Cutoff", 0.5f, 0, &fmt},
    {"level", "Level", 0.5f, 0, &fmt},
    {"mode", "Mode", 0.0f, 3, &fmt},
};
constexpr ParamDescriptor kParamsB[] = {
    {"level", "Level", 0.5f, 0, &fmt},
    {"tone", "Tone", 0.5f, 0, &fmt},
};

// Records what the rack gives it and outputs a constant (DC) level.
class FakeDevice final : public IInstrument
{
public:
    FakeDevice(std::span<const ParamDescriptor> p, float level)
        : params(p)
        , values(p.size(), 0.0f)
        , dc(level)
    {
    }

    void prepare(double sr, std::uint32_t) noexcept override { sampleRate = sr; }
    std::span<const ParamDescriptor> parameters() const noexcept override { return params; }
    float param(int i) const noexcept override { return values[std::size_t(i)]; }
    void setParam(int i, float v) noexcept override { values[std::size_t(i)] = v; }
    void setParamFromHost(int i, float v) noexcept override
    {
        values[std::size_t(i)] = v;
        changed |= std::uint64_t(1) << i;
    }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return std::exchange(changed, 0); }
    int inputChannels() const noexcept override { return inputs; }

    void process(const AudioBlock &block) noexcept override
    {
        ++calls;
        gotInputs.push_back(block.inputs != nullptr);
        if (block.inputs) // each input's first and last sample this block
            for (int c = 0; c < inputs; ++c)
                inputSamples.push_back({block.inputs[c][0], block.inputs[c][block.frames - 1]});
        for (std::uint32_t i = 0; i < block.midiCount; ++i)
            received.push_back(block.midi[i]);
        if (block.transport)
            transports.push_back(*block.transport);
        if (sineHz > 0.0) { // a sine of amplitude dc instead (silent from `quietAfter` on)
            for (std::uint32_t i = 0; i < block.frames; ++i) {
                block.outputs[0][i] = block.outputs[1][i] = played < quietAfter ? dc * float(std::sin(phase)) : 0.0f;
                phase += 2.0 * std::numbers::pi * sineHz / sampleRate;
                ++played;
            }
            return;
        }
        std::fill_n(block.outputs[0], block.frames, dc);
        std::fill_n(block.outputs[1], block.frames, dc);
    }
    double sineHz = 0.0;
    double phase = 0.0;
    std::uint64_t played = 0, quietAfter = ~std::uint64_t(0);

    std::span<const ParamDescriptor> params;
    std::vector<float> values;
    float dc;
    double sampleRate = 0.0;
    std::uint64_t changed = 0;
    int calls = 0;
    std::vector<MidiEvent> received;
    std::vector<TransportState> transports;
    int inputs = 0;
    std::vector<bool> gotInputs;
    std::vector<std::pair<float, float>> inputSamples;
};

MidiEvent msg(std::uint8_t status, std::uint8_t d1, std::uint8_t d2 = 0, std::uint32_t frame = 0)
{
    return MidiEvent{frame, std::uint8_t((status & 0xF0) == 0xC0 ? 2 : 3), {status, d1, d2}};
}
MidiEvent noteOn(int ch, int note, std::uint32_t frame = 0) { return msg(std::uint8_t(0x90 | ch), std::uint8_t(note), 100, frame); }
MidiEvent noteOff(int ch, int note) { return msg(std::uint8_t(0x80 | ch), std::uint8_t(note), 0); }
MidiEvent cc(int ch, int number, int value) { return msg(std::uint8_t(0xB0 | ch), std::uint8_t(number), std::uint8_t(value)); }
MidiEvent program(int ch, int number) { return msg(std::uint8_t(0xC0 | ch), std::uint8_t(number)); }

struct Output
{
    std::vector<float> left, right;
    std::vector<std::vector<float>> devices; // L0, R0, L1, R1, ...
};

Output run(Rack &rack, std::uint32_t frames, std::vector<MidiEvent> midi = {},
           const TransportState *transport = nullptr, const float *const *inputs = nullptr)
{
    Output out;
    out.left.assign(frames, 0.0f);
    out.right.assign(frames, 0.0f);
    out.devices.assign(std::size_t(2 * rack.deviceCount()), std::vector<float>(frames, 0.0f));
    std::vector<float *> devicePtrs;
    for (auto &d : out.devices)
        devicePtrs.push_back(d.data());
    RackBlock block;
    block.frames = frames;
    block.midi = midi.data();
    block.midiCount = std::uint32_t(midi.size());
    block.transport = transport;
    block.inputs = inputs;
    block.masterL = out.left.data();
    block.masterR = out.right.data();
    block.deviceOutputs = devicePtrs.data();
    rack.process(block);
    return out;
}

} // namespace

class TestRack : public QObject
{
    Q_OBJECT

private slots:
    // --- MIDI routing ------------------------------------------------------------

    void notesGoToTheFocusedDevice()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        QCOMPARE(rack.focus(), 0);
        run(rack, 64, {noteOn(0, 60)});
        QCOMPARE(a.received.size(), std::size_t(1));
        QVERIFY(b.received.empty());
        rack.setFocus(1);
        run(rack, 64, {noteOn(0, 62)});
        QCOMPARE(b.received.size(), std::size_t(1));
        QCOMPARE(int(b.received[0].data[1]), 62);
    }

    // A channel assigned to a device always reaches it, whatever has focus.
    void assignedChannelOverridesFocus()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b, /*midiChannel*/ 9); // pads on channel 10
        run(rack, 64, {noteOn(9, 36), noteOn(0, 60)});
        QCOMPARE(b.received.size(), std::size_t(1));
        QCOMPARE(int(b.received[0].data[1]), 36);
        QCOMPARE(a.received.size(), std::size_t(1));
        QCOMPARE(int(a.received[0].data[1]), 60);
    }

    // The keyboard follows focus: on its channel (or any), focus wins over a
    // device holding the channel; other channels keep their routing; off, as before.
    void keysFollowFocus()
    {
        for (int keys : {Rack::kAnyChannel, 1, -1}) {
            FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f), c(kParamsB, 0.0f);
            Rack rack;
            rack.addDevice(a);
            rack.addDevice(b, /*midiChannel*/ 1); // holds channel 2: the keyboard's
            rack.addDevice(c, /*midiChannel*/ 9); // pads on channel 10
            rack.setKeysChannel(keys);
            QCOMPARE(rack.keysChannel(), keys);
            rack.setFocus(0);
            run(rack, 64, {noteOn(1, 60), noteOn(9, 36)});
            const bool keysToFocus = keys != -1;
            QCOMPARE(a.received.size(), std::size_t(keysToFocus ? (keys == Rack::kAnyChannel ? 2 : 1) : 0));
            QCOMPARE(b.received.size(), std::size_t(keysToFocus ? 0 : 1));
            QCOMPARE(c.received.size(), std::size_t(keys == Rack::kAnyChannel ? 0 : 1)); // pads keep channel 10 unless any
        }
    }

    // Switching focus with a key down: its note-off still reaches the device that got the note-on.
    void noteEndsWhereItStarted()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.setKeysChannel(Rack::kAnyChannel);
        run(rack, 64, {noteOn(0, 60)});
        rack.setFocus(1);
        run(rack, 64, {noteOff(0, 60), noteOn(0, 64)});
        QCOMPARE(a.received.size(), std::size_t(2)); // on and off
        QCOMPARE(int(a.received[1].data[0] & 0xF0), 0x80);
        QCOMPARE(b.received.size(), std::size_t(1)); // only the new note
        QCOMPARE(int(b.received[0].data[1]), 64);
        run(rack, 64, {msg(0x90, 64, 0)}); // a velocity-0 note-on ends it too
        QCOMPARE(b.received.size(), std::size_t(2));
    }

    void eventsKeepTheirFrame()
    {
        FakeDevice a(kParamsA, 0.0f);
        Rack rack;
        rack.addDevice(a);
        run(rack, 64, {noteOn(0, 60, 37)});
        QCOMPARE(a.received.at(0).frame, 37u);
    }

    void programChangeSwitchesFocus()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        run(rack, 64, {program(0, 1)});
        QCOMPARE(rack.focus(), 1);
        QVERIFY(a.received.empty() && b.received.empty()); // consumed
        run(rack, 64, {program(0, 5)});                    // no device 5: passed on
        QCOMPARE(rack.focus(), 1);
        QCOMPARE(b.received.size(), std::size_t(1));
        rack.setProgramChangeFocus(false);
        run(rack, 64, {program(0, 0)});
        QCOMPARE(rack.focus(), 1);
        QCOMPARE(b.received.size(), std::size_t(2));
    }

    // --- Focus actions -----------------------------------------------------------

    void focusActionLearnedFromAButton()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f), c(kParamsA, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.addDevice(c);
        rack.arm({Kind::FocusAction, -1, Rack::kFocusNext});
        run(rack, 64, {cc(0, 102, 127)}); // the teaching press
        QCOMPARE(rack.focus(), 0);        // ...doesn't fire
        const auto learned = rack.popLearned();
        QVERIFY(learned.has_value());
        QVERIFY(!learned->isNote);
        QCOMPARE(int(learned->number), 102);
        QCOMPARE(int(rack.armed().kind), int(Kind::None));

        run(rack, 64, {cc(0, 102, 127)});
        QCOMPARE(rack.focus(), 1);
        run(rack, 64, {cc(0, 102, 0)}); // release: nothing
        QCOMPARE(rack.focus(), 1);
        run(rack, 64, {cc(0, 102, 127), cc(0, 102, 0), cc(0, 102, 127)});
        QCOMPARE(rack.focus(), 0); // wraps: 1 -> 2 -> 0
        QVERIFY(a.received.empty() && b.received.empty() && c.received.empty());
    }

    void focusActionLearnedFromANote()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.arm({Kind::FocusAction, -1, 1}); // "focus device 1"
        run(rack, 64, {noteOn(9, 40), noteOff(9, 40)});
        QVERIFY(rack.popLearned()->isNote);
        QCOMPARE(rack.focus(), 0);
        run(rack, 64, {noteOn(9, 40), noteOff(9, 40)});
        QCOMPARE(rack.focus(), 1);
        QVERIFY(a.received.empty() && b.received.empty()); // the button never plays
        run(rack, 64, {noteOn(9, 41)});                    // other notes still play
        QCOMPARE(b.received.size(), std::size_t(1));
    }

    // Actions (play/stop, an instrument's buttons) are handed to the GUI on a press.
    void actionsFireOnPressFromCcOrNote()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.arm({Kind::Action, Rack::kRackDevice, 0}); // e.g. play/stop
        run(rack, 64, {cc(15, 47, 127)});              // the teaching press
        QVERIFY(!rack.popFired().has_value());
        QCOMPARE(int(rack.popLearned()->mapping.kind), int(Kind::Action));

        run(rack, 64, {cc(15, 47, 127), cc(15, 47, 0)});
        auto fired = rack.popFired();
        QVERIFY(fired.has_value());
        QCOMPARE(fired->device, Rack::kRackDevice);
        QCOMPARE(fired->action, 0);
        QVERIFY(!rack.popFired().has_value()); // the release does nothing

        rack.arm({Kind::Action, 1, 2}); // device 1's action 2, learned from a pad
        run(rack, 64, {noteOn(9, 38), noteOff(9, 38)});
        QVERIFY(!rack.popFired().has_value());
        run(rack, 64, {noteOn(9, 38), noteOff(9, 38)});
        fired = rack.popFired();
        QCOMPARE(fired->device, 1);
        QCOMPARE(fired->action, 2);
        QVERIFY(a.received.empty() && b.received.empty()); // the pad never plays
    }

    void noteMapsOnlyHoldButtons()
    {
        Rack rack;
        rack.setNoteMapping(0, 60, {Kind::Param, 0, 0}); // not a button: refused
        QCOMPARE(int(rack.noteMapping(0, 60).kind), int(Kind::None));
        rack.setNoteMapping(0, 61, {Kind::Mixer, 0, Rack::MixerFader}); // a fader isn't either
        QCOMPARE(int(rack.noteMapping(0, 61).kind), int(Kind::None));
        rack.setNoteMapping(0, 62, {Kind::Mixer, 0, Rack::MixerMute});
        QCOMPARE(int(rack.noteMapping(0, 62).kind), int(Kind::Mixer));
    }

    // Mixer controls go to the GUI as moves: faders and pans every value (the
    // learning move too), mute and solo on presses only.
    void mixerControlsBecomeMoves()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.arm({Kind::Mixer, 1, Rack::MixerFader});
        run(rack, 64, {cc(0, 7, 90)});
        auto move = rack.popMixerMove();
        QVERIFY(move.has_value()); // a fader applies the value that taught it
        QCOMPARE(move->device, 1);
        QCOMPARE(move->control, int(Rack::MixerFader));
        QCOMPARE(move->value, std::uint8_t(90));
        run(rack, 64, {cc(0, 7, 10), cc(0, 7, 11)});
        QCOMPARE(rack.popMixerMove()->value, std::uint8_t(10));
        QCOMPARE(rack.popMixerMove()->value, std::uint8_t(11));

        rack.arm({Kind::Mixer, Rack::kRackDevice, Rack::MixerFader}); // the master
        run(rack, 64, {cc(0, 8, 100)});
        QCOMPARE(rack.popMixerMove()->device, Rack::kRackDevice);

        rack.arm({Kind::Mixer, 0, Rack::MixerMute}); // a button, from a CC
        run(rack, 64, {cc(15, 60, 127)});
        QVERIFY(!rack.popMixerMove().has_value()); // the teaching press
        run(rack, 64, {cc(15, 60, 127), cc(15, 60, 0)});
        QCOMPARE(rack.popMixerMove()->control, int(Rack::MixerMute));
        QVERIFY(!rack.popMixerMove().has_value()); // the release does nothing

        rack.arm({Kind::Mixer, 1, Rack::MixerSolo}); // from a pad
        run(rack, 64, {noteOn(9, 40), noteOff(9, 40)});
        run(rack, 64, {noteOn(9, 40), noteOff(9, 40)});
        move = rack.popMixerMove();
        QCOMPARE(move->device, 1);
        QCOMPARE(move->control, int(Rack::MixerSolo));
        QVERIFY(!rack.popMixerMove().has_value());
        QVERIFY(a.received.empty() && b.received.empty()); // nothing reached the instruments
    }

    // --- Rack CC map ---------------------------------------------------------------

    void fixedMappingIgnoresFocus()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.setCcMapping(Rack::kAnyChannel, 20, {Kind::Param, 1, 1}); // b's "tone"
        run(rack, 64, {cc(5, 20, 127)});
        QCOMPARE(b.values[1], 1.0f);
        QCOMPARE(b.takeParamsChangedByAudio(), std::uint64_t(1) << 1); // b's panel will follow
        QCOMPARE(a.values[1], 0.0f);
        QVERIFY(a.received.empty() && b.received.empty()); // consumed
    }

    // A follow-focus knob drives the same parameter key on whichever device has focus.
    void followFocusResolvesByKey()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.setCcMapping(Rack::kAnyChannel, 21, {Kind::FollowFocus, 0, 1}); // learned on a's "level"
        run(rack, 64, {cc(0, 21, 127)});
        QCOMPARE(a.values[1], 1.0f);
        rack.setFocus(1);
        run(rack, 64, {cc(0, 21, 64)});
        QCOMPARE(b.values[0], 64.0f / 127.0f); // "level" is index 0 on b
        QCOMPARE(a.values[1], 1.0f);

        rack.setCcMapping(Rack::kAnyChannel, 22, {Kind::FollowFocus, 0, 0}); // a's "cutoff"
        run(rack, 64, {cc(0, 22, 127)});      // b has no "cutoff": nothing happens
        QCOMPARE(b.values[0], 64.0f / 127.0f);
        QCOMPARE(b.values[1], 0.0f);
    }

    // Learning records the channel: the same CC on another channel stays free.
    void learnRecordsTheChannel()
    {
        FakeDevice a(kParamsA, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.arm({Kind::Param, 0, 0});
        run(rack, 64, {cc(3, 30, 127)});
        QCOMPARE(a.values[0], 1.0f); // applied at once
        QCOMPARE(int(rack.ccMapping(3, 30).kind), int(Kind::Param));
        QCOMPARE(int(rack.ccMapping(4, 30).kind), int(Kind::None));
        QCOMPARE(int(rack.popLearned()->channel), 3);

        run(rack, 64, {cc(4, 30, 10)}); // unmapped: forwarded for the device to use
        QCOMPARE(a.received.size(), std::size_t(1));
        QCOMPARE(a.values[0], 1.0f);
    }

    void channelSpecificBeatsAny()
    {
        FakeDevice a(kParamsA, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.setCcMapping(Rack::kAnyChannel, 40, {Kind::Param, 0, 0});
        rack.setCcMapping(2, 40, {Kind::Param, 0, 1});
        run(rack, 64, {cc(2, 40, 127), cc(0, 40, 127)});
        QCOMPARE(a.values[1], 1.0f); // channel 3 row
        QCOMPARE(a.values[0], 1.0f); // any-channel row
    }

    void selectorsSnapToPositions()
    {
        FakeDevice a(kParamsA, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.setCcMapping(Rack::kAnyChannel, 50, {Kind::Param, 0, 2}); // "mode", 3 positions
        run(rack, 64, {cc(0, 50, 80)});
        QCOMPARE(a.values[2], 0.5f);
    }

    void clearMappings()
    {
        FakeDevice a(kParamsA, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.setCcMapping(0, 1, {Kind::Param, 0, 0});
        rack.setNoteMapping(0, 36, {Kind::FocusAction, -1, 0});
        QCOMPARE(int(rack.noteMapping(0, 36).kind), int(Kind::FocusAction));
        rack.clearMappings();
        QCOMPARE(int(rack.ccMapping(0, 1).kind), int(Kind::None));
        QCOMPARE(int(rack.noteMapping(0, 36).kind), int(Kind::None));
    }

    // --- Mixer ---------------------------------------------------------------------

    void mixerSumsLevelsPanMuteSolo()
    {
        FakeDevice a(kParamsA, 0.1f), b(kParamsB, 0.2f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        const auto settled = [&rack] { run(rack, 64); return run(rack, 64); }; // gains ramp over a block

        auto out = settled();
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.3f)) < 1e-6f); // sum, then master soft clip

        rack.setLevel(1, 0.5f);
        out = settled();
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.2f)) < 1e-6f);

        rack.setPan(0, 1.0f); // a hard right: its left side drops out
        out = settled();
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.1f)) < 1e-6f);
        QVERIFY(std::abs(out.right.back() - krate::MasterStage::shape(0.2f)) < 1e-6f);

        rack.setPan(0, 0.0f);
        rack.setMute(1, true);
        out = settled();
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.1f)) < 1e-6f);

        rack.setMute(1, false);
        rack.setSolo(1, true); // solo wins: only b
        out = settled();
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.1f)) < 1e-6f); // b at level 0.5

        rack.setSolo(1, false);
        rack.setLevel(1, 1.0f);
        rack.setMasterLevel(0.5f);
        out = settled();
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.15f)) < 1e-6f);
    }

    void faderMovesAreRamped()
    {
        FakeDevice a(kParamsA, 0.5f);
        Rack rack;
        rack.addDevice(a);
        run(rack, 64);
        rack.setMute(0, true);
        const auto out = run(rack, 64);
        QVERIFY(out.left.front() > 0.4f);          // starts near where it was
        QVERIFY(std::abs(out.left.back()) < 1e-6f); // ends silent
        for (std::size_t i = 1; i < out.left.size(); ++i)
            QVERIFY(out.left[i] <= out.left[i - 1] + 1e-7f); // no jumps up
    }

    // The very first block starts at the fader settings: a muted device is
    // silent from sample 0 instead of fading out from unity.
    void firstBlockStartsAtTheFaderSettings()
    {
        FakeDevice a(kParamsA, 0.5f), b(kParamsB, 0.25f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.setMute(0, true);
        rack.setMasterLevel(0.5f);
        const auto out = run(rack, 64);
        QVERIFY(std::abs(out.left.front() - krate::MasterStage::shape(0.125f)) < 1e-6f);
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.125f)) < 1e-6f);
    }

    void deviceOutputsAreBeforeTheMixer()
    {
        FakeDevice a(kParamsA, 0.25f);
        Rack rack;
        rack.addDevice(a);
        rack.setLevel(0, 0.0f);
        const auto out = run(rack, 64);
        QCOMPARE(out.devices[0].back(), 0.25f);
        QCOMPARE(out.devices[1].back(), 0.25f);
        QVERIFY(std::abs(out.left.back()) < 1e-6f);
    }

    // The strip EQ: each band measured on a sine matches the filter's response
    // (within 0.2 dB); flat, the sound passes untouched; the device's own
    // output stays before it.
    void stripEq()
    {
        const auto measure = [](double hz, ChannelEq::Settings eq) {
            FakeDevice a(kParamsA, 0.1f); // well under the master stage's knee
            a.sineHz = hz;
            Rack rack;
            rack.addDevice(a);
            rack.prepare(48000.0);
            for (int b = 0; b < ChannelEq::Count; ++b)
                rack.setEq(0, b, eq[std::size_t(b)]);
            const auto out = run(rack, 48000);
            // Amplitude from the RMS (sample peaks of a high sine fall short of its true peak).
            double sum = 0.0, deviceSum = 0.0;
            for (std::size_t i = 24000; i < out.left.size(); ++i) {
                sum += double(out.left[i]) * out.left[i];
                deviceSum += double(out.devices[0][i]) * out.devices[0][i];
            }
            const double n = double(out.left.size() - 24000), amplitude = 0.1 / std::sqrt(2.0);
            return std::pair{20.0 * std::log10(std::sqrt(sum / n) / amplitude),
                             20.0 * std::log10(std::sqrt(deviceSum / n) / amplitude)};
        };
        auto eq = ChannelEq::kFlat;
        eq[ChannelEq::MidGain] = 9.0f;
        eq[ChannelEq::MidFreq] = 1000.0f;
        eq[ChannelEq::MidQ] = 1.0f;
        const auto [mid, before] = measure(1000.0, eq);
        QVERIFY2(std::fabs(mid - 9.0) < 0.2, qPrintable(QString::number(mid)));
        QVERIFY2(std::fabs(before) < 0.05, qPrintable(QString::number(before))); // the device's own output: no EQ
        const double away = measure(4000.0, eq).first;
        QVERIFY2(std::fabs(away - 20.0 * std::log10(ChannelEq::magnitude(eq, 48000.0, 4000.0))) < 0.2,
                 qPrintable(QString::number(away)));

        eq = ChannelEq::kFlat;
        eq[ChannelEq::LowGain] = -12.0f;
        eq[ChannelEq::LowFreq] = 200.0f;
        const double low = measure(40.0, eq).first;
        QVERIFY2(std::fabs(low - 20.0 * std::log10(ChannelEq::magnitude(eq, 48000.0, 40.0))) < 0.2,
                 qPrintable(QString::number(low)));
        QVERIFY(low < -10.0);

        eq = ChannelEq::kFlat;
        eq[ChannelEq::HighGain] = 6.0f;
        eq[ChannelEq::HighFreq] = 4000.0f;
        const double high = measure(14000.0, eq).first;
        QVERIFY2(std::fabs(high - 20.0 * std::log10(ChannelEq::magnitude(eq, 48000.0, 14000.0))) < 0.2,
                 qPrintable(QString::number(high)));
        QVERIFY(high > 5.0);

        // Flat: bit for bit what it was without an EQ.
        FakeDevice a(kParamsA, 0.25f);
        a.sineHz = 440.0;
        Rack rack;
        rack.addDevice(a);
        rack.prepare(48000.0);
        rack.setEq(0, ChannelEq::MidFreq, 2000.0f); // a frequency, no gain: still flat
        const auto out = run(rack, 4800);
        for (std::size_t i = 0; i < out.left.size(); ++i)
            QCOMPARE(out.left[i], MasterStage::shape(out.devices[0][i]));
    }

    // Effects: nothing sent, nothing changes (bit for bit, returns up or not);
    // the delay's first echo comes exactly one division later; the reverb rings
    // on after the sound stops; a return at 0, a muted strip or a closed fader
    // sends nothing.
    void effects()
    {
        TransportState t;
        t.playing = true;
        t.bpm = 120.0;
        struct Setup
        {
            float send = 0.0f;
            int effect = Rack::Reverb;
            float returnLevel = 1.0f;
            bool mute = false;
            float level = 1.0f;
        };
        const auto run2 = [&t](Setup su, std::uint64_t frames) {
            FakeDevice a(kParamsA, 0.2f);
            a.sineHz = 1000.0;
            a.quietAfter = 4800; // 0.1 s of sine, then silence
            Rack rack(256);
            rack.addDevice(a);
            rack.prepare(48000.0);
            rack.setLevel(0, su.level);
            rack.setMute(0, su.mute);
            rack.setSend(0, su.effect, su.send);
            rack.setReverb(su.effect == Rack::Reverb ? su.returnLevel : 0.0f, 0.5f);
            rack.setDelay(su.effect == Rack::Delay ? su.returnLevel : 0.0f, 4, 0.5f); // 1/4: 0.5 s at 120 BPM
            std::vector<float> left;
            for (std::uint64_t done = 0; done < frames; done += 256) {
                const auto out = run(rack, 256, {}, &t);
                left.insert(left.end(), out.left.begin(), out.left.end());
            }
            return left;
        };
        const auto rms = [](const std::vector<float> &x, std::size_t from, std::size_t to) {
            double sum = 0.0;
            for (std::size_t i = from; i < to; ++i)
                sum += double(x[i]) * x[i];
            return std::sqrt(sum / double(to - from));
        };

        // Nothing sent: the same samples as with the effects' returns down.
        const auto none = run2({0.0f, Rack::Reverb, 1.0f}, 24064);
        const auto off = run2({0.0f, Rack::Reverb, 0.0f}, 24064);
        QVERIFY(none == off);
        QVERIFY(rms(none, 4900, 24000) == 0.0);

        // Delay: silence after the sine until the echo, 0.5 s after it began.
        const auto delayed = run2({1.0f, Rack::Delay}, 48000);
        QVERIFY2(rms(delayed, 5000, 23900) < 1e-6, qPrintable(QString::number(rms(delayed, 5000, 23900))));
        QVERIFY2(rms(delayed, 24100, 28700) > 0.02, qPrintable(QString::number(rms(delayed, 24100, 28700))));

        // Reverb: a tail after the sound stops; none with its return at 0.
        const auto wet = run2({1.0f, Rack::Reverb}, 24064);
        QVERIFY2(rms(wet, 9600, 19200) > 1e-3, qPrintable(QString::number(rms(wet, 9600, 19200))));
        QVERIFY(rms(run2({1.0f, Rack::Reverb, 0.0f}, 24064), 9600, 19200) == 0.0);

        // Muted, or the fader down: nothing reaches the effects (sends are after the fader).
        QVERIFY(rms(run2({1.0f, Rack::Reverb, 1.0f, true}, 24064), 0, 24000) == 0.0);
        QVERIFY(rms(run2({1.0f, Rack::Reverb, 1.0f, false, 0.0f}, 24064), 0, 24000) == 0.0);
    }

    void meters()
    {
        FakeDevice a(kParamsA, 0.5f), b(kParamsB, 0.5f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.setLevel(1, 0.5f);
        run(rack, 64);
        run(rack, 64);
        rack.takeDevicePeak(0);
        rack.takeDevicePeak(1);
        rack.takeMasterPeak();
        run(rack, 64);
        QVERIFY(std::abs(rack.takeDevicePeak(0) - 0.5f) < 1e-6f);  // post-fader
        QVERIFY(std::abs(rack.takeDevicePeak(1) - 0.25f) < 1e-6f);
        QVERIFY(std::abs(rack.takeMasterPeak() - krate::MasterStage::shape(0.75f)) < 1e-6f);
        QCOMPARE(rack.takeDevicePeak(0), 0.0f); // reset after reading
    }

    // --- Transport -----------------------------------------------------------------

    void everyDeviceGetsTheSameTransport()
    {
        FakeDevice a(kParamsA, 0.0f), b(kParamsB, 0.0f);
        Rack rack;
        rack.addDevice(a);
        rack.addDevice(b);
        rack.prepare(48000.0);
        QCOMPARE(a.sampleRate, 48000.0);
        TransportState t;
        t.playing = true;
        t.restarted = true;
        t.bpm = 133.0;
        t.beatAtBlockStart = 7.25;
        run(rack, 64, {}, &t);
        QCOMPARE(a.transports.size(), std::size_t(1));
        QCOMPARE(b.transports.size(), std::size_t(1));
        QCOMPARE(a.transports[0].beatAtBlockStart, 7.25);
        QCOMPARE(b.transports[0].bpm, 133.0);
        QVERIFY(a.transports[0].restarted && b.transports[0].restarted);
    }

    // Blocks longer than the rack's buffers are split; MIDI and beat follow.
    void longBlocksAreSplit()
    {
        FakeDevice a(kParamsA, 0.1f);
        Rack rack(4096);
        rack.addDevice(a);
        rack.prepare(48000.0);
        TransportState t;
        t.playing = true;
        t.restarted = true;
        t.bpm = 120.0;
        const auto out = run(rack, 10000, {noteOn(0, 60, 5000)}, &t);
        QCOMPARE(a.calls, 3);
        QCOMPARE(a.received.size(), std::size_t(1));
        QCOMPARE(a.received[0].frame, 5000u - 4096u); // re-based into the second chunk
        QVERIFY(a.transports[0].restarted);
        QVERIFY(!a.transports[1].restarted);
        QVERIFY(std::abs(a.transports[1].beatAtBlockStart - 4096.0 / 24000.0) < 1e-12);
        QVERIFY(std::abs(a.transports[2].beatAtBlockStart - 8192.0 / 24000.0) < 1e-12);
        QVERIFY(std::abs(out.left.back() - krate::MasterStage::shape(0.1f)) < 1e-6f); // all of it was rendered
    }

    // --- Audio inputs --------------------------------------------------------------

    // Devices with inputs get the rack's (re-based per chunk); extra ones are
    // silent; devices without inputs get none.
    void devicesWithInputsGetTheRacksInputs()
    {
        FakeDevice amp(kParamsA, 0.0f), synth(kParamsB, 0.0f);
        amp.inputs = 3;
        Rack rack(4096);
        rack.addDevice(amp);
        rack.addDevice(synth);
        rack.prepare(48000.0);
        std::vector<float> in1(5000), in2(5000);
        for (std::size_t i = 0; i < in1.size(); ++i) {
            in1[i] = float(i);
            in2[i] = -float(i);
        }
        const float *const inputs[Rack::kInputs] = {in1.data(), in2.data()};
        run(rack, 5000, {}, nullptr, inputs);

        QCOMPARE(amp.gotInputs, (std::vector<bool>{true, true}));
        QCOMPARE(amp.inputSamples.size(), std::size_t(6)); // 2 chunks x 3 inputs
        QCOMPARE(amp.inputSamples[0], std::make_pair(0.0f, 4095.0f));
        QCOMPARE(amp.inputSamples[1], std::make_pair(-0.0f, -4095.0f));
        QCOMPARE(amp.inputSamples[2], std::make_pair(0.0f, 0.0f)); // no rack input behind it
        QCOMPARE(amp.inputSamples[3], std::make_pair(4096.0f, 4999.0f));
        QCOMPARE(amp.inputSamples[4], std::make_pair(-4096.0f, -4999.0f));
        QCOMPARE(synth.gotInputs, (std::vector<bool>{false, false}));

        // A host without inputs (offline renders): silence.
        run(rack, 64);
        QCOMPARE(amp.inputSamples.back(), std::make_pair(0.0f, 0.0f));
    }

    void deviceLimit()
    {
        std::vector<std::unique_ptr<FakeDevice>> devices;
        Rack rack;
        for (int i = 0; i < Rack::kMaxDevices; ++i) {
            devices.push_back(std::make_unique<FakeDevice>(kParamsA, 0.0f));
            QCOMPARE(rack.addDevice(*devices.back()), i);
        }
        FakeDevice extra(kParamsA, 0.0f);
        QCOMPARE(rack.addDevice(extra), -1);
    }
};

QTEST_APPLESS_MAIN(TestRack)
#include "tst_rack.moc"
