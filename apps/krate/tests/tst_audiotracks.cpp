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
            QCOMPARE(tracks.selectedTake(2), 0);
            const QVariantMap take = tracks.takes(2)[0].toMap();
            QCOMPARE(take.value("seconds").toDouble(), 100.0 * kBlock / 48000.0);
            QVERIFY(take.value("onLane").toBool()); // on the lane where it was recorded
            QCOMPARE(tracks.clips(2).size(), 1);
            QCOMPARE(tracks.clips(2)[0].toMap().value("startBeat").toDouble(), 0.0);
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
        QCOMPARE(track3["clips"].toArray()[0].toObject()["file"].toString(), file);
        QCOMPARE(track3["takes"].toArray()[0].toObject()["offset"].toInt(), 64);
        QCOMPARE(track3["arm"].toDouble(), 0.0);

        // Reopened: the take is back on the lane; off it, on again, then
        // deleted with its file and its clip.
        AudioTracksController tracks(recordings);
        QCOMPARE(tracks.takes(2).size(), 1);
        QVERIFY(tracks.onLane(2, 0));
        waitFor([&] { return tracks.clips(2).value(0).toMap().value("loaded").toBool(); });
        QVERIFY(!tracks.peaks(2).isEmpty());
        tracks.setOnLane(2, 0, false);
        QVERIFY(tracks.clips(2).isEmpty());
        QVERIFY(!tracks.peaks(2).isEmpty()); // still selected: its waveform stays
        tracks.setOnLane(2, 0, true);
        QCOMPARE(tracks.clips(2).size(), 1);
        tracks.deleteTake(2, 0);
        QVERIFY(tracks.takes(2).isEmpty());
        QVERIFY(tracks.clips(2).isEmpty());
        QVERIFY(!QFileInfo::exists(QDir(recordings).filePath("audio3/" + file)));
    }

    // The take being recorded, for the arranger: from where it started, as
    // long as what's recorded so far, its peaks; gone once the take is on the
    // lane with its own waveform.
    void drawsTheTakeAsItRecords()
    {
        QTemporaryDir dir;
        AudioTracksController tracks(dir.filePath("recordings"));
        krate::AudioTrack &engine = tracks.engine(1);
        engine.prepare(48000.0, kBlock);
        Feed feed{engine};
        feed.beat = 8.0;
        QVERIFY(tracks.liveTake(1).isEmpty());
        tracks.setParam(1, krate::AudioTrack::Arm, 1.0);
        tracks.setRecording(true);
        feed.block(true, true);
        for (int k = 0; k < 187; ++k) // a second (188 blocks)
            feed.block(true);
        waitFor([&] { return !tracks.liveTake(1).isEmpty(); });
        QVariantMap live = tracks.liveTake(1);
        QCOMPARE(live.value("startBeat").toDouble(), 8.0);
        QCOMPARE(live.value("seconds").toDouble(), 188.0 * kBlock / 48000.0);
        QVariantList peaks = live.value("peaks").toList();
        QCOMPARE(peaks.size(), 21); // 20 a second, the last one begun
        QCOMPARE(peaks.first().toDouble(), 0.5);
        QCOMPARE(peaks.last().toDouble(), 0.5);

        // Louder for the next half second: only the new part is.
        std::fill(feed.a.begin(), feed.a.end(), 0.9f);
        for (int k = 0; k < 94; ++k)
            feed.block(true);
        waitFor([&] { return tracks.liveTake(1).value("peaks").toList().size() > 21; });
        peaks = tracks.liveTake(1).value("peaks").toList();
        QCOMPARE(peaks.size(), 31);
        QCOMPARE(peaks[5].toDouble(), 0.5);
        QVERIFY(std::fabs(peaks[25].toDouble() - 0.9) < 1e-6);

        feed.block(false);
        waitFor([&] { return tracks.liveTake(1).isEmpty(); });
        QVERIFY(tracks.liveTake(1).isEmpty());
        QCOMPARE(tracks.clips(1).size(), 1);
        QVERIFY(tracks.clips(1)[0].toMap().value("loaded").toBool());

        // As a saved sequence keeps the lanes: cleared (a new one), then back.
        const QJsonArray kept = tracks.lanesJson();
        tracks.setLanesJson({});
        QVERIFY(tracks.clips(1).isEmpty());
        QCOMPARE(tracks.takes(1).size(), 1); // the take stays
        tracks.setLanesJson(kept);
        QCOMPARE(tracks.clips(1).size(), 1);
        QCOMPARE(tracks.clips(1)[0].toMap().value("startBeat").toDouble(), 8.0);
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

    // Two takes on one lane, the later on top; a lane from before (one
    // "active" take) comes back as one clip.
    void lanesAndOldFiles()
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
            feed.block(true, true); // take 1: beat 0, 0.5
            for (int k = 0; k < 7; ++k)
                feed.block(true);
            feed.block(false);
            waitFor([&] { return tracks.takes(0).size() == 1; });
            feed.a.assign(kBlock, 0.125f);
            feed.beat = 0.0;
            feed.block(true, true); // take 2: beat 0 again, 0.125 (an empty block lets the writer finish first)
            for (int k = 0; k < 7 && !engine.recording(); ++k) {
                QTest::qWait(20);
                feed.beat = 0.0;
                feed.block(true, true);
            }
            for (int k = 0; k < 7; ++k)
                feed.block(true);
            feed.block(false);
            waitFor([&] { return tracks.takes(0).size() == 2; });
            QCOMPARE(tracks.clips(0).size(), 2);
            waitFor([&] {
                const auto c = tracks.clips(0);
                return c[0].toMap().value("loaded").toBool() && c[1].toMap().value("loaded").toBool();
            });
            tracks.setParam(0, krate::AudioTrack::Arm, 0.0);
            feed.beat = 0.0;
            feed.block(true, true);
            QCOMPARE(feed.l[10], 0.125f); // the later take, on top
            tracks.setOnLane(0, 1, false);
            feed.beat = 0.0;
            feed.block(true, true);
            QCOMPARE(feed.l[10], 0.5f);
            feed.block(false);
        }
        // An old tracks.json: its active take becomes the lane's one clip.
        QFile json(QDir(recordings).filePath("tracks.json"));
        QVERIFY(json.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(json.readAll()).object();
        json.close();
        QJsonArray list = root["tracks"].toArray();
        QJsonObject first = list[0].toObject();
        const QString active = first["takes"].toArray()[1].toObject()["file"].toString();
        first.remove("clips");
        first["active"] = active;
        list[0] = first;
        root["tracks"] = list;
        QVERIFY(json.open(QIODevice::WriteOnly | QIODevice::Truncate));
        json.write(QJsonDocument(root).toJson());
        json.close();
        AudioTracksController tracks(recordings);
        QCOMPARE(tracks.clips(0).size(), 1);
        QVERIFY(tracks.onLane(0, 1));
        QVERIFY(!tracks.onLane(0, 0));
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
