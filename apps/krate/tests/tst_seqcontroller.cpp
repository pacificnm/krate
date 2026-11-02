#include <QtTest>

#include <QTemporaryDir>

#include "ArrangeController.h"
#include "RackRunControl.h"
#include "SequencerController.h"
#include "core/Transport.h"
#include "rack/MidiCapture.h"
#include "rack/Automation.h"
#include "rec/WavWriter.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include "rack/Rack.h"

using namespace krate;
using namespace krate::seq;
using namespace Qt::StringLiterals;

namespace {
std::string fmt(float v) { return std::to_string(int(std::lround(v * 100.0f))) + "%"; }
constexpr ParamDescriptor kFakeParams[] = {{"tone", "Tone", 0.5f, 0, &fmt}, {"cutoff", "Cutoff", 0.5f, 0, &fmt}};
// Two knobs, nothing else (automation tests).
class FakeInstrument final : public IInstrument
{
public:
    void prepare(double, std::uint32_t) noexcept override {}
    std::span<const ParamDescriptor> parameters() const noexcept override { return kFakeParams; }
    float param(int i) const noexcept override { return values[std::size_t(i)]; }
    void setParam(int i, float v) noexcept override { values[std::size_t(i)] = v; }
    void setParamFromHost(int i, float v) noexcept override { values[std::size_t(i)] = v; }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    void process(const AudioBlock &) noexcept override {}
    std::array<float, 2> values{0.5f, 0.5f};
};
} // namespace

// The SEQ screen's controller (docs/SEQUENCER.md): edits reach the player at
// once, targets resolve to instruments, and sequencer.json keeps it all.
class TestSeqController : public QObject
{
    Q_OBJECT

    static std::vector<NoteTarget> targets()
    {
        return {{QStringLiteral("Korrode"), QStringLiteral("korrode"), 0, 0, false},
                {QStringLiteral("Kessel"), QStringLiteral("kessel"), 1, 9, true},
                {QStringLiteral("Krimson B"), QStringLiteral("krimson:B"), 3, 3, false}};
    }

    // The first note-on the player sends within a beat of play.
    static std::optional<DeviceEvent> firstOn(Sequencer &seq)
    {
        std::array<DeviceEvent, 64> out{};
        for (std::uint64_t at = 0; at < 24000; at += 64) {
            TransportState t;
            t.playing = true;
            t.restarted = at == 0;
            t.beatAtBlockStart = double(at) / 24000.0;
            const std::uint32_t n = seq.render(t, 64, 48000.0, out.data(), std::uint32_t(out.size()));
            for (std::uint32_t i = 0; i < n; ++i)
                if ((out[i].event.data[0] & 0xF0) == 0x90)
                    return out[i];
        }
        return std::nullopt;
    }

private slots:
    // The arranger (docs/DAW.md, D3): clips placed and removed, the song
    // played in SONG mode, start / loop / punch handed to the transport (only
    // in SONG mode), all of it kept in sequencer.json.
    // Recording MIDI clips (docs/DAW.md, D3): the keyboard's notes, captured
    // as the rack hears them, become a clip on the armed lane over the bars
    // they span, replacing what was there; outside the punch range, or with
    // REC off, nothing is kept.
    // Clip editing (D4) on a MIDI lane: move (to another lane too), trim
    // either end, split, duplicate, never overlapping; undo and redo each.
    void editsMidiClips()
    {
        QTemporaryDir dir;
        Sequencer seq;
        Transport transport;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        ArrangeController arrange(c, transport, nullptr);
        const auto clipOf = [&](int track, int i) { return c.state().arrangement.lanes[std::size_t(track)][std::size_t(i)]; };
        QVERIFY(!arrange.canUndo());
        QVERIFY(arrange.placePattern(0, 0, 3, 4)); // bars 1-4
        QVERIFY(arrange.placePattern(0, 6, 1, 2)); // bars 7-8
        QVERIFY(arrange.canUndo());

        QVERIFY(!arrange.moveClip(0, 0, 0, 4));   // would overlap bars 7-8
        QVERIFY(arrange.moveClip(0, 0, 0, 2));    // bars 3-6
        QCOMPARE(clipOf(0, 0).startBar, 2);
        QVERIFY(arrange.trimClipStart(0, 0, 3));  // bars 4-6, a bar into the pattern
        QCOMPARE(clipOf(0, 0).offsetBars, 1);
        QCOMPARE(clipOf(0, 0).bars, 3);
        QVERIFY(!arrange.trimClipStart(0, 0, 1)); // not before its content's start
        QVERIFY(!arrange.trimClipEnd(0, 0, 7));   // into the next clip
        QVERIFY(arrange.trimClipEnd(0, 0, 5));    // bars 4-5
        QVERIFY(arrange.splitClip(0, 0, 4));      // bar 4 | bar 5
        QCOMPARE(arrange.lane(0).size(), 3);
        QCOMPARE(clipOf(0, 1).startBar, 4);
        QCOMPARE(clipOf(0, 1).offsetBars, 2);
        QVERIFY(arrange.duplicateClip(0, 1));     // bar 6
        QVERIFY(!arrange.duplicateClip(0, 2));    // bar 7 is taken
        QVERIFY(arrange.moveClip(0, 3, 5, 0));    // bars 7-8 to lane 6, bar 1
        QCOMPARE(arrange.lane(5).size(), 1);
        QCOMPARE(arrange.lane(0).size(), 3);

        // Undo all of it, step by step, then redo one.
        int steps = 0;
        while (arrange.canUndo()) {
            arrange.undo();
            ++steps;
        }
        QCOMPARE(steps, 8); // the refused ones left no step
        QCOMPARE(c.state().arrangement.endBar(), 0);
        arrange.redo();
        QCOMPARE(arrange.lane(0).size(), 1);
        QVERIFY(arrange.canRedo());
        arrange.placePattern(2, 0, 0); // a new edit: nothing left to redo
        QVERIFY(!arrange.canRedo());
        // A split MIDI clip's halves have their own notes: one can go alone.
        auto &song = c.arrangerState().arrangement;
        song.midi.push_back({{{0, 24, 60, 100}, {384, 24, 62, 100}}});
        song.lanes[4] = {{0, 2, -1, int(song.midi.size()) - 1}};
        c.arrangerEdited(true);
        QVERIFY(arrange.splitClip(4, 0, 1));
        arrange.removeClip(4, 0);
        QCOMPARE(c.state().arrangement.midi.size(), std::size_t(1));
        QCOMPARE(c.state().arrangement.lanes[4][0].midi, 0);
        QCOMPARE(c.state().arrangement.lanes[4][0].offsetBars, 1);
        // Another sequence: its own history.
        c.newSequence();
        QVERIFY(!arrange.canUndo());
    }

