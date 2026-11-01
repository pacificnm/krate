#include <QtTest>

#include <QDir>
#include <QTemporaryDir>

#include <cmath>

#include "dsp/Wav.h"
#include "rec/AudioRing.h"
#include "rec/AudioTrack.h"
#include "rec/Recorder.h"
#include "rec/WavWriter.h"

using namespace krate;

// Recording (docs/DAW.md, D2): the ring buffer, the WAV writer, an audio
// track recording through the disk thread and playing its take back.
class TestRecording : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 256;

    // A recognisable input: a different value every sample, left and right apart.
    static float inL(std::uint64_t i) { return float(std::sin(double(i) * 0.01)) * 0.5f; }
    static float inR(std::uint64_t i) { return float(std::cos(double(i) * 0.013)) * 0.25f; }

    struct Rig
    {
        AudioTrack track;
        std::uint64_t played = 0; // input samples fed so far
        double beat = 0.0;
        std::vector<float> l = std::vector<float>(kBlock), r = std::vector<float>(kBlock);
        std::vector<float> a = std::vector<float>(kBlock), b = std::vector<float>(kBlock);
        Rig() { track.prepare(kRate, kBlock); }
        // One block, the transport playing (or not) at 120 BPM.
        void block(bool playing, bool restarted = false)
        {
            for (std::uint32_t i = 0; i < kBlock; ++i) {
                a[i] = inL(played + i);
                b[i] = inR(played + i);
            }
            const float *inputs[2] = {a.data(), b.data()};
            float *outputs[2] = {l.data(), r.data()};
            TransportState t;
            t.playing = playing;
            t.restarted = restarted;
            t.bpm = 120.0;
            t.beatAtBlockStart = playing ? beat : 0.0;
            track.process(AudioBlock{outputs, kBlock, nullptr, 0, &t, inputs});
            played += kBlock;
            if (playing)
                beat += kBlock * 2.0 / kRate; // 120 BPM: 2 beats a second
        }
    };

