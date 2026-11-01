#include <QtTest>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>

#include "AudioTracks.h"

// Krate's audio tracks (docs/DAW.md, D2): a take recorded through the
// controller lands in the track's folder and tracks.json, plays, survives a
// restart, and can be picked or deleted. The test plays the audio thread.
class TestAudioTracks : public QObject
{
    Q_OBJECT

    static constexpr std::uint32_t kBlock = 256;

    struct Feed
    {
        krate::AudioTrack &track;
        double beat = 0.0;
        std::vector<float> a = std::vector<float>(kBlock, 0.5f), b = std::vector<float>(kBlock, -0.25f);
        std::vector<float> l = std::vector<float>(kBlock), r = std::vector<float>(kBlock);
        void block(bool playing, bool restarted = false)
        {
            const float *inputs[2] = {a.data(), b.data()};
            float *outputs[2] = {l.data(), r.data()};
            krate::TransportState t;
            t.playing = playing;
            t.restarted = restarted;
            t.bpm = 120.0;
            t.beatAtBlockStart = beat;
            track.process(krate::AudioBlock{outputs, kBlock, nullptr, 0, &t, inputs});
            if (playing)
                beat += kBlock * 2.0 / 48000.0;
        }
    };

    static void waitFor(const std::function<bool()> &done)
    {
        for (int k = 0; k < 200 && !done(); ++k)
            QTest::qWait(20);
    }

private slots:
    void recordsKeepsAndDeletes()
    {
        QTemporaryDir dir;
        const QString recordings = dir.filePath("recordings");
        QString file;
        {
            AudioTracksController tracks(recordings);
            tracks.setRoundTrip([] { return 64; });
            krate::AudioTrack &engine = tracks.engine(2);
            engine.prepare(48000.0, kBlock);
            Feed feed{engine};
            tracks.setParam(2, krate::AudioTrack::Arm, 1.0);
            tracks.setRecording(true);
            feed.block(true, true);
            for (int k = 0; k < 99; ++k)
                feed.block(true);
            feed.block(false);
            waitFor([&] { return !tracks.takes(2).isEmpty(); });
            QCOMPARE(tracks.takes(2).size(), 1);
            QCOMPARE(tracks.activeTake(2), 0);
            const QVariantMap take = tracks.takes(2)[0].toMap();
            QCOMPARE(take.value("seconds").toDouble(), 100.0 * kBlock / 48000.0);
            QVERIFY(take.value("active").toBool());
            file = take.value("name").toString() + ".wav";
            QVERIFY(QFileInfo::exists(QDir(recordings).filePath("audio3/" + file)));

            // Loaded and handed to the track: disarmed, it plays the take
            // (the input, 0.5 / -0.25), its round trip skipped.
            waitFor([&] { return !tracks.peaks(2).isEmpty(); });
            QVERIFY(std::fabs(tracks.peaks(2)[10].toDouble() - 0.5) < 1e-3);
            tracks.setParam(2, krate::AudioTrack::Arm, 0.0);
            tracks.setRecording(false);
            feed.beat = 0.0;
            feed.block(true, true);
            QVERIFY(std::fabs(feed.l[3] - 0.5f) < 1e-5f);
            QVERIFY(std::fabs(feed.r[3] + 0.25f) < 1e-5f);
            feed.block(false);
        }

        // tracks.json: the take with its offset, active.
        QFile json(QDir(recordings).filePath("tracks.json"));
        QVERIFY(json.open(QIODevice::ReadOnly));
        const QJsonObject track3 = QJsonDocument::fromJson(json.readAll()).object()["tracks"].toArray()[2].toObject();
        QCOMPARE(track3["active"].toString(), file);
        QCOMPARE(track3["takes"].toArray()[0].toObject()["offset"].toInt(), 64);
        QCOMPARE(track3["arm"].toDouble(), 0.0);

        // Reopened: the take is back and playing; none, then deleted with its file.
        AudioTracksController tracks(recordings);
        QCOMPARE(tracks.takes(2).size(), 1);
        QCOMPARE(tracks.activeTake(2), 0);
        waitFor([&] { return !tracks.peaks(2).isEmpty(); });
        QVERIFY(!tracks.peaks(2).isEmpty());
        tracks.setActiveTake(2, -1);
        QVERIFY(tracks.peaks(2).isEmpty());
        tracks.deleteTake(2, 0);
        QVERIFY(tracks.takes(2).isEmpty());
        QVERIFY(!QFileInfo::exists(QDir(recordings).filePath("audio3/" + file)));
    }

    // Quitting while recording keeps the take; a track that recorded
    // nothing leaves no file.
    void quittingKeepsTheTake()
    {
        QTemporaryDir dir;
        const QString recordings = dir.filePath("recordings");
        {
            AudioTracksController tracks(recordings);
            krate::AudioTrack &engine = tracks.engine(0);
            engine.prepare(48000.0, kBlock);
            Feed feed{engine};
            tracks.setParam(0, krate::AudioTrack::Arm, 1.0);
            tracks.setRecording(true);
            feed.block(true, true);
            feed.block(true); // still recording when it quits
            tracks.finish();
        }
        AudioTracksController tracks(recordings);
        QCOMPARE(tracks.takes(0).size(), 1);
        QCOMPARE(tracks.takes(0)[0].toMap().value("seconds").toDouble(), 2.0 * kBlock / 48000.0);
        QCOMPARE(QDir(recordings).entryList({"audio*"}, QDir::Dirs).size(), 1);
    }

    void registryEntries()
    {
        QTemporaryDir dir;
        AudioTracksController tracks(dir.path());
        const auto entries = audioTrackEntries(tracks);
        QCOMPARE(entries.size(), AudioTracksController::kTracks);
        QCOMPARE(entries[0].id, QStringLiteral("audio1"));
        QCOMPARE(entries[7].name, QStringLiteral("Audio 8"));
        QVERIFY(!entries[0].inRackByDefault);
        QVERIFY(!entries[0].playsNotes);
        auto instance = entries[4].create({});
        QCOMPARE(&instance->instrument(), static_cast<krate::IInstrument *>(&tracks.engine(4)));
        tracks.setDevice(4, 12);
        QCOMPARE(tracks.trackOf(12), 4);
        QCOMPARE(tracks.trackOf(3), -1);
    }
};

QTEST_GUILESS_MAIN(TestAudioTracks)
#include "tst_audiotracks.moc"