    // Audio clips (D4): trim, split, duplicate, fades, move, at the tempo;
    // undo spans audio and MIDI lanes and a recording landing.
    void editsAudioClips()
    {
        QTemporaryDir dir;
        const QString recordings = dir.filePath(QStringLiteral("recordings"));
        QDir().mkpath(recordings + QStringLiteral("/audio1"));
        { // a 2-second take, recorded from beat 4 with a 480-frame round trip
            WavWriter w;
            QVERIFY(w.open((recordings + QStringLiteral("/audio1/take-a.wav")).toStdString(), 48000, 1));
            std::vector<float> l(96000, 0.5f);
            QVERIFY(w.write(l.data(), l.data(), l.size()));
            QVERIFY(w.close());
            QJsonObject take{{"file", "take-a.wav"}, {"startBeat", 4.0}, {"offset", 480}, {"rate", 48000},
                             {"frames", 96000}, {"channels", 1}};
            QJsonArray tracks{QJsonObject{{"takes", QJsonArray{take}},
                                          {"clips", QJsonArray{QJsonObject{{"file", "take-a.wav"}, {"startBeat", 4.0}}}}}};
            QFile f(recordings + QStringLiteral("/tracks.json"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(QJsonObject{{"tracks", tracks}}).toJson());
        }
        Sequencer seq;
        Transport transport;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        AudioTracksController tracks(recordings);
        ArrangeController arrange(c, transport, &tracks);
        const auto clip = [&](int i) { return tracks.clips(0)[i].toMap(); };
        QCOMPARE(clip(0).value(u"seconds"_s).toDouble(), 95520.0 / 48000.0); // less the round trip

        // At 120 BPM a beat is 24000 frames.
        tracks.trimClipStart(0, 0, 5.0, 120.0);  // a beat off the start
        QCOMPARE(clip(0).value(u"startBeat"_s).toDouble(), 5.0);
        QCOMPARE(clip(0).value(u"seconds"_s).toDouble(), 71520.0 / 48000.0);
        tracks.trimClipStart(0, 0, 2.0, 120.0);  // back: no further than the take's start
        QCOMPARE(clip(0).value(u"startBeat"_s).toDouble(), 4.0);
        tracks.trimClipEnd(0, 0, 7.0, 120.0);    // 3 beats long
        QCOMPARE(clip(0).value(u"seconds"_s).toDouble(), 1.5);
        QCOMPARE(tracks.splitClip(0, 0, 5.0, 120.0), 1); // beat 4 | beats 5-6
        QCOMPARE(clip(0).value(u"seconds"_s).toDouble(), 0.5);
        QCOMPARE(clip(1).value(u"startBeat"_s).toDouble(), 5.0);
        QCOMPARE(clip(1).value(u"seconds"_s).toDouble(), 1.0);
        QCOMPARE(tracks.duplicateClip(0, 1, 120.0), 2);   // beats 7-8
        QCOMPARE(clip(2).value(u"startBeat"_s).toDouble(), 7.0);
        tracks.setClipFades(0, 2, 0.25, 2.0);   // the fade out is cut to what's left
        QCOMPARE(clip(2).value(u"fadeIn"_s).toDouble(), 0.25);
        QCOMPARE(clip(2).value(u"fadeOut"_s).toDouble(), 0.75);
        QCOMPARE(tracks.moveClip(0, 0, 12.0), 2); // on top
        QCOMPARE(clip(2).value(u"startBeat"_s).toDouble(), 12.0);
        QVERIFY(arrange.placePattern(1, 0, 0));    // a MIDI edit in between

        // Undo: the MIDI clip, then the audio edits, back to the one clip.
        arrange.undo();
        QVERIFY(arrange.lane(1).isEmpty());
        QCOMPARE(tracks.clips(0).size(), 3);
        for (int k = 0; k < 7; ++k)
            arrange.undo();
        QVERIFY(!arrange.canUndo());
        QCOMPARE(tracks.clips(0).size(), 1);
        QCOMPARE(clip(0).value(u"seconds"_s).toDouble(), 95520.0 / 48000.0);
        arrange.redo();
        QCOMPARE(clip(0).value(u"startBeat"_s).toDouble(), 5.0);
        // The edits are kept in tracks.json.
        tracks.save();
        AudioTracksController again(dir.filePath(QStringLiteral("recordings")));
        QCOMPARE(again.clips(0)[0].toMap().value(u"startBeat"_s).toDouble(), 5.0);
        QCOMPARE(again.clips(0)[0].toMap().value(u"seconds"_s).toDouble(), 71520.0 / 48000.0);
    }

    // Automation (D5): lanes and points edited (undoable) and handed to the
    // player; knobs turned while AUTO records become a lane, or replace a
    // stretch of one; the lane playing isn't taken for someone turning it.
    void automation()
    {
        QTemporaryDir dir;
        Sequencer seq;
        Transport transport;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        ArrangeController arrange(c, transport, nullptr);
        AutomationPlayer player;
        FakeInstrument a, b;
        arrange.setAutomationTargets(&player, {u"a"_s, u"b"_s}, {u"Synth A"_s, u"Synth B"_s}, {&a, &b});
        QCOMPARE(arrange.automatableDevices().size(), 2);
        QCOMPARE(arrange.paramLabels(1), (QStringList{u"Tone"_s, u"Cutoff"_s}));

        // Lanes and points.
        b.values[1] = 0.25f;
        QCOMPARE(arrange.addAutomationLane(1, 1), 0);
        QCOMPARE(arrange.addAutomationLane(1, 1), 0); // there already
        QCOMPARE(arrange.addPoint(0, 8.0, 1.0), 1);
        QCOMPARE(arrange.addPoint(0, 4.0, 2.0), 1);   // clamped to 1, in beat order
        QCOMPARE(arrange.movePoint(0, 1, 12.0, 0.5), 2);
        QCOMPARE(arrange.valueText(0, 0.5), u"50%"_s);
        const QVariantMap lane = arrange.automationLanes()[0].toMap();
        QCOMPARE(lane.value(u"deviceName"_s).toString(), u"Synth B"_s);
        QCOMPARE(lane.value(u"label"_s).toString(), u"Cutoff"_s);
        QCOMPARE(lane.value(u"points"_s).toList().size(), 3);
        QCOMPARE(lane.value(u"points"_s).toList()[0].toMap().value(u"value"_s).toDouble(), 0.25); // where the knob was
        arrange.removePoint(0, 2);
        arrange.undo();
        QCOMPARE(arrange.automationLanes()[0].toMap().value(u"points"_s).toList().size(), 3);

        // Played in SONG mode: at beat 6, halfway from 0.25 (beat 0)... to 1 (beat 8).
        arrange.setSongMode(true);
        TransportState t;
        t.playing = true;
        t.bpm = 120.0;
        t.beatAtBlockStart = 4.0;
        player.apply(t);
        QCOMPARE(b.values[1], 0.625f);

        // Recording: knob A's tone, turned at beats 4 and 6 (a lane of its own).
        arrange.setAutoWrite(true);
        transport.setPlaying(true);
        const auto at = [&](double beat) { // the transport's beat, as the host leaves it
            transport.advance(std::uint32_t(beat * 24000.0) - std::uint32_t(transport.beatPosition() * 24000.0), 48000.0);
            transport.advance(0, 48000.0);
        };
        transport.advance(0, 48000.0);
        arrange.poll();
        QVERIFY(arrange.writing());
        at(4.0);
        a.values[0] = 0.3f;
        QTest::qWait(40);
        at(6.0);
        a.values[0] = 0.6f;
        // Lane 0 playing knob B isn't someone turning it.
        t.beatAtBlockStart = 6.0;
        player.apply(t);
        QTest::qWait(40);
        // Knob B turned by hand at beat 7: recorded over its lane from there.
        at(7.0);
        b.values[1] = 0.9f;
        QTest::qWait(40);
        transport.setPlaying(false);
        arrange.poll();
        QVERIFY(!arrange.writing());

        const QVariantList lanes = arrange.automationLanes();
        QCOMPARE(lanes.size(), 2);
        const QVariantList tone = lanes[1].toMap().value(u"points"_s).toList();
        QCOMPARE(lanes[1].toMap().value(u"label"_s).toString(), u"Tone"_s);
        QCOMPARE(tone.size(), 2);
        QCOMPARE(tone[0].toMap().value(u"beat"_s).toDouble(), 4.0);
        QVERIFY(std::fabs(tone[0].toMap().value(u"value"_s).toDouble() - 0.3) < 1e-6);
        QCOMPARE(tone[1].toMap().value(u"beat"_s).toDouble(), 6.0);
        // Knob B's lane: as before up to beat 7, 0.9 there, back to the old line after.
        const auto &cutoff = c.state().automation[0].points;
        const krate::AutomationLane line{-1, -1, cutoff};
        QVERIFY(std::fabs(line.valueAt(2.0) - 0.4375f) < 1e-4f);
        QCOMPARE(line.valueAt(7.0), 0.9f);
        QVERIFY(std::fabs(line.valueAt(7.0 + 1.0 / 96.0) - float(0.25 + 0.75 * (7.0 + 1.0 / 96.0) / 8.0)) < 1e-5f);
        QVERIFY(line.valueAt(8.0) == 1.0f); // the old line resumes
        arrange.undo(); // all of the recording in one step
        QCOMPARE(arrange.automationLanes().size(), 1);
    }

    void recordsMidiClips()
    {
        QTemporaryDir dir;
        Sequencer seq;
        Transport transport;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        ArrangeController arrange(c, transport, nullptr);
        MidiCapture capture;
        arrange.setMidiCapture(&capture);
        QVERIFY(arrange.placePattern(1, 1, 0, 2)); // bars 2-3: replaced by the take
        QVERIFY(arrange.placePattern(1, 6, 0, 1)); // bar 7: stays
        arrange.setSongMode(true);
        arrange.setArmed(1, true);
        arrange.setRecording(true);
        transport.setPlaying(true);
        const auto at = [&](double beat, std::uint8_t status, std::uint8_t key, std::uint8_t velocity) {
            TransportState t;
            t.playing = true;
            t.bpm = 120.0;
            t.beatAtBlockStart = beat;
            const MidiEvent e{24, 3, {status, key, velocity}};
            capture.capture(t, &e, 1, 48000.0); // frame 24: 0.001 beats in
        };
        at(4.0, 0x90, 60, 90);
        at(5.0, 0x80, 60, 0);
        arrange.poll();
        QCOMPARE(arrange.takeNotes(), 1);
        at(6.0, 0x90, 64, 70); // still held when play stops
        at(6.5, 0x90, 67, 0);  // a note-on at velocity 0 is a release (of nothing here)
        arrange.poll();
        transport.setPlaying(false);
        arrange.poll();
        const QVariantList lane = arrange.lane(1);
        QCOMPARE(lane.size(), 2);
        const QVariantMap clip = lane[0].toMap();
        QCOMPARE(clip.value(u"startBar"_s).toInt(), 1);
        QCOMPARE(clip.value(u"bars"_s).toInt(), 1);
        QCOMPARE(clip.value(u"midi"_s).toInt(), 0);
        QCOMPARE(lane[1].toMap().value(u"startBar"_s).toInt(), 6);
        const MidiClip &notes = c.state().arrangement.midi[0];
        QCOMPARE(notes.notes.size(), std::size_t(2));
        QCOMPARE(notes.notes[0], (ClipNote{0, 96, 60, 90}));
        QCOMPARE(notes.notes[1].start, std::uint32_t(192));
        QCOMPARE(notes.notes[1].pitch, std::uint8_t(64));

        // Punch on bar 3 only: the note on bar 2 isn't kept. REC off: nothing.
        arrange.setPunch(2, 3);
        arrange.setPunchOn(true);
        transport.setPlaying(true);
        at(4.0, 0x90, 50, 100);
        at(4.5, 0x80, 50, 0);
        at(8.0, 0x90, 52, 100);
        at(9.0, 0x80, 52, 0);
        arrange.poll();
        transport.setPlaying(false);
        arrange.poll();
        QCOMPARE(c.state().arrangement.midi.size(), std::size_t(2)); // a new clip on bar 3, beside bar 2's
        QCOMPARE(c.state().arrangement.midi[1].notes.size(), std::size_t(1));
        QCOMPARE(c.state().arrangement.midi[1].notes[0].pitch, std::uint8_t(52));
        QCOMPARE(arrange.lane(1)[1].toMap().value(u"startBar"_s).toInt(), 2);
        arrange.setRecording(false);
        transport.setPlaying(true);
        at(20.0, 0x90, 40, 100);
        at(21.0, 0x80, 40, 0);
        arrange.poll();
        transport.setPlaying(false);
        arrange.poll();
        QCOMPARE(arrange.lane(1).size(), 3);
    }

    void arranger()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sequencer.json"));
        {
            Sequencer seq;
            Transport transport;
            SequencerController c(seq, targets(), path);
            c.load();
            ArrangeController arrange(c, transport, nullptr);
            c.setTarget(0, 1); // Korrode
            c.setTrack(0);
            c.setPattern(2);
            c.setLength(2 * 384); // two bars
            QCOMPARE(c.addNote(0, 50, 24, 100), 0);
            QCOMPARE(arrange.patternBars(2), 2);

            QVERIFY(arrange.placePattern(0, 4, 2));  // bars 5-6
            QVERIFY(arrange.placePattern(0, 1, 2, 9)); // shortened: ends where the next begins
            QVERIFY(!arrange.placePattern(0, 5, 0)); // taken
            QCOMPARE(arrange.lane(0).size(), 2);
            QCOMPARE(arrange.lane(0)[0].toMap().value(u"bars"_s).toInt(), 3);
            QCOMPARE(arrange.clipAt(0, 3), 0);
            QCOMPARE(arrange.clipAt(0, 6), -1);
            QCOMPARE(arrange.endBar(), 6);
            QCOMPARE(arrange.clipNotes(0, 0).size(), 2); // the 2-bar pattern twice in 3 bars (the second cut)
            // Trimmed a bar in (D4): the pattern's first note is gone, its next pass's is a bar in.
            QVERIFY(arrange.trimClipStart(0, 0, 2));
            const QVariantList trimmed = arrange.clipNotes(0, 0);
            QCOMPARE(trimmed.size(), 1);
            QCOMPARE(trimmed[0].toMap().value(u"x"_s).toDouble(), 4.0);
            arrange.undo();

            // Pattern mode: the transport from the top, no loop.
            arrange.setStartBar(4);
            arrange.setLoop(6, 4);
            arrange.setLoopOn(true);
            QCOMPARE(transport.startBeat(), 0.0);
            QVERIFY(!transport.looping());
            // SONG: the arrangement plays from bar 5, the loop goes to the transport.
            arrange.setSongMode(true);
            QVERIFY(seq.songMode());
            QCOMPARE(transport.startBeat(), 16.0);
            QVERIFY(transport.looping());
            QCOMPARE(transport.loopFromBar(), 4);
            QCOMPARE(transport.loopToBar(), 6);
            std::array<DeviceEvent, 64> out{};
            TransportState t;
            t.playing = true;
            t.restarted = true;
            t.beatAtBlockStart = 16.0;
            QCOMPARE(seq.render(t, 64, 48000.0, out.data(), std::uint32_t(out.size())), std::uint32_t(1));
            QCOMPARE(out[0].event.data[1], std::uint8_t(50));
            arrange.removeClip(0, 0);
            QCOMPARE(arrange.lane(0).size(), 1);
            c.flush();
        }
        // Back from the file.
        Sequencer seq;
        Transport transport;
        SequencerController c(seq, targets(), path);
        c.load();
        ArrangeController arrange(c, transport, nullptr);
        QVERIFY(arrange.songMode());
        QCOMPARE(arrange.startBar(), 4);
        QCOMPARE(arrange.lane(0).size(), 1);
        QCOMPARE(arrange.lane(0)[0].toMap().value(u"startBar"_s).toInt(), 4);
        QCOMPARE(transport.startBeat(), 16.0);
        QVERIFY(transport.looping());
    }