private slots:
    void ringWrapsAndCountsOverruns()
    {
        AudioRing ring(10);
        float l[8], r[8], a[8], b[8];
        for (int i = 0; i < 8; ++i) {
            l[i] = float(i);
            r[i] = float(-i);
        }
        QCOMPARE(ring.write(l, r, 8), std::size_t(8));
        QCOMPARE(ring.read(a, b, 5), std::size_t(5));
        QCOMPARE(a[4], 4.0f);
        QCOMPARE(ring.write(l, r, 8), std::size_t(7)); // 3 left + 7 free of 10
        QCOMPARE(ring.takeOverruns(), std::uint64_t(1));
        QCOMPARE(ring.available(), std::size_t(10));
        QCOMPARE(ring.read(a, b, 8), std::size_t(8));
        QCOMPARE(a[0], 5.0f);
        QCOMPARE(a[3], 0.0f); // the second write, after the wrap
        QCOMPARE(b[4], -1.0f);
    }

    // 24-bit, read back by the shared reader within a 24-bit step.
    void writesTwentyFourBit()
    {
        QTemporaryDir dir;
        const std::string path = dir.filePath("t.wav").toStdString();
        std::vector<float> l(1000), r(1000);
        for (std::size_t i = 0; i < l.size(); ++i) {
            l[i] = inL(i);
            r[i] = inR(i);
        }
        l[10] = 1.5f; // clipped
        WavWriter w;
        QVERIFY(w.open(path, 44100, 2));
        QVERIFY(w.write(l.data(), r.data(), 600));
        QVERIFY(w.write(l.data() + 600, r.data() + 600, 400));
        QCOMPARE(w.frames(), std::uint64_t(1000));
        QVERIFY(w.close());
        WavData data;
        QVERIFY(readWavFile(path, &data));
        QCOMPARE(data.rate, 44100);
        QCOMPARE(data.channels, 2);
        QCOMPARE(data.left.size(), std::size_t(1000));
        for (std::size_t i = 0; i < 1000; ++i) {
            QVERIFY(std::fabs(data.left[i] - std::clamp(l[i], -1.0f, 1.0f)) < 2.0f / 8388607.0f);
            QVERIFY(std::fabs(data.right[i] - r[i]) < 2.0f / 8388607.0f);
        }
    }

    // Armed, REC on, play: a take, written by the disk thread, exactly the
    // input; loaded back it plays in time, its offset (the round trip) skipped.
    void recordsAndPlaysBack()
    {
        QTemporaryDir dir;
        Rig rig;
        Recorder recorder({&rig.track}, [&](int) { return dir.filePath("take.wav").toStdString(); });
        rig.track.setParam(AudioTrack::Arm, 1.0f);
        rig.track.setParam(AudioTrack::Monitor, 0.0f);
        rig.track.setRecordEnabled(true);
        rig.block(false); // stopped: nothing
        QVERIFY(!rig.track.recording());
        const std::uint64_t first = rig.played; // what the take starts with
        rig.block(true, true);
        QVERIFY(rig.track.recording());
        for (int k = 0; k < 40; ++k) {
            rig.block(true);
            if (k % 8 == 0)
                recorder.poll();
        }
        rig.block(false); // stop
        QVERIFY(!rig.track.recording());
        recorder.poll();
        const auto finished = recorder.takeFinished();
        QCOMPARE(finished.size(), std::size_t(1));
        QVERIFY(finished[0].ok);
        QCOMPARE(finished[0].frames, std::uint64_t(41 * kBlock));
        QCOMPARE(finished[0].startBeat, 0.0);
        QCOMPARE(finished[0].channels, 2);

        WavData data;
        QVERIFY(readWavFile(finished[0].path, &data));
        QCOMPARE(data.left.size(), std::size_t(41 * kBlock));
        for (std::size_t i = 0; i < data.left.size(); i += 97) {
            QVERIFY(std::fabs(data.left[i] - inL(first + i)) < 1e-6f);
            QVERIFY(std::fabs(data.right[i] - inR(first + i)) < 1e-6f);
        }

        // Played back: disarmed, the transport from the top; offset 100 frames.
        Take take;
        take.left = data.left;
        take.right = data.right;
        take.offset = 100;
        QVERIFY(rig.track.setTake(&take) == nullptr);
        rig.track.setParam(AudioTrack::Arm, 0.0f);
        rig.beat = 0.0;
        rig.block(true, true);
        for (std::uint32_t i = 0; i < kBlock; i += 31)
            QVERIFY(std::fabs(rig.l[i] - data.left[100 + i]) < 1e-5f);
        rig.block(true);
        QVERIFY(std::fabs(rig.r[5] - data.right[100 + kBlock + 5]) < 1e-5f);
        rig.block(false);
        QCOMPARE(rig.l[0], 0.0f); // stopped: silent
        rig.track.setTake(nullptr);
    }

    // Monitoring: the input heard (On, or Auto while armed); an armed track
    // recording doesn't play its take; In 1 records a mono file.
    void monitorAndMono()
    {
        QTemporaryDir dir;
        Rig rig;
        Recorder recorder({&rig.track}, [&](int) { return dir.filePath("mono.wav").toStdString(); });
        Take take;
        take.left.assign(kBlock * 4, 0.75f);
        rig.track.setTake(&take);
        rig.block(true, true);
        QCOMPARE(rig.l[3], 0.75f); // not armed, Auto: the take only
        rig.track.setParam(AudioTrack::Monitor, 1.0f); // On
        rig.block(true);
        QVERIFY(std::fabs(rig.l[3] - (0.75f + rig.a[3])) < 1e-6f);
        rig.track.setParam(AudioTrack::Monitor, 0.5f); // Auto
        rig.track.setParam(AudioTrack::Arm, 1.0f);
        rig.track.setParam(AudioTrack::Input, 0.5f); // In 1
        rig.track.setRecordEnabled(true);
        rig.block(true);
        QCOMPARE(rig.l[3], rig.a[3]); // the input (In 1 on both sides), not the take
        QCOMPARE(rig.r[3], rig.a[3]);
        rig.block(false);
        recorder.poll();
        const auto finished = recorder.takeFinished();
        QCOMPARE(finished.size(), std::size_t(1));
        QCOMPARE(finished[0].channels, 1);
        WavData data;
        QVERIFY(readWavFile(finished[0].path, &data));
        QCOMPARE(data.channels, 1);
        rig.track.setTake(nullptr);
    }

    // A new take waits until the last one is written.
    void oneTakeAtATime()
    {
        QTemporaryDir dir;
        Rig rig;
        int n = 0;
        Recorder recorder({&rig.track}, [&](int) { return dir.filePath(QStringLiteral("t%1.wav").arg(n++)).toStdString(); });
        rig.track.setParam(AudioTrack::Arm, 1.0f);
        rig.track.setRecordEnabled(true);
        rig.block(true, true);
        rig.block(false);
        rig.block(true, true); // again at once: the first isn't written yet
        QVERIFY(!rig.track.recording());
        recorder.poll(); // the first take written
        QCOMPARE(recorder.takeFinished().size(), std::size_t(1));
        rig.block(true);
        QVERIFY(rig.track.recording()); // now the second begins
        rig.block(false);
        recorder.poll();
        const auto second = recorder.takeFinished();
        QCOMPARE(second.size(), std::size_t(1));
        QCOMPARE(second[0].frames, std::uint64_t(kBlock));
        QVERIFY(second[0].startBeat > 0.0);
    }

    // The disk thread itself, running while recording.
    void diskThread()
    {
        QTemporaryDir dir;
        Rig rig;
        Recorder recorder({&rig.track}, [&](int) { return dir.filePath("thread.wav").toStdString(); });
        recorder.start();
        rig.track.setParam(AudioTrack::Arm, 1.0f);
        rig.track.setRecordEnabled(true);
        rig.block(true, true);
        for (int k = 0; k < 200; ++k)
            rig.block(true);
        rig.block(false);
        std::vector<Recorder::Finished> finished;
        // (Not QTRY_VERIFY: it asks once more after succeeding, and the list is handed over once.)
        for (int k = 0; k < 250 && finished.empty(); ++k) {
            QTest::qWait(20);
            finished = recorder.takeFinished();
        }
        QCOMPARE(finished.size(), std::size_t(1));
        QCOMPARE(finished[0].frames, std::uint64_t(201 * kBlock));
        QCOMPARE(rig.track.ring().takeOverruns(), std::uint64_t(0));
        recorder.stop();
    }
};

QTEST_GUILESS_MAIN(TestRecording)
#include "tst_recording.moc"
