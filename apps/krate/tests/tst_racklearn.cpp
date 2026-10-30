#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <span>
#include <string>
#include <vector>

#include "RackController.h"
#include "RackMidiLearn.h"
#include "RackRunControl.h"
#include "RackSession.h"
#include "app/InstrumentRegistry.h"
#include "core/Instrument.h"
#include "core/Transport.h"
#include "rack/Rack.h"

using namespace krate;
using Kind = Rack::Mapping::Kind;

namespace {

std::string fmt(float v) { return std::to_string(v); }

constexpr ParamDescriptor kParamsA[] = {{"cutoff", "Cutoff", 0.5f, 0, &fmt}, {"level", "Level", 0.5f, 0, &fmt}};
constexpr ParamDescriptor kParamsB[] = {{"level", "Level", 0.5f, 0, &fmt}, {"tone", "Tone", 0.5f, 0, &fmt}};

class Fake final : public IInstrument
{
public:
    explicit Fake(std::span<const ParamDescriptor> p) : params(p), values(p.size(), 0.0f) {}
    void prepare(double, std::uint32_t) noexcept override {}
    std::span<const ParamDescriptor> parameters() const noexcept override { return params; }
    float param(int i) const noexcept override { return values[std::size_t(i)]; }
    void setParam(int i, float v) noexcept override { values[std::size_t(i)] = v; }
    void setParamFromHost(int i, float v) noexcept override { values[std::size_t(i)] = v; }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    void process(const AudioBlock &b) noexcept override
    {
        std::fill_n(b.outputs[0], b.frames, 0.0f);
        std::fill_n(b.outputs[1], b.frames, 0.0f);
        for (std::uint32_t i = 0; i < b.midiCount; ++i)
            if ((b.midi[i].data[0] & 0xF0) == 0x90)
                ++notes;
    }
    std::span<const ParamDescriptor> params;
    std::vector<float> values;
    int notes = 0; // note-ons received
    bool sequencer = true;
    void setSequencerEnabled(bool on) noexcept override { sequencer = on; }
};

class FakeInstance final : public InstrumentInstance
{
public:
    explicit FakeInstance(IInstrument &i) : inst(i) {}
    IInstrument &instrument() override { return inst; }
    void triggerAction(const QString &a) override { triggered << a; }
    IInstrument &inst;
    QStringList triggered;
};

MidiEvent cc(int ch, int n, int v) { return MidiEvent{0, 3, {std::uint8_t(0xB0 | ch), std::uint8_t(n), std::uint8_t(v)}}; }
MidiEvent noteOn(int ch, int n) { return MidiEvent{0, 3, {std::uint8_t(0x90 | ch), std::uint8_t(n), 100}}; }

// A rack of two fakes ("alpha": cutoff/level + buttons, "beta": level/tone).
struct Fixture
{
    Fake a{kParamsA}, b{kParamsB};
    FakeInstance ia{a}, ib{b};
    Rack rack;
    Transport transport;
    QList<InstrumentEntry> entries;
    std::unique_ptr<RackController> controller;
    std::unique_ptr<RackMidiLearn> learn;
    int playStops = 0;
    int mixerToggles = 0;

    explicit Fixture(bool reversed = false)
    {
        InstrumentEntry ea, eb;
        ea.id = "alpha";
        ea.name = "Alpha";
        ea.actions = {"pattern1", "runstop"};
        ea.panelComponent = "Panel"; // instruments with panels: they take focus
        eb.id = "beta";
        eb.name = "Beta";
        eb.panelComponent = "Panel";
        std::vector<RackMidiLearn::Device> devices;
        if (!reversed) {
            rack.addDevice(a, 0);
            rack.addDevice(b);
            entries = {ea, eb};
            devices = {{"alpha", kParamsA, ea.actions, &ia}, {"beta", kParamsB, {}, &ib}};
        } else {
            rack.addDevice(b);
            rack.addDevice(a, 0);
            entries = {eb, ea};
            devices = {{"beta", kParamsB, {}, &ib}, {"alpha", kParamsA, ea.actions, &ia}};
        }
        controller = std::make_unique<RackController>(rack, transport, entries, nullptr);
        learn = std::make_unique<RackMidiLearn>(rack, std::move(devices), [this](const QString &action) {
            if (action == u"playstop")
                ++playStops;
            else if (action == u"mixer")
                ++mixerToggles;
        }, nullptr);
    }

