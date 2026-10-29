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

    explicit Fixture(bool reversed = false)
    {
        InstrumentEntry ea, eb;
        ea.id = "alpha";
        ea.name = "Alpha";
        ea.actions = {"pattern1", "runstop"};
        eb.id = "beta";
        eb.name = "Beta";
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
        learn = std::make_unique<RackMidiLearn>(rack, std::move(devices), [this] { ++playStops; }, nullptr);
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
        QCOMPARE(g.rack.channelDevice(9), 1);
        QCOMPARE(g.rack.channelDevice(0), -1);
        QCOMPARE(int(g.rack.ccMapping(15, 50).kind), int(Kind::Action));
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
};

QTEST_GUILESS_MAIN(TestRackLearn)
#include "tst_racklearn.moc"
