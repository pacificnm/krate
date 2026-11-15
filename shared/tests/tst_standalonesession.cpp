// A standalone app's own settings (StandaloneSession): the audio choices and the
// updates switch kept in app.json, a first start taking Krate's choices, and the
// interface's inputs wired in order when nothing was ever chosen.

#include <QtTest>

#include <QJsonArray>
#include <QTemporaryDir>

#include "app/HostController.h"
#include "app/JsonFile.h"
#include "app/StandaloneSession.h"
#include "app/UpdateController.h"
#include "host/AudioHost.h"

namespace {

// Two inputs and an interface with two jacks; opens (or not) on demand.
class FakeHost final : public krate::AudioHost
{
public:
    bool canOpen = true;
    bool open(const char *, std::string *error) override
    {
        if (!canOpen)
            *error = "no server";
        m_open = canOpen;
        return canOpen;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    bool serverGone() const override { return false; }
    std::uint32_t sampleRate() const override { return m_open ? 48000 : 0; }
    std::uint32_t bufferSize() const override { return 128; }
    std::uint32_t xrunCount() const override { return 0; }
    float takePeak() noexcept override { return 0.0f; }
    std::vector<std::string> connectToPlayback(const std::string &) override { return {}; }
    std::vector<std::string> midiSourcePorts(bool) const override { return {}; }
    bool connectMidiFrom(const std::string &) override { return false; }
    bool midiInputConnected() const override { return false; }
    bool takePortsChanged() override { return false; }
    int inputCount() const override { return 2; }
    std::vector<std::string> captureSourcePorts() const override
    {
        return {"Interface:capture_FL", "Interface:capture_FR"};
    }
    bool connectInput(int, const std::string &) override { return true; }
    bool setBufferSize(std::uint32_t) override { return true; }

private:
    bool m_open = false;
};

QJsonObject readFile(const QString &path)
{
    QJsonObject json;
    krate::files::readVersionedJson(path, 1, &json);
    return json;
}

} // namespace

class TestStandaloneSession : public QObject
{
    Q_OBJECT

private slots:
    void keepsTheChoices()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("app.json"));
        {
            FakeHost fake;
            fake.canOpen = false; // offline: nothing is wired by itself
            krate::HostController host(fake, QStringLiteral("test"), nullptr);
            UpdateController updates(QStringLiteral("0.1.0"), QStringLiteral("/nonexistent"), QStringLiteral("/nonexistent"));
            krate::StandaloneSession session(path, host, &updates);
            QVERIFY(!session.seeded());
            host.setInputPort(1, QStringLiteral("Interface:capture_FR"));
            host.setRequestedBufferSize(64);
            host.setOutputDevice(QStringLiteral("Headphones"));
            updates.setAutoCheck(false);
        }
        const QJsonObject json = readFile(path);
        QCOMPARE(json.value(u"audioInputs").toArray(), (QJsonArray{QString(), QStringLiteral("Interface:capture_FR")}));
        QCOMPARE(json.value(u"bufferSize").toInt(), 64);
        QCOMPARE(json.value(u"outputDevice").toString(), QStringLiteral("Headphones"));
        QCOMPARE(json.value(u"autoCheckUpdates").toBool(true), false);

        // The next start has them back.
        FakeHost fake;
        fake.canOpen = false;
        krate::HostController host(fake, QStringLiteral("test"), nullptr);
        UpdateController updates(QStringLiteral("0.1.0"), QStringLiteral("/nonexistent"), QStringLiteral("/nonexistent"));
        krate::StandaloneSession session(path, host, &updates);
        QCOMPARE(host.inputPorts(), (QStringList{QString(), QStringLiteral("Interface:capture_FR")}));
        QCOMPARE(host.requestedBufferSize(), 64);
        QCOMPARE(host.outputDevice(), QStringLiteral("Headphones"));
        QVERIFY(!updates.autoCheck());
    }

    // A first start: Krate's guitar input and buffer, then this app's own file.
    void takesKratesChoicesFirst()
    {
        QTemporaryDir dir;
        const QString rack = dir.filePath(QStringLiteral("rack.json"));
        QVERIFY(krate::files::writeJsonFile(rack, QJsonObject{{QStringLiteral("version"), 1},
                                                              {QStringLiteral("audioInputs"), QJsonArray{QStringLiteral("Interface:capture_FR")}},
                                                              {QStringLiteral("bufferSize"), 128},
                                                              {QStringLiteral("tempo"), 99}}));
        const QString path = dir.filePath(QStringLiteral("app.json"));
        FakeHost fake;
        krate::HostController host(fake, QStringLiteral("test"), nullptr);
        krate::StandaloneSession session(path, host, nullptr, rack);
        QVERIFY(session.seeded());
        QCOMPARE(host.inputPorts().value(0), QStringLiteral("Interface:capture_FR")); // not rewired in order
        QCOMPARE(host.requestedBufferSize(), 128);
        QCOMPARE(readFile(path).value(u"bufferSize").toInt(), 128);
    }

    // Nothing chosen anywhere: the interface's jacks in order, kept.
    void wiresTheInterfaceInOrder()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("app.json"));
        FakeHost fake;
        krate::HostController host(fake, QStringLiteral("test"), nullptr);
        krate::StandaloneSession session(path, host, nullptr, dir.filePath(QStringLiteral("missing.json")));
        QVERIFY(!session.seeded());
        QCOMPARE(host.inputPorts(), (QStringList{QStringLiteral("Interface:capture_FL"), QStringLiteral("Interface:capture_FR")}));
        QCOMPARE(readFile(path).value(u"audioInputs").toArray().size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestStandaloneSession)
#include "tst_standalonesession.moc"