    void send(std::vector<MidiEvent> midi)
    {
        std::vector<float> l(64), r(64);
        RackBlock block;
        block.frames = 64;
        block.midi = midi.data();
        block.midiCount = std::uint32_t(midi.size());
        block.masterL = l.data();
        block.masterR = r.data();
        rack.process(block);
        learn->drainEvents();
    }

    void teach(const QString &key, std::vector<MidiEvent> midi)
    {
        learn->setActive(true);
        learn->arm(key);
        send(std::move(midi));
        learn->setActive(false);
    }
};

} // namespace

class TestRackLearn : public QObject
{
    Q_OBJECT

private slots:
    // A panel knob learned in the rack drives that instrument, whatever has focus.
    void fixedKnob()
    {
        Fixture f;
        f.teach("cutoff", {cc(2, 20, 127)}); // alpha has focus
        QCOMPARE(f.a.values[0], 1.0f);
        QCOMPARE(f.learn->badgeFor("cutoff"), QStringLiteral("CC 20 ch3"));
        f.controller->setFocus(1);
        f.send({cc(2, 20, 0)});
        QCOMPARE(f.a.values[0], 0.0f);        // still alpha's
        QCOMPARE(f.learn->badgeFor("cutoff"), QString()); // beta has no cutoff
    }

    void followFocusKnob()
    {
        Fixture f;
        f.learn->setFollowFocus(true);
        f.teach("level", {cc(0, 21, 127)});
        QCOMPARE(int(f.rack.ccMapping(0, 21).kind), int(Kind::FollowFocus));
        QCOMPARE(f.a.values[1], 1.0f);
        f.controller->setFocus(1);
        f.send({cc(0, 21, 64)});
        QCOMPARE(f.b.values[0], 64.0f / 127.0f); // now beta's level
        QCOMPARE(f.learn->badgeFor("level"), QStringLiteral("CC 21 ch1")); // shown on beta too
    }

    void instrumentButton()
    {
        Fixture f;
        f.teach("pattern1", {cc(15, 47, 127)});
        QVERIFY(f.ia.triggered.isEmpty()); // the teaching press
        f.send({cc(15, 47, 127), cc(15, 47, 0)});
        QCOMPARE(f.ia.triggered, QStringList{"pattern1"});
        QCOMPARE(f.learn->badgeFor("pattern1"), QStringLiteral("CC 47 ch16"));
    }

    void rackButtons()
    {
        Fixture f;
        f.teach("rack:playstop", {cc(15, 50, 127)});
        f.send({cc(15, 50, 127)});
        QCOMPARE(f.playStops, 1);

        f.teach("rack:focus:1", {noteOn(9, 36)}); // a pad
        f.send({noteOn(9, 36)});
        QCOMPARE(f.rack.focus(), 1);
        QCOMPARE(f.learn->badgeFor("rack:focus:1"), QStringLiteral("note 36 ch10"));

        f.teach("rack:next", {cc(15, 51, 127)});
        f.send({cc(15, 51, 127)});
        QCOMPARE(f.rack.focus(), 0);

        f.teach("rack:mixer", {noteOn(9, 37)});
        f.send({noteOn(9, 37)});
        QCOMPARE(f.mixerToggles, 1);
        QCOMPARE(f.playStops, 1);
    }

