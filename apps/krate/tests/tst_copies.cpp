// Copies of an instrument in the rack (docs/DAW.md#copies): their entries,
// the device standing in until one is built, building it (its files from the
// first one's), the rack controller's part, and the sequencer's targets.

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "InstrumentCopies.h"
#include "NoteTargets.h"
#include "RackController.h"
#include "app/InstrumentRegistry.h"
#include "core/Transport.h"
#include "rack/Rack.h"

namespace {

std::string fmt(float v) { return std::to_string(v); }
constexpr krate::ParamDescriptor kParams[] = {
    {"level", "Level", 0.75f, 0, &fmt},
    {"tone", "Tone", 0.25f, 0, &fmt},
};

class Fake final : public krate::IInstrument
{
public:
    explicit Fake(float dc) : dc(dc) {}
    void prepare(double, std::uint32_t) noexcept override { ++prepared; }
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    float param(int index) const noexcept override { return values[std::size_t(index)]; }
    void setParam(int index, float v) noexcept override { values[std::size_t(index)] = v; }
    void setParamFromHost(int index, float v) noexcept override { values[std::size_t(index)] = v; }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    void setSequencerEnabled(bool on) noexcept override { enabled = on; }
    void setSequencerSuspended(bool on) noexcept override { suspended = on; }
    void process(const krate::AudioBlock &block) noexcept override
    {
        for (int c = 0; c < 2; ++c)
            std::fill_n(block.outputs[c], block.frames, dc);
    }