    void editsPlayAndPersist()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sequencer.json"));
        {
            Sequencer seq;
            SequencerController c(seq, targets(), path);
            c.load();
            QCOMPARE(c.targetChoices(), (QStringList{"None", "Korrode", "Kessel", "Krimson B"}));
            c.setTrack(2);
            c.setTarget(2, 3); // Krimson B
            QCOMPARE(c.addNote(48, 64, 24, 90), 0);
            c.setTarget(5, 2); // Kessel
            QVERIFY(c.tracks()[5].toMap().value(u"drums"_s).toBool());

            const auto on = firstOn(seq);
            QVERIFY(on);
            QCOMPARE(on->device, 3);
            QCOMPARE(on->event.data[0], std::uint8_t(0x93));
            QCOMPARE(on->event.data[1], std::uint8_t(64));
            QCOMPARE(on->event.frame, std::uint32_t(12000 % 64)); // step 3 at 120 BPM: frame 12000

            c.moveNote(0, 96, 67);
            c.resizeNote(0, 48);
            c.setVelocity(0, 200); // clamped
            const QVariantMap note = c.notes()[0].toMap();
            QCOMPARE(note.value(u"start"_s).toInt(), 96);
            QCOMPARE(note.value(u"pitch"_s).toInt(), 67);
            QCOMPARE(note.value(u"length"_s).toInt(), 48);
            QCOMPARE(note.value(u"velocity"_s).toInt(), 127);

            c.setGrid(2);       // 1/16T
            c.setLength(1000);  // rounded to the grid: 62.5 steps -> 63 x 16 = 1008
            QCOMPARE(c.length(), 1008);
            c.setLength(99999); // at most 4 bars
            QCOMPARE(c.length(), kMaxTicks);
            c.setMute(2, true);
            c.setPattern(4);
            QCOMPARE(c.notes().size(), 0);
            c.setPattern(0);
            c.copyPattern();
            c.setPattern(7);
            c.pastePattern();
            QCOMPARE(c.notes().size(), 1);
            QVERIFY(c.patternsUsed()[7].toBool());
        } // the destructor writes what's waiting