    // Controller faders, knobs and buttons on the mixer strips (and the master fader).
    void mixerControls()
    {
        Fixture f;
        f.teach("rack:mix:1:fader", {cc(0, 7, 127)});
        f.send({cc(0, 7, 0), cc(0, 7, 127)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->fader(1), 1.0);
        f.send({cc(0, 7, 64)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->fader(1), 64 / 127.0);
        QCOMPARE(f.learn->badgeFor("rack:mix:1:fader"), QStringLiteral("CC 7 ch1"));
        QCOMPARE(f.learn->badgeFor("rack:mix:0:fader"), QString()); // only strip 1's

        f.teach("rack:mix:0:pan", {cc(0, 10, 64)});
        f.send({cc(0, 10, 0)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->pan(0), -1.0);
        f.send({cc(0, 10, 64)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->pan(0), 0.0);

        f.teach("rack:mix:0:mute", {noteOn(9, 40)});
        f.send({noteOn(9, 40)});
        f.controller->applyMixerMoves();
        QVERIFY(f.controller->mute(0));
        f.send({noteOn(9, 40)});
        f.controller->applyMixerMoves();
        QVERIFY(!f.controller->mute(0));
        QCOMPARE(f.a.notes, 0); // the pad is a button, not a note

        f.teach("rack:mix:1:solo", {cc(15, 61, 127)});
        f.send({cc(15, 61, 127)});
        f.controller->applyMixerMoves();
        QVERIFY(f.controller->solo(1));

        f.teach("rack:mix:master:fader", {cc(0, 8, 0)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->fader(-1), 0.0);
        f.learn->setActive(true);
        f.learn->arm("rack:mix:master:mute"); // the master has only a fader
        QVERIFY(f.learn->armedParam().isEmpty());
        f.learn->arm("rack:mix:9:fader");    // no such strip
        QVERIFY(f.learn->armedParam().isEmpty());
        f.learn->setActive(false);
    }

    // Strip EQ: knob positions map to dB / Hz / Q and back; every strip starts
    // flat; it reaches the rack; the mixer state keeps it (older states: flat);
    // a learned knob turns it.
    void stripEq()
    {
        Fixture f;
        for (int b = 0; b < ChannelEq::Count; ++b) {
            QVERIFY(std::fabs(RackController::eqValue(b, f.controller->eq(0, b)) - ChannelEq::kFlat[std::size_t(b)]) < 1e-3);
            QVERIFY(std::fabs(RackController::eqPosition(b, RackController::eqValue(b, 0.3)) - 0.3) < 1e-9);
        }
        QVERIFY(f.controller->eqFlat(0));
        QCOMPARE(RackController::eqValue(ChannelEq::MidGain, 1.0), 15.0);
        QCOMPARE(RackController::eqValue(ChannelEq::LowFreq, 0.0), 30.0);
        QCOMPARE(f.controller->eqText(ChannelEq::MidGain, 0.6), QStringLiteral("+3.0 dB"));
        QCOMPARE(f.controller->eqText(ChannelEq::HighFreq, 1.0), QStringLiteral("16.0 kHz"));
        QCOMPARE(f.controller->eqText(ChannelEq::LowFreq, 0.0), QStringLiteral("30 Hz"));

        f.controller->setEq(1, ChannelEq::MidGain, 0.8);
        QVERIFY(!f.controller->eqFlat(1));
        QVERIFY(std::fabs(f.rack.eq(1, ChannelEq::MidGain) - 9.0f) < 1e-4f);
        f.controller->setEq(1, ChannelEq::MidGain, 0.503); // near the middle: exactly 0 dB
        QVERIFY(f.controller->eqFlat(1));
        f.controller->setEq(1, ChannelEq::HighGain, 0.0);
        const QJsonObject state = f.controller->mixerState();
        f.controller->resetEq(1);
        QVERIFY(f.controller->eqFlat(1));
        f.controller->setMixerState(state);
        QCOMPARE(f.controller->eq(1, ChannelEq::HighGain), 0.0);
        QVERIFY(std::fabs(f.rack.eq(1, ChannelEq::HighGain) + 15.0f) < 1e-4f);

        QJsonObject old = state; // from before the EQ: no "eq" in its strips
        QJsonObject strips = old.value(u"strips").toObject();
        QJsonObject beta = strips.value(u"beta").toObject();
        beta.remove(u"eq");
        strips.insert(u"beta", beta);
        old.insert(u"strips", strips);
        f.controller->setMixerState(old);
        QVERIFY(f.controller->eqFlat(1));

        f.teach("rack:mix:0:midfreq", {cc(0, 20, 0)});
        f.send({cc(0, 20, 127)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->eq(0, ChannelEq::MidFreq), 1.0);
        QVERIFY(std::fabs(f.rack.eq(0, ChannelEq::MidFreq) - 10000.0f) < 0.5f);
    }

    // Effects: sends per strip and the master's returns reach the rack, are
    // kept in the mixer state (older states: no sends, default returns), the
    // delay time snaps to its 7 positions, and both learn (returns only on the master).
    void effects()
    {
        Fixture f;
        QCOMPARE(f.controller->send(0, 0), 0.0);
        QCOMPARE(f.controller->effectText(3, f.controller->effect(3)), QStringLiteral("1/8."));
        f.controller->setSend(1, 0, 0.4);
        f.controller->setSend(1, 1, 0.25);
        QVERIFY(std::fabs(f.rack.send(1, Rack::Reverb) - 0.4f) < 1e-6f);
        f.controller->setEffect(3, 0.95); // delay time: snaps to 1/2
        QCOMPARE(f.controller->effect(3), 1.0);
        QCOMPARE(f.controller->effectText(3, 1.0), QStringLiteral("1/2"));
        QCOMPARE(f.controller->sendText(0.0), QStringLiteral("off"));

        const QJsonObject state = f.controller->mixerState();
        f.controller->setSend(1, 0, 0.0);
        f.controller->setEffect(0, 0.1);
        f.controller->setMixerState(state);
        QCOMPARE(f.controller->send(1, 0), 0.4);
        QCOMPARE(f.controller->send(1, 1), 0.25);
        QCOMPARE(f.controller->effect(0), f.controller->effectDefault(0));
        QCOMPARE(f.controller->effect(3), 1.0);

        f.controller->setMixerState(QJsonObject{{"strips", QJsonObject{{"beta", QJsonObject{{"fader", 0.8}}}}}});
        QCOMPARE(f.controller->send(1, 0), 0.0);    // an older state: no sends
        QCOMPARE(f.controller->effect(3), 0.5);     // and the returns' defaults

        f.teach("rack:mix:0:revsend", {cc(0, 30, 0)});
        f.send({cc(0, 30, 127)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->send(0, 0), 1.0);
        f.teach("rack:mix:master:dlyfeedback", {cc(0, 31, 0)});
        f.send({cc(0, 31, 127)});
        f.controller->applyMixerMoves();
        QCOMPARE(f.controller->effect(4), 1.0);
        f.learn->setActive(true);
        f.learn->arm("rack:mix:0:revlevel"); // a return: only on the master
        QVERIFY(f.learn->armedParam().isEmpty());
        f.learn->arm("rack:mix:master:revsend"); // a send: only on a strip
        QVERIFY(f.learn->armedParam().isEmpty());
        f.learn->setActive(false);
    }

    // Each instrument's own RUN / STOP runs just its sequencer; the rack's PLAY runs them all.
    void instrumentsRunOnTheirOwn()
    {
        Fixture f; // alpha, beta
        RackRunControl run(f.rack, f.transport, {"alpha", "beta"});
        run.addRunner("alpha");
        run.addRunner("beta");
        f.controller->setRunControl(&run);
        QSignalSpy changed(&run, &krate::RunControl::changed);

        run.setRunning("beta", true); // from silence: beta alone
        QVERIFY(f.transport.playing());
        QVERIFY(run.running("beta"));
        QVERIFY(!run.running("alpha"));
        QVERIFY(f.b.sequencer);
        QVERIFY(!f.a.sequencer);
        run.setRunning("alpha", true); // joins while beta plays
        QVERIFY(run.running("alpha"));
        QVERIFY(f.a.sequencer);
        run.setRunning("beta", false); // alpha keeps going
        QVERIFY(f.transport.playing());
        QVERIFY(!f.b.sequencer);
        run.setRunning("alpha", false); // the last one: the transport stops
        QVERIFY(!f.transport.playing());
        QVERIFY(changed.count() >= 4);

        f.controller->setPlaying(true); // the rack's PLAY: everyone
        QVERIFY(f.a.sequencer && f.b.sequencer);
        QVERIFY(run.running("alpha") && run.running("beta"));
        f.controller->setPlaying(false);
        QVERIFY(!run.running("alpha"));
        run.setRunning("nobody", true); // unknown: nothing
        QVERIFY(!f.transport.playing());
    }

    // The mixer screen: shown on request, gone when focus changes (from a tab,
    // or from the controller).
    void mixerScreen()
    {
        Fixture f;
        QVERIFY(!f.controller->mixerShown());
        QSignalSpy spy(f.controller.get(), &RackController::mixerShownChanged);
        f.controller->setMixerShown(true);
        QVERIFY(f.controller->mixerShown());
        f.controller->setFocus(1);
        QVERIFY(!f.controller->mixerShown());
        f.controller->setMixerShown(true);
        f.rack.setFocus(0); // as a learned focus button does
        QTRY_VERIFY(!f.controller->mixerShown());
        QCOMPARE(spy.count(), 4);
        // The song screen: one or the other.
        f.controller->setMixerShown(true);
        f.controller->setSongShown(true);
        QVERIFY(f.controller->songShown());
        QVERIFY(!f.controller->mixerShown());
        f.controller->setFocus(1);
        QVERIFY(!f.controller->songShown());
        // Saved and restored.
        QTemporaryDir dir;
        const QString path = dir.filePath("rack.json");
        f.controller->setSongShown(true);
        {
            RackSession session(path, *f.controller, f.rack, *f.learn, {"alpha", "beta"}, nullptr);
            session.save();
        }
        Fixture g;
        RackSession session(path, *g.controller, g.rack, *g.learn, {"alpha", "beta"}, nullptr);
        session.load();
        QVERIFY(g.controller->songShown());
        QVERIFY(!g.controller->mixerShown());
    }

    void clearAndClearAll()
    {
        Fixture f;
        f.teach("cutoff", {cc(0, 20, 1)});
        f.teach("rack:playstop", {cc(0, 50, 127)});
        f.learn->clear("cutoff");
        QCOMPARE(int(f.rack.ccMapping(0, 20).kind), int(Kind::None));
        QCOMPARE(int(f.rack.ccMapping(0, 50).kind), int(Kind::Action));
        f.learn->clearAll();
        QCOMPARE(int(f.rack.ccMapping(0, 50).kind), int(Kind::None));
    }

    // Mappings are saved by instrument id and key: they survive a reordered rack.
    void mappingsSurviveReordering()
    {
        QJsonArray saved;
        {
            Fixture f;
            f.teach("cutoff", {cc(2, 20, 1)});
            f.teach("pattern1", {cc(15, 47, 127)});
            f.teach("rack:focus:1", {noteOn(9, 36)}); // focus beta
            f.learn->setFollowFocus(true);
            f.teach("level", {cc(0, 21, 1)});
            saved = f.learn->saveMappings();
            QCOMPARE(saved.size(), 4);
        }
        Fixture g(/*reversed*/ true); // beta is device 0 now, alpha device 1
        g.learn->loadMappings(saved);
        QCOMPARE(g.rack.ccMapping(2, 20).device, 1);           // alpha's cutoff
        QCOMPARE(g.rack.ccMapping(15, 47).device, 1);          // alpha's pattern1
        QCOMPARE(g.rack.noteMapping(9, 36).target, 0);         // focus beta = device 0
        QCOMPARE(int(g.rack.ccMapping(0, 21).kind), int(Kind::FollowFocus));
    }

    void sessionRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("rack.json");
        {
            Fixture f;
            RackSession session(path, *f.controller, f.rack, *f.learn, {"alpha", "beta"}, nullptr);
            session.load(); // nothing yet: defaults
            QCOMPARE(session.midiInput(), QStringLiteral("Impact LX49"));
            f.controller->setBpm(133);
            f.controller->setFader(1, 0.5);
            f.controller->setPan(0, -0.25);
            f.controller->setMute(1, true);
            f.controller->setFocus(1);
            f.rack.setChannelDevice(0, -1);
            f.rack.setChannelDevice(9, 1); // beta on channel 10
            f.teach("rack:playstop", {cc(15, 50, 127)});
            f.teach("rack:mixer", {cc(15, 52, 127)});
            f.teach("rack:mix:1:fader", {cc(0, 7, 100)});
            f.teach("rack:mix:0:mute", {noteOn(9, 40)});
            f.teach("rack:mix:master:fader", {cc(0, 8, 100)});
            f.controller->setMixerShown(true);
            session.save();
        }
        Fixture g;
        RackSession session(path, *g.controller, g.rack, *g.learn, {"alpha", "beta"}, nullptr);
        session.load();
        QCOMPARE(g.controller->bpm(), 133.0);
        QCOMPARE(g.controller->fader(1), 0.5);
        QCOMPARE(g.controller->pan(0), -0.25);
        QVERIFY(g.controller->mute(1));
        QCOMPARE(g.controller->focus(), 1);
        QVERIFY(g.controller->mixerShown()); // reopens on the mixer screen
        QCOMPARE(g.rack.channelDevice(9), 1);
        QCOMPARE(g.rack.channelDevice(0), -1);
        QCOMPARE(int(g.rack.ccMapping(15, 50).kind), int(Kind::Action));
        g.send({cc(15, 52, 127)});
        QCOMPARE(g.mixerToggles, 1);
        QCOMPARE(g.playStops, 0);
        QCOMPARE(g.learn->badgeFor("rack:mix:1:fader"), QStringLiteral("CC 7 ch1"));
        QCOMPARE(g.learn->badgeFor("rack:mix:0:mute"), QStringLiteral("note 40 ch10"));
        QCOMPARE(g.learn->badgeFor("rack:mix:master:fader"), QStringLiteral("CC 8 ch1"));
    }

    // The mixer strip's channel selector: a channel goes to one device; FOCUS
    // (0) frees it, so those notes follow focus.
    void midiChannelSelector()
    {
        Fixture f; // alpha on channel 1
        QCOMPARE(f.controller->midiChannel(0), 1);
        QCOMPARE(f.controller->midiChannel(1), 0);
        f.controller->setMidiChannel(1, 1); // beta takes channel 1 from alpha
        QCOMPARE(f.controller->midiChannel(1), 1);
        QCOMPARE(f.controller->midiChannel(0), 0);
        QCOMPARE(f.rack.channelDevice(0), 1);
        f.controller->setMidiChannel(1, 0);  // FOCUS
        QCOMPARE(f.rack.channelDevice(0), -1);
        // Channel 1 now follows focus: with focus on alpha, its notes reach alpha.
        f.controller->setFocus(0);
        f.send({noteOn(0, 60)});
        QVERIFY(f.a.notes > 0);
        QCOMPARE(f.b.notes, 0);
    }

    // A device can own several channels (Krimson: one per slot); sessions keep
    // the list, and older sessions' single "channel" still loads.
    void multiChannelDevices()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("rack.json");
        {
            Fixture f;
            f.entries[1].defaultMidiChannel = 2;
            f.entries[1].extraMidiChannels = {3, 4, 5};
            f.controller = std::make_unique<RackController>(f.rack, f.transport, f.entries, nullptr);
            f.controller->restoreMidiChannels(1);
            QCOMPARE(f.controller->midiChannel(1), 3);
            QCOMPARE(f.controller->midiChannelCount(1), 4);
            QCOMPARE(f.rack.channelDevice(5), 1);
            f.controller->setMidiChannel(1, 10); // a single channel again
            QCOMPARE(f.controller->midiChannelCount(1), 1);
            QCOMPARE(f.rack.channelDevice(3), -1);
            f.controller->restoreMidiChannels(1);
            RackSession session(path, *f.controller, f.rack, *f.learn, {"alpha", "beta"}, nullptr);
            session.save();
        }
        {
            Fixture g;
            RackSession session(path, *g.controller, g.rack, *g.learn, {"alpha", "beta"}, nullptr);
            session.load();
            QCOMPARE(g.controller->midiChannel(1), 3);
            QCOMPARE(g.controller->midiChannelCount(1), 4);
            QCOMPARE(g.controller->midiChannel(0), 1);
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({ "version": 1, "devices": { "alpha": { "channel": 0 }, "beta": { "channel": 7 } } })");
        file.close();
        Fixture h;
        RackSession session(path, *h.controller, h.rack, *h.learn, {"alpha", "beta"}, nullptr);
        session.load();
        QCOMPARE(h.controller->midiChannel(1), 7);
        QCOMPARE(h.controller->midiChannelCount(1), 1);
        QCOMPARE(h.controller->midiChannel(0), 0);
    }

    void sessionIgnoresUnknownInstruments()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("rack.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({ "version": 1, "focus": "gone", "devices": { "gone": { "fader": 0.1 } },
                        "mappings": [ { "source": "cc", "channel": 1, "number": 5, "kind": "param",
                                        "device": "gone", "key": "cutoff" } ] })");
        file.close();
        Fixture f;
        RackSession session(path, *f.controller, f.rack, *f.learn, {"alpha", "beta"}, nullptr);
        session.load();
        QCOMPARE(f.controller->focus(), 0);
        QCOMPARE(f.controller->fader(0), RackController::kUnityFader);
        QCOMPARE(int(f.rack.ccMapping(0, 5).kind), int(Kind::None));
        QCOMPARE(f.rack.channelDevice(0), 0); // registry default kept
    }

    // A device without a panel (the backing track) is passed over by the
    // rack's focus actions (learned buttons).
    void panelLessDevicesDontTakeFocus()
    {
        Fixture f;
        f.rack.setFocusable(1, false);
        f.controller->setFocus(0);
        f.rack.setNoteMapping(0, 60, {krate::Rack::Mapping::Kind::FocusAction, -1, krate::Rack::kFocusNext});
        f.send({noteOn(0, 60)});
        QCOMPARE(f.rack.focus(), 0); // the only other device can't take it
        f.rack.setNoteMapping(0, 61, {krate::Rack::Mapping::Kind::FocusAction, -1, 1}); // straight to it: refused
        f.send({noteOn(0, 61)});
        QCOMPARE(f.rack.focus(), 0);
        f.rack.setFocusable(1, true);
        f.send({noteOn(0, 60)});
        QCOMPARE(f.rack.focus(), 1);
    }
};

QTEST_GUILESS_MAIN(TestRackLearn)
#include "tst_racklearn.moc"