    float dc;
    int prepared = 0;
    bool enabled = true, suspended = false;
    std::array<float, 2> values{0.5f, 0.5f};
};

class FakeInstance final : public krate::InstrumentInstance
{
public:
    explicit FakeInstance(const krate::InstrumentContext &c, float dc = 0.5f) : context(c), fake(dc) {}
    krate::IInstrument &instrument() override { return fake; }
    void triggerAction(const QString &action) override { actions << action; }
    krate::InstrumentContext context;
    Fake fake;
    QStringList actions;
};

FakeInstance *g_lastBuilt = nullptr; // the instance the last entry's create() made

krate::InstrumentEntry entry(const QString &id, int copies, int *built = nullptr)
{
    krate::InstrumentEntry e;
    e.id = id;
    e.name = id.at(0).toUpper() + id.mid(1);
    e.maxCopies = copies;
    e.defaultMidiChannel = 9;
    e.qmlModule = QStringLiteral("Fake");
    e.panelComponent = QStringLiteral("Panel");
    e.create = [built](const krate::InstrumentContext &c) -> std::unique_ptr<krate::InstrumentInstance> {
        if (built)
            ++*built;
        auto instance = std::make_unique<FakeInstance>(c, 0.25f);
        g_lastBuilt = instance.get();
        return instance;
    };
    return e;
}

void write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestCopies : public QObject
{
    Q_OBJECT

private slots:
    void entries()
    {
        const auto copies = copyEntries(entry(QStringLiteral("kessel"), 16));
        QCOMPARE(copies.size(), 15);
        QCOMPARE(copies[0].id, QStringLiteral("kessel2"));
        QCOMPARE(copies[0].name, QStringLiteral("Kessel 2"));
        QCOMPARE(copies.back().id, QStringLiteral("kessel16"));
        QCOMPARE(copies[0].kind, QStringLiteral("kessel"));
        QCOMPARE(copies[0].kindOrId(), QStringLiteral("kessel"));
        QCOMPARE(copies[3].copy, 5);
        QVERIFY(!copies[0].inRackByDefault);
        QCOMPARE(copies[0].defaultMidiChannel, 9); // kept for the targets; the rack gives it none
        QVERIFY(copyEntries(entry(QStringLiteral("klick"), 1)).isEmpty());
    }

    // Until built: the first one's knobs at their defaults, silent; the RUN /
    // STOP and song settings it's given are the instrument's once built.
    void standsInUntilBuilt()
    {
        Fake first(1.0f), copy(0.25f);
        CopyInstrument device(first);
        QCOMPARE(device.parameters().size(), std::size_t(2));
        QCOMPARE(device.param(0), 0.75f);
        device.setParam(0, 0.1f); // nowhere to go yet
        device.setSequencerEnabled(false);
        device.setSequencerSuspended(true);
        std::vector<float> l(32, 9.0f), r(32, 9.0f);
        krate::processStereo(device, l.data(), r.data(), 32);
        QCOMPARE(l.back(), 0.0f);
        QCOMPARE(r.front(), 0.0f);

        device.setTarget(&copy);
        QVERIFY(!copy.enabled);
        QVERIFY(copy.suspended);
        device.setParam(1, 0.9f);
        QCOMPARE(copy.values[1], 0.9f);
        QCOMPARE(device.param(1), 0.9f);
        device.prepare(48000.0, 64);
        QCOMPARE(copy.prepared, 1);
        krate::processStereo(device, l.data(), r.data(), 32);
        QCOMPARE(l.back(), 0.25f);
    }

    // Built once: its own folders, a copy of the first one's (a file inside a
    // folder too); a second build, or folders already there, change nothing.
    void buildsWithTheFirstOnesFiles()
    {
        QTemporaryDir dir;
        write(dir.filePath("config/kessel/panel.json"), "first panel");
        write(dir.filePath("data/kessel/kits.json"), "first kits");
        write(dir.filePath("data/kessel/samples/kick.wav"), "RIFF");
        int built = 0;
        const auto base = entry(QStringLiteral("kessel"), 16, &built);
        krate::InstrumentContext firstContext;
        FakeInstance first(firstContext, 1.0f);

        const auto copy = copyEntries(base).at(0);
        krate::InstrumentContext context;
        context.configDir = dir.filePath("config/kessel2");
        context.dataDir = dir.filePath("data/kessel2");
        context.id = copy.id;
        CopyInstance instance(copy, context, first, dir.filePath("config/kessel"), dir.filePath("data/kessel"));
        QVERIFY(!instance.built());
        QCOMPARE(built, 0);
        QVERIFY(!QFileInfo::exists(context.configDir));
        instance.triggerAction(QStringLiteral("runstop")); // nothing to run yet
        QVERIFY(instance.panelProperties().isEmpty());
        QVERIFY(instance.saveSound().isEmpty());

        instance.build();
        instance.build();
        QCOMPARE(built, 1);
        QCOMPARE(read(dir.filePath("config/kessel2/panel.json")), QByteArray("first panel"));
        QCOMPARE(read(dir.filePath("data/kessel2/kits.json")), QByteArray("first kits"));
        QCOMPARE(read(dir.filePath("data/kessel2/samples/kick.wav")), QByteArray("RIFF"));
        FakeInstance *real = g_lastBuilt;
        QVERIFY(real);
        QCOMPARE(instanceTarget(instance), &real->fake);
        QCOMPARE(real->context.id, QStringLiteral("kessel2"));
        instance.triggerAction(QStringLiteral("runstop"));
        QCOMPARE(real->actions, QStringList{QStringLiteral("runstop")});

        write(dir.filePath("config/kessel/panel.json"), "changed"); // the first one's, later
        seedFolder(dir.filePath("config/kessel"), dir.filePath("config/kessel2"));
        QCOMPARE(read(dir.filePath("config/kessel2/panel.json")), QByteArray("first panel"));
    }

    // Loading a sound into a copy not built yet builds it (a song putting its sounds back).
    void aSoundBuildsIt()
    {
        QTemporaryDir dir;
        krate::InstrumentContext firstContext;
        FakeInstance first(firstContext);
        krate::InstrumentContext context;
        context.configDir = dir.filePath("c2");
        context.dataDir = dir.filePath("d2");
        CopyInstance instance(copyEntries(entry(QStringLiteral("korrode"), 16)).at(0), context, first, {}, {});
        instance.loadSound({}); // nothing to put back: stays unbuilt
        QVERIFY(!instance.built());
        instance.loadSound(QJsonObject{{QStringLiteral("params"), QJsonObject{{QStringLiteral("tone"), 0.125}}}});
        QVERIFY(instance.built());
        QCOMPARE(instance.instrument().param(1), 0.125f);
    }

    // In the rack: copies out until added, built just before they go in, with
    // no MIDI channel of their own; what each device is, and which copy.
    void rackController()
    {
        QList<krate::InstrumentEntry> entries{entry(QStringLiteral("kessel"), 3)};
        entries += copyEntries(entries[0]);
        std::vector<std::unique_ptr<Fake>> devices;
        krate::Rack rack;
        krate::Transport transport;
        for (int i = 0; i < 3; ++i) {
            devices.push_back(std::make_unique<Fake>(0.0f));
            rack.addDevice(*devices.back(), i == 0 ? 9 : -1);
        }
        RackController controller(rack, transport, entries, nullptr);
        QCOMPARE(controller.deviceKinds(), QStringList({"kessel", "kessel", "kessel"}));
        QCOMPARE(controller.deviceCopies(), QVariantList({1, 2, 3}));
        QCOMPARE(controller.presentDevices(), QVariantList{0});
        QList<int> built;
        controller.setBeforeAdding([&built, &rack](int d) {
            QVERIFY(!rack.present(d)); // before it's in
            built << d;
        });
        controller.setPresent(2, true);
        QCOMPARE(built, QList<int>{2});
        QCOMPARE(controller.presentDevices(), QVariantList({0, 2}));
        QCOMPARE(controller.present(), QVariantList({true, false, true}));
        controller.setPresent(2, false);
        controller.setPresent(2, true);
        QCOMPARE(built, QList<int>({2, 2})); // asked each time; building twice is the copy's to skip

        controller.restoreMidiChannels(2);
        QCOMPARE(controller.midiChannel(2), 0);
        QCOMPARE(controller.midiChannel(0), 10); // Kessel keeps channel 10
        controller.restoreMidiChannels(0);
        QCOMPARE(controller.midiChannel(0), 10);
    }

    // The sequencer's targets: a copy's own (Krimson 2's slots on the first
    // one's channels), Kessel's drum lanes for its copies too.
    void targets()
    {
        auto krimson = entry(QStringLiteral("krimson"), 2);
        krimson.defaultMidiChannel = 2;
        krimson.extraMidiChannels = {3, 4, 5};
        QList<krate::InstrumentEntry> entries{entry(QStringLiteral("kessel"), 2), krimson};
        entries.insert(1, copyEntries(entries[0]).at(0));
        entries += copyEntries(krimson);
        const auto targets = noteTargets(entries);
        QCOMPARE(int(targets.size()), 2 + 4 + 4);
        QCOMPARE(targets[1].key, QStringLiteral("kessel2"));
        QVERIFY(targets[1].drumMap);
        QCOMPARE(targets[1].kind, QStringLiteral("kessel"));
        QCOMPARE(targets[1].device, 1);
        QCOMPARE(targets[7].key, QStringLiteral("krimson2:B"));
        QCOMPARE(targets[7].label, QStringLiteral("Krimson 2 B"));
        QCOMPARE(targets[7].channel, 3);
        QCOMPARE(targets[7].kind, QStringLiteral("krimson"));
    }

private:
    // The instrument a copy plays once built (its device forwards to it).
    static krate::IInstrument *instanceTarget(CopyInstance &instance)
    {
        return static_cast<CopyInstrument &>(instance.instrument()).target();
    }
};

QTEST_MAIN(TestCopies)
#include "tst_copies.moc"