        Sequencer seq;
        SequencerController c(seq, targets(), path);
        c.load();
        QCOMPARE(c.pattern(), 7);
        QCOMPARE(c.tracks()[2].toMap().value(u"choice"_s).toInt(), 3);
        QVERIFY(c.tracks()[2].toMap().value(u"mute"_s).toBool());
        QCOMPARE(c.state().bank->patterns[0].length, std::uint16_t(kMaxTicks));
        QCOMPARE(c.state().bank->patterns[7].tracks[2].count, std::uint16_t(1));
        c.setTrack(2);
        c.deleteNote(0);
        QCOMPARE(c.notes().size(), 0);
    }

    // Bars from a song: each chunk replaces one track's notes in a pattern (the
    // other tracks stay), sets the pattern's length and grid, and stops at pattern 16.
    void importChunks()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        c.setPattern(14);
        c.setTrack(1);
        c.addNote(0, 40, 24, 100);  // track 1: stays
        c.setTrack(0);
        c.addNote(96, 30, 24, 100); // track 0: replaced
        QCOMPARE(c.patternsWithNotes(14, 3, 0), (QList<int>{14}));

        std::vector<songimport::Chunk> chunks(3);
        for (auto &chunk : chunks) {
            chunk.length = 8 * kTicksPerBeat;
            chunk.grid = 2;
            chunk.notes = {{0, 16, 36, 110}, {48, 16, 38, 90}};
        }
        QCOMPARE(c.importChunks(14, 0, chunks, 2), 2); // patterns 15 and 16; the third doesn't fit
        const Pattern &p = c.state().bank->patterns[14];
        QCOMPARE(p.length, std::uint16_t(8 * kTicksPerBeat));
        QCOMPARE(p.grid, std::uint8_t(2));
        QCOMPARE(p.tracks[0].count, std::uint16_t(2));
        QCOMPARE(p.tracks[0].notes[0].pitch, std::uint8_t(36));
        QCOMPARE(p.tracks[1].count, std::uint16_t(1));
        QCOMPARE(c.state().bank->patterns[15].tracks[0].count, std::uint16_t(2));
        QCOMPARE(c.tracks()[0].toMap().value(u"choice"_s).toInt(), 2); // Kessel
        QCOMPARE(c.pattern(), 14);
        QCOMPARE(c.track(), 0);
    }

    // The chain: add (at the end or after a step), repeats clamped, remove; the
    // pattern buttons wrap.
    void chainEdits()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        c.addToChain(2);
        c.addToChain(5);
        c.addToChain(9, 0); // after the first
        c.setChainRepeats(1, 99);
        const auto steps = c.chain();
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps[0].toMap().value(u"pattern"_s).toInt(), 2);
        QCOMPARE(steps[1].toMap().value(u"pattern"_s).toInt(), 9);
        QCOMPARE(steps[1].toMap().value(u"repeats"_s).toInt(), kMaxRepeats);
        QCOMPARE(steps[2].toMap().value(u"pattern"_s).toInt(), 5);
        c.removeChainStep(0);
        QCOMPARE(c.chain().size(), 2);
        c.setChainMode(true);
        QVERIFY(seq.chainMode());
        // One play mode at a time; the range stays in order.
        c.setPlayMode(1);
        QVERIFY(seq.rangeMode() && !seq.chainMode());
        QVERIFY(!c.chainMode());
        c.setRangeTo(5);
        c.setRangeFrom(8); // past TO: TO follows
        QCOMPARE(c.rangeFrom(), 8);
        QCOMPARE(c.rangeTo(), 8);
        c.setRangeTo(2);   // before FROM: FROM follows
        QCOMPARE(c.rangeFrom(), 2);
        c.setPlayMode(2);
        QVERIFY(seq.chainMode() && !seq.rangeMode());
        c.setPattern(0);
        c.stepPattern(-1);
        QCOMPARE(c.pattern(), kPatterns - 1);
        c.stepPattern(1);
        QCOMPARE(c.pattern(), 0);
        c.clearChain();
        QCOMPARE(c.chain().size(), 0);
    }

    // RUN works like an instrument's RUN / STOP: from silence it starts the
    // rack; pressed again it stops it; off while the rack plays, it joins.
    void runStartsAndStops()
    {
        QTemporaryDir dir;
        krate::Rack rack(64);
        krate::Transport transport;
        RackRunControl run(rack, transport, {});
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.setRunControl(&run);
        c.load();

        QVERIFY(!transport.playing());
        c.toggleRun();
        QVERIFY(transport.playing());
        QVERIFY(c.running());
        c.toggleRun();
        QVERIFY(!c.running());
        QVERIFY(!transport.playing()); // nothing else was running

        transport.setPlaying(true); // the rack's PLAY, the sequencer off
        c.toggleRun();
        QVERIFY(c.running());
        QVERIFY(transport.playing());
    }

    // An instrument's own RUN from silence plays it alone: the rack sequencer
    // (whose RUN is on, as by default) is switched off; the rack's PLAY runs
    // it again with everyone; an instrument stopping leaves it playing.
    void instrumentRunsAlone()
    {
        QTemporaryDir dir;
        krate::Rack rack(64);
        krate::Transport transport;
        RackRunControl run(rack, transport, {QStringLiteral("korrode")});
        run.addRunner(QStringLiteral("korrode"));
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.setRunControl(&run);
        c.load();
        run.setRackSequencer([&c] { return c.running(); }, [&c](bool on) { c.setRunning(on); });
        QVERIFY(c.running());

        run.setRunning(QStringLiteral("korrode"), true);
        QVERIFY(transport.playing());
        QVERIFY(!c.running()); // the Seq screen's tracks don't play
        QVERIFY(!seq.runningSetting());
        run.setRunning(QStringLiteral("korrode"), false);
        QVERIFY(!transport.playing());

        run.playAll(); // the rack's PLAY
        transport.setPlaying(true);
        QVERIFY(c.running());
        QVERIFY(run.running(QStringLiteral("korrode")));
        run.setRunning(QStringLiteral("korrode"), false); // the sequencer still plays
        QVERIFY(transport.playing());
    }

    // In the arranger's SONG mode the rack's PLAY plays the song alone: the
    // instruments' own patterns stay off (an empty song plays nothing); SONG
    // switched on while everything plays stops them.
    void songPlaysAlone()
    {
        QTemporaryDir dir;
        krate::Rack rack(64);
        krate::Transport transport;
        RackRunControl run(rack, transport, {QStringLiteral("korrode"), QStringLiteral("kessel")});
        run.addRunner(QStringLiteral("korrode"));
        run.addRunner(QStringLiteral("kessel"));
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.setRunControl(&run);
        c.load();
        run.setRackSequencer([&c] { return c.running(); }, [&c](bool on) { c.setRunning(on); });
        bool song = true;
        run.setSongMode([&song] { return song; });

        run.playAll();
        transport.setPlaying(true);
        QVERIFY(c.running()); // the song's player
        QVERIFY(!run.running(QStringLiteral("korrode")));
        QVERIFY(!run.running(QStringLiteral("kessel")));
        QVERIFY(!run.anyInstrumentRunning());
        transport.setPlaying(false);

        song = false; // the patterns: everyone, as before
        run.playAll();
        transport.setPlaying(true);
        QVERIFY(run.running(QStringLiteral("korrode")));
        QVERIFY(run.running(QStringLiteral("kessel")));
        song = true; // SONG on while playing
        run.songModeChanged();
        QVERIFY(!run.running(QStringLiteral("korrode")));
        QVERIFY(!run.running(QStringLiteral("kessel")));
        QVERIFY(c.running());
        QVERIFY(transport.playing());
    }

    // A track's notes copied onto another track, in another pattern; the
    // destination keeps its instrument; the source is untouched.
    void copyAndPasteTrack()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        c.setTarget(5, 1); // Korrode
        c.setTrack(2);
        c.addNote(0, 40, 24, 100);
        c.addNote(48, 43, 48, 90);
        QVERIFY(c.trackClipboard().isEmpty());
        c.copyTrack();
        QCOMPARE(c.trackClipboard(), QStringLiteral("pattern 1, track 3"));
        c.setPattern(3);
        c.setTrack(5);
        c.addNote(96, 50, 24, 100); // replaced by the paste
        c.pasteTrack();
        QCOMPARE(c.notes().size(), 2);
        QCOMPARE(c.notes()[1].toMap().value(u"pitch"_s).toInt(), 43);
        QCOMPARE(c.notes()[1].toMap().value(u"length"_s).toInt(), 48);
        QCOMPARE(c.tracks()[5].toMap().value(u"choice"_s).toInt(), 1);
        c.setPattern(0);
        c.setTrack(2);
        QCOMPARE(c.notes().size(), 2);
    }

    // RANGE (and CHAIN): the pattern shown follows the one playing; PATTERN: it stays yours.
    void shownPatternFollowsPlay()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        for (int p = 0; p < 3; ++p) { // one beat each
            c.setPattern(p);
            c.setGrid(0);
            c.setLength(96);
        }
        c.setPattern(0);
        c.setRangeFrom(0);
        c.setRangeTo(2);
        c.setPlayMode(1);
        std::array<DeviceEvent, 64> out{};
        const auto play = [&](std::uint64_t from, std::uint64_t to) {
            for (std::uint64_t at = from; at < to; at += 64) {
                TransportState t;
                t.playing = true;
                t.restarted = at == 0;
                t.beatAtBlockStart = double(at) / 24000.0;
                seq.render(t, 64, 48000.0, out.data(), std::uint32_t(out.size()));
            }
            c.poll(); // the screen's timer
        };
        play(0, 24000 + 640); // into the second pattern
        QCOMPARE(seq.playingPattern(), 1);
        QCOMPARE(c.pattern(), 1);
        play(24000 + 640, 48000 + 640);
        QCOMPARE(c.pattern(), 2);

        c.setPlayMode(0); // PATTERN: choosing is yours again
        c.setPattern(0);
        play(48000 + 640, 72000 + 640);
        QCOMPARE(c.pattern(), 0);
    }

    // NEW: every pattern, the chain, the range and mode, the mutes cleared;
    // the tracks keep their instruments; RUN stays; no longer saved, not changed.
    void newSequence()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        c.setTarget(2, 3);
        c.setMute(2, true);
        c.setPattern(6);
        c.setTrack(2);
        c.addNote(0, 60, 24, 100);
        c.setLength(2 * 384);
        c.addToChain(6);
        c.setPlayMode(1);
        c.setRangeTo(9);
        c.setRunning(false);
        QVERIFY(c.modified());

        c.newSequence();
        QVERIFY(!c.modified());
        QCOMPARE(c.sequenceId(), 0);
        QCOMPARE(c.pattern(), 0);
        QCOMPARE(c.track(), 0);
        for (const QVariant &used : c.patternsUsed())
            QVERIFY(!used.toBool());
        QCOMPARE(c.chain().size(), 0);
        QCOMPARE(c.playMode(), 0);
        QCOMPARE(c.rangeTo(), 3);
        QCOMPARE(c.tracks()[2].toMap().value(u"choice"_s).toInt(), 3); // its instrument kept
        QVERIFY(!c.tracks()[2].toMap().value(u"mute"_s).toBool());
        QVERIFY(!c.running()); // RUN as it was
        QCOMPARE(c.state().bank->patterns[6].length, std::uint16_t(384));
        QVERIFY(seq.rangeMode() == false);
    }
};

QTEST_GUILESS_MAIN(TestSeqController)
#include "tst_seqcontroller.moc"
