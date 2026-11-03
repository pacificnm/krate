#include "AudioTracks.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QSaveFile>

#include "RackController.h"
#include "dsp/Wav.h"

#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcTracks, "krate.tracks")

namespace {
constexpr int kPeakBins = 600; // the waveform's columns
constexpr int kLiveBinsPerSecond = 20; // the waveform of a take being recorded

QJsonObject clipToJson(const AudioTracksController::LaneClip &clip)
{
    return {{QStringLiteral("file"), clip.file},
            {QStringLiteral("startBeat"), clip.startBeat},
            {QStringLiteral("trim"), clip.trim},
            {QStringLiteral("length"), clip.length},
            {QStringLiteral("fadeIn"), clip.fadeIn},
            {QStringLiteral("fadeOut"), clip.fadeOut}};
}

AudioTracksController::LaneClip clipFromJson(const QJsonObject &j)
{
    return {j.value(QStringLiteral("file")).toString(), j.value(QStringLiteral("startBeat")).toDouble(),
            std::max<qint64>(0, j.value(QStringLiteral("trim")).toInteger()),
            std::max<qint64>(0, j.value(QStringLiteral("length")).toInteger()),
            std::max<qint64>(0, j.value(QStringLiteral("fadeIn")).toInteger()),
            std::max<qint64>(0, j.value(QStringLiteral("fadeOut")).toInteger())};
}

// A rack device that is one of the tracks.
class AudioTrackInstance final : public krate::InstrumentInstance
{
public:
    explicit AudioTrackInstance(krate::AudioTrack &track) : m_track(track) {}
    krate::IInstrument &instrument() override { return m_track; }
    // Its takes and knobs are the tracks' own (recordings/tracks.json), not a song's sound.
    QJsonObject saveSound() override { return {}; }
    void loadSound(const QJsonObject &) override {}

private:
    krate::AudioTrack &m_track;
};
} // namespace

AudioTracksController::AudioTracksController(const QString &dir, QObject *parent)
    : QObject(parent)
    , m_dir(dir)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    std::vector<krate::AudioTrack *> engines;
    for (Track &t : m_tracks) {
        t.engine = std::make_unique<krate::AudioTrack>();
        engines.push_back(t.engine.get());
    }
    load();
    // Runs on the disk thread: a new file in the track's folder, named by when.
    m_recorder = std::make_unique<krate::Recorder>(engines, [this](int track) {
        const QString folder = trackDir(track);
        QDir().mkpath(folder);
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        QString path = QDir(folder).filePath(QStringLiteral("take-%1.wav").arg(stamp));
        for (int n = 2; QFileInfo::exists(path); ++n) // two takes in one second
            path = QDir(folder).filePath(QStringLiteral("take-%1-%2.wav").arg(stamp).arg(n));
        return path.toStdString();
    });
    m_recorder->start();
    for (int i = 0; i < kTracks; ++i)
        refresh(i);
    m_pollTimer.setInterval(50);
    connect(&m_pollTimer, &QTimer::timeout, this, &AudioTracksController::poll);
    m_pollTimer.start();
}

AudioTracksController::~AudioTracksController()
{
    finish();
    for (Track &t : m_tracks) {
        if (t.loading.valid())
            t.loading.wait();
        t.engine->setPlaylist(nullptr); // the audio has stopped (the host goes first)
    }
    s_instance = nullptr;
}

void AudioTracksController::finish()
{
    if (m_finished)
        return;
    m_pollTimer.stop();
    // Quitting while recording: what's recorded so far is kept as a take.
    m_recorder->stop();
    m_stopping = true;
    for (const auto &f : m_recorder->takeFinished())
        addFinished(f);
    m_finished = true; // after the rack and the host: neither is asked again
    m_rack = nullptr;
    m_roundTrip = [] { return 0; };
    save();
}

AudioTracksController *AudioTracksController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString AudioTracksController::trackDir(int track) const
{
    return QDir(m_dir).filePath(QStringLiteral("audio%1").arg(track + 1));
}

void AudioTracksController::setRecording(bool on)
{
    if (on == m_recording)
        return;
    m_recording = on;
    for (Track &t : m_tracks)
        t.engine->setRecordEnabled(on);
    QStringList armed;
    for (int i = 0; i < kTracks; ++i)
        if (m_tracks[std::size_t(i)].engine->param(krate::AudioTrack::Arm) > 0.5f)
            armed << QStringLiteral("Audio %1").arg(i + 1);
    qCInfo(lcTracks).noquote() << "REC" << (on ? "on" : "off") << "- armed:" << (armed.isEmpty() ? QStringLiteral("none") : armed.join(u", "));
    emit recordingChanged();
}

int AudioTracksController::trackOf(int rackDevice) const
{
    for (int i = 0; i < kTracks; ++i)
        if (rackDevice >= 0 && m_tracks[std::size_t(i)].device == rackDevice)
            return i;
    return -1;
}

int AudioTracksController::takeIndex(const Track &t, const QString &file) const
{
    for (std::size_t i = 0; i < t.takes.size(); ++i)
        if (t.takes[i].file == file)
            return int(i);
    return -1;
}

QVariantList AudioTracksController::takes(int track) const
{
    QVariantList list;
    if (track < 0 || track >= kTracks)
        return list;
    const Track &t = m_tracks[std::size_t(track)];
    for (std::size_t i = 0; i < t.takes.size(); ++i) {
        const TakeInfo &take = t.takes[i];
        list.append(QVariantMap{
            {QStringLiteral("name"), QFileInfo(take.file).completeBaseName()},
            {QStringLiteral("seconds"), take.rate > 0 ? double(take.frames) / take.rate : 0.0},
            {QStringLiteral("mono"), take.channels == 1},
            {QStringLiteral("onLane"), onLane(track, int(i))},
            {QStringLiteral("selected"), int(i) == t.selected},
        });
    }
    return list;
}

int AudioTracksController::selectedTake(int track) const
{
    return track >= 0 && track < kTracks ? m_tracks[std::size_t(track)].selected : -1;
}

void AudioTracksController::selectTake(int track, int index)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (index < -1 || index >= int(t.takes.size()) || index == t.selected)
        return;
    t.selected = index;
    refresh(track);
    bump();
}

bool AudioTracksController::onLane(int track, int index) const
{
    if (track < 0 || track >= kTracks)
        return false;
    const Track &t = m_tracks[std::size_t(track)];
    if (index < 0 || index >= int(t.takes.size()))
        return false;
    return std::any_of(t.clips.begin(), t.clips.end(),
                       [&](const LaneClip &c) { return c.file == t.takes[std::size_t(index)].file; });
}

void AudioTracksController::setOnLane(int track, int index, bool on)
{
    if (track < 0 || track >= kTracks || on == onLane(track, index))
        return;
    beforeEdit();
    Track &t = m_tracks[std::size_t(track)];
    const TakeInfo &take = t.takes[std::size_t(index)];
    if (on)
        t.clips.push_back({take.file, take.startBeat}); // on top
    else
        std::erase_if(t.clips, [&](const LaneClip &c) { return c.file == take.file; });
    refresh(track);
    save();
    bump();
}

void AudioTracksController::deleteTake(int track, int index)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (index < 0 || index >= int(t.takes.size()))
        return;
    const QString file = t.takes[std::size_t(index)].file;
    if (onLane(track, index))
        beforeEdit();
    std::erase_if(t.clips, [&](const LaneClip &c) { return c.file == file; });
    t.takes.erase(t.takes.begin() + index);
    if (index == t.selected)
        t.selected = -1;
    else if (index < t.selected)
        --t.selected;
    refresh(track); // lets go of its audio
    QFile::remove(QDir(trackDir(track)).filePath(file));
    save();
    bump();
}

QVariantList AudioTracksController::peaks(int track) const
{
    return takePeaks(track, selectedTake(track));
}

QVariantMap AudioTracksController::liveTake(int track) const
{
    if (track < 0 || track >= kTracks)
        return {};
    const Track &t = m_tracks[std::size_t(track)];
    if (t.live.id == 0 || t.live.frames <= 0)
        return {};
    QVariantList peaks;
    peaks.reserve(qsizetype(t.live.peaks.size()));
    for (float p : t.live.peaks)
        peaks.append(p);
    return {{QStringLiteral("startBeat"), t.live.startBeat},
            {QStringLiteral("seconds"), double(t.live.frames) / std::max(1.0, t.engine->sampleRate())},
            {QStringLiteral("peaks"), peaks}};
}

QVariantList AudioTracksController::takePeaks(int track, int index) const
{
    QVariantList list;
    if (track < 0 || track >= kTracks)
        return list;
    const Track &t = m_tracks[std::size_t(track)];
    if (index < 0 || index >= int(t.takes.size()))
        return list;
    const auto it = t.cache.find(t.takes[std::size_t(index)].file);
    if (it != t.cache.end())
        for (float p : it->second.peaks)
            list.append(p);
    return list;
}

QVariantList AudioTracksController::clips(int track) const
{
    QVariantList list;
    if (track < 0 || track >= kTracks)
        return list;
    const Track &t = m_tracks[std::size_t(track)];
    for (const LaneClip &clip : t.clips) {
        const int index = takeIndex(t, clip.file);
        if (index < 0)
            continue;
        const TakeInfo &take = t.takes[std::size_t(index)];
        const double rate = std::max(take.rate, 1);
        const double total = double(std::max<qint64>(take.frames, 1));
        const double from = double(take.offset + clip.trim);
        list.append(QVariantMap{
            {QStringLiteral("take"), index},
            {QStringLiteral("name"), QFileInfo(take.file).completeBaseName()},
            {QStringLiteral("startBeat"), clip.startBeat},
            {QStringLiteral("seconds"), double(playable(t, clip)) / rate},
            {QStringLiteral("from"), from / total},
            {QStringLiteral("to"), (from + double(playable(t, clip))) / total},
            {QStringLiteral("fadeIn"), double(clip.fadeIn) / rate},
            {QStringLiteral("fadeOut"), double(clip.fadeOut) / rate},
            {QStringLiteral("loaded"), t.cache.count(clip.file) > 0},
        });
    }
    return list;
}

qint64 AudioTracksController::playable(const Track &t, const LaneClip &c) const
{
    const int index = takeIndex(t, c.file);
    if (index < 0)
        return 0;
    const TakeInfo &take = t.takes[std::size_t(index)];
    const qint64 rest = std::max<qint64>(0, take.frames - take.offset - c.trim);
    return c.length > 0 ? std::min(c.length, rest) : rest;
}

void AudioTracksController::laneEdited(int track)
{
    refresh(track);
    save();
    bump();
}

namespace {
constexpr double kMinSeconds = 0.01; // the shortest a clip is trimmed or split to
// A take's frames in a beat.
double framesPerBeat(int rate, double bpm) { return 60.0 / std::max(bpm, 1.0) * std::max(rate, 1); }
} // namespace

int AudioTracksController::moveClip(int track, int clip, double startBeat)
{
    if (track < 0 || track >= kTracks)
        return -1;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return -1;
    startBeat = std::max(0.0, startBeat);
    if (startBeat == t.clips[std::size_t(clip)].startBeat)
        return clip;
    beforeEdit();
    LaneClip moved = t.clips[std::size_t(clip)];
    moved.startBeat = startBeat;
    t.clips.erase(t.clips.begin() + clip);
    t.clips.push_back(moved); // on top
    laneEdited(track);
    return int(t.clips.size()) - 1;
}

void AudioTracksController::trimClipStart(int track, int clip, double startBeat, double bpm)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return;
    LaneClip c = t.clips[std::size_t(clip)];
    const TakeInfo &take = t.takes[std::size_t(takeIndex(t, c.file))];
    const double fpb = framesPerBeat(take.rate, bpm);
    const qint64 rest = playable(t, c);
    // Frames to take off (or put back, < 0): no further back than the take's start, not past its end.
    const qint64 d = std::clamp<qint64>(std::llround((startBeat - c.startBeat) * fpb), -c.trim,
                                        rest - qint64(kMinSeconds * take.rate));
    if (d == 0)
        return;
    beforeEdit();
    c.trim += d;
    c.startBeat += double(d) / fpb;
    c.length = rest - d;
    c.fadeIn = std::min(c.fadeIn, c.length);
    c.fadeOut = std::min(c.fadeOut, c.length - c.fadeIn);
    t.clips[std::size_t(clip)] = c;
    laneEdited(track);
}

void AudioTracksController::trimClipEnd(int track, int clip, double endBeat, double bpm)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return;
    LaneClip c = t.clips[std::size_t(clip)];
    const TakeInfo &take = t.takes[std::size_t(takeIndex(t, c.file))];
    const qint64 most = std::max<qint64>(0, take.frames - take.offset - c.trim);
    const qint64 length = std::clamp<qint64>(std::llround((endBeat - c.startBeat) * framesPerBeat(take.rate, bpm)),
                                             qint64(kMinSeconds * take.rate), most);
    if (length == playable(t, c))
        return;
    beforeEdit();
    c.length = length;
    c.fadeIn = std::min(c.fadeIn, c.length);
    c.fadeOut = std::min(c.fadeOut, c.length - c.fadeIn);
    t.clips[std::size_t(clip)] = c;
    laneEdited(track);
}

int AudioTracksController::splitClip(int track, int clip, double beat, double bpm)
{
    if (track < 0 || track >= kTracks)
        return -1;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return -1;
    const LaneClip c = t.clips[std::size_t(clip)];
    const TakeInfo &take = t.takes[std::size_t(takeIndex(t, c.file))];
    const double fpb = framesPerBeat(take.rate, bpm);
    const qint64 rest = playable(t, c), d = std::llround((beat - c.startBeat) * fpb);
    const qint64 shortest = qint64(kMinSeconds * take.rate);
    if (d < shortest || d > rest - shortest)
        return -1;
    beforeEdit();
    LaneClip first = c, second = c;
    first.length = d;
    first.fadeIn = std::min(c.fadeIn, d);
    first.fadeOut = 0;
    second.trim += d;
    second.startBeat += double(d) / fpb;
    second.length = rest - d;
    second.fadeIn = 0;
    second.fadeOut = std::min(c.fadeOut, rest - d);
    t.clips[std::size_t(clip)] = first;
    t.clips.push_back(second);
    laneEdited(track);
    return int(t.clips.size()) - 1;
}

int AudioTracksController::duplicateClip(int track, int clip, double bpm)
{
    if (track < 0 || track >= kTracks)
        return -1;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return -1;
    beforeEdit();
    LaneClip copy = t.clips[std::size_t(clip)];
    const TakeInfo &take = t.takes[std::size_t(takeIndex(t, copy.file))];
    copy.startBeat += double(playable(t, copy)) / framesPerBeat(take.rate, bpm);
    t.clips.push_back(copy);
    laneEdited(track);
    return int(t.clips.size()) - 1;
}

void AudioTracksController::setClipFades(int track, int clip, double fadeInSeconds, double fadeOutSeconds)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return;
    LaneClip c = t.clips[std::size_t(clip)];
    const TakeInfo &take = t.takes[std::size_t(takeIndex(t, c.file))];
    const qint64 rest = playable(t, c);
    const qint64 in = std::clamp<qint64>(std::llround(fadeInSeconds * take.rate), 0, rest);
    const qint64 out = std::clamp<qint64>(std::llround(fadeOutSeconds * take.rate), 0, rest - in);
    if (in == c.fadeIn && out == c.fadeOut)
        return;
    beforeEdit();
    c.fadeIn = in;
    c.fadeOut = out;
    t.clips[std::size_t(clip)] = c;
    laneEdited(track);
}

AudioTracksController::Lanes AudioTracksController::lanes() const
{
    Lanes lanes;
    for (int i = 0; i < kTracks; ++i)
        lanes[std::size_t(i)] = m_tracks[std::size_t(i)].clips;
    return lanes;
}

void AudioTracksController::setLanes(const Lanes &lanes)
{
    for (int i = 0; i < kTracks; ++i) {
        Track &t = m_tracks[std::size_t(i)];
        std::vector<LaneClip> clips;
        for (const LaneClip &c : lanes[std::size_t(i)])
            if (takeIndex(t, c.file) >= 0) // a take deleted since is gone for good
                clips.push_back(c);
        if (clips == t.clips)
            continue;
        t.clips = std::move(clips);
        refresh(i);
    }
    save();
    bump();
}

QJsonArray AudioTracksController::lanesJson() const
{
    QJsonArray lanes;
    for (const Track &t : m_tracks) {
        QJsonArray clips;
        for (const LaneClip &clip : t.clips)
            clips.append(clipToJson(clip));
        lanes.append(clips);
    }
    return lanes;
}

void AudioTracksController::setLanesJson(const QJsonArray &lanes)
{
    Lanes put;
    for (int i = 0; i < kTracks; ++i)
        for (const QJsonValue &v : lanes.at(i).toArray())
            put[std::size_t(i)].push_back(clipFromJson(v.toObject()));
    setLanes(put);
}

void AudioTracksController::removeClip(int track, int clip)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return;
    beforeEdit();
    t.clips.erase(t.clips.begin() + clip);
    refresh(track);
    save();
    bump();
}

bool AudioTracksController::isRecording(int track) const
{
    return track >= 0 && track < kTracks && m_tracks[std::size_t(track)].engine->recording();
}

qreal AudioTracksController::inputLevel(int track) const
{
    return track >= 0 && track < kTracks ? m_tracks[std::size_t(track)].level : 0.0;
}

qreal AudioTracksController::param(int track, int index) const
{
    return track >= 0 && track < kTracks ? m_tracks[std::size_t(track)].engine->param(index) : 0.0;
}

void AudioTracksController::setParam(int track, int index, qreal value)
{
    if (track < 0 || track >= kTracks)
        return;
    const float before = m_tracks[std::size_t(track)].engine->param(index);
    m_tracks[std::size_t(track)].engine->setParam(index, float(value));
    if (index == krate::AudioTrack::Arm && (before > 0.5f) != (value > 0.5))
        qCInfo(lcTracks).noquote() << QStringLiteral("Audio %1").arg(track + 1) << (value > 0.5 ? "armed" : "disarmed");
    save();
    bump();
}

QString AudioTracksController::paramText(int track, int index) const
{
    if (track < 0 || track >= kTracks || index < 0 || index >= krate::AudioTrack::ParamCount)
        return {};
    const krate::AudioTrack &engine = *m_tracks[std::size_t(track)].engine;
    return QString::fromStdString(engine.parameters()[std::size_t(index)].format(engine.param(index)));
}

void AudioTracksController::poll()
{
    for (const auto &f : m_recorder->takeFinished())
        addFinished(f);
    for (int i = 0; i < kTracks; ++i) {
        Track &t = m_tracks[std::size_t(i)];
        // The log: a take starting and ending; armed with REC on while playing but not recording, why.
        const bool now = t.engine->recording();
        if (now != t.wasRecording) {
            t.wasRecording = now;
            qCInfo(lcTracks).noquote() << QStringLiteral("Audio %1").arg(i + 1) << (now ? "recording" : "stopped recording");
        }
        const bool shouldRecord = m_recording && t.engine->param(krate::AudioTrack::Arm) > 0.5f && m_playing && m_playing();
        if (shouldRecord && !now && ++t.notRecordingPolls == 40) // ~2 s
            qCWarning(lcTracks).noquote() << QStringLiteral("Audio %1").arg(i + 1) << "is armed, REC is on and the transport plays, but it isn't recording:"
                                          << "writer busy" << t.engine->writerBusy() << "REC in the engine" << t.engine->recordEnabled()
                                          << "in the rack" << (m_rack && t.device >= 0 && m_rack->isPresent(t.device));
        if (!shouldRecord || now)
            t.notRecordingPolls = 0;
        // The take being recorded, drawn as it grows: each poll's peak over the frames since the last.
        if (now) {
            const std::uint32_t id = t.engine->takeId();
            if (id != t.live.id) {
                t.live = {};
                t.live.id = id;
                t.live.startBeat = t.engine->takeStartBeat();
            }
            const qint64 frames = t.engine->takeFrames();
            const float peak = t.engine->takeRecordedPeak();
            const qint64 perBin = std::max<qint64>(1, qint64(t.engine->sampleRate()) / kLiveBinsPerSecond);
            if (frames > t.live.frames) {
                const std::size_t first = std::size_t(t.live.frames / perBin), last = std::size_t((frames - 1) / perBin);
                if (t.live.peaks.size() < last + 1)
                    t.live.peaks.resize(last + 1, 0.0f);
                for (std::size_t b = first; b <= last; ++b)
                    t.live.peaks[b] = std::max(t.live.peaks[b], std::min(peak, 1.0f));
                t.live.frames = frames;
            }
        } else if (t.live.id != 0 && !t.live.landed.isEmpty() && t.cache.count(t.live.landed)) {
            t.live = {}; // on the lane with its own waveform now
        }
        if (t.loading.valid() && t.loading.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            Loaded loaded = t.loading.get();
            if (loaded.take) {
                t.cache[t.loadingFile] = std::move(loaded);
            } else {
                qWarning("krate: couldn't read the take %s", qPrintable(t.loadingFile));
                if (t.loadingFile == t.live.landed)
                    t.live = {};
            }
            refresh(i); // plays it, loads the next
            bump();
        }
        // A retired playlist (and its takes) is freed once the audio thread can't be in it any more.
        const std::uint64_t blocks = t.engine->blocks();
        std::erase_if(t.retired, [blocks](const Retired &r) { return blocks >= r.block + 2; });
        // The input meter: -60..0 dB, falling back slowly.
        const float peak = t.engine->takeInputPeak();
        const qreal level = peak > 0.0f ? std::clamp((20.0 * std::log10(double(peak)) + 60.0) / 60.0, 0.0, 1.0) : 0.0;
        t.level = std::max(level, t.level - 0.04);
    }
    ++m_frame;
    emit frameChanged();
}

void AudioTracksController::addFinished(const krate::Recorder::Finished &f)
{
    if (f.track < 0 || f.track >= kTracks)
        return;
    // What was drawn while it recorded: kept until the take's own waveform is loaded (poll).
    Track::Live live = std::exchange(m_tracks[std::size_t(f.track)].live, {});
    if (!f.ok || f.frames == 0) {
        if (f.frames == 0) {
            QFile::remove(QString::fromStdString(f.path));
            qCWarning(lcTracks).noquote() << QStringLiteral("Audio %1:").arg(f.track + 1) << "an empty take (no audio reached it), dropped";
        }
        if (!f.ok)
            qCWarning(lcTracks).noquote() << QStringLiteral("Audio %1:").arg(f.track + 1) << "couldn't write the take" << QString::fromStdString(f.path);
        if (f.frames == 0)
            return;
    }
    Track &t = m_tracks[std::size_t(f.track)];
    TakeInfo take;
    take.file = QFileInfo(QString::fromStdString(f.path)).fileName();
    take.startBeat = f.startBeat;
    // From the interface: what was played reached Krate a round trip late.
    const bool fromInterface = !m_rack || t.device < 0 || m_rack->inputSource(t.device) < 0;
    take.offset = fromInterface ? m_roundTrip() : 0;
    take.rate = f.rate;
    take.frames = qint64(f.frames);
    take.channels = f.channels;
    if (!m_finished)
        beforeEdit(); // (not while quitting: the arranger may be gone)
    t.takes.push_back(take);
    t.clips.push_back({take.file, take.startBeat}); // on the lane where it was recorded, on top
    t.selected = int(t.takes.size()) - 1;
    t.live = std::move(live);
    t.live.landed = take.file;
    qCInfo(lcTracks).noquote() << QStringLiteral("Audio %1: take %2, %3 s, at beat %4").arg(f.track + 1).arg(take.file)
                                         .arg(double(f.frames) / std::max(1, f.rate), 0, 'f', 1).arg(f.startBeat, 0, 'f', 2);
    refresh(f.track);
    save();
    bump();
}

void AudioTracksController::refresh(int track)
{
    Track &t = m_tracks[std::size_t(track)];
    if (m_stopping)
        return; // quitting: nothing more to play
    // What's needed: the lane's takes and the selected one.
    std::vector<QString> wanted;
    for (const LaneClip &c : t.clips)
        wanted.push_back(c.file);
    if (t.selected >= 0)
        wanted.push_back(t.takes[std::size_t(t.selected)].file);
    const auto isWanted = [&](const QString &file) { return std::find(wanted.begin(), wanted.end(), file) != wanted.end(); };

    // The engine's new playlist: the clips whose takes are loaded.
    auto playlist = std::make_unique<krate::Playlist>();
    for (const LaneClip &c : t.clips) {
        const auto it = t.cache.find(c.file);
        const int index = takeIndex(t, c.file);
        if (it != t.cache.end() && index >= 0)
            playlist->clips.push_back({it->second.take.get(), c.startBeat, t.takes[std::size_t(index)].offset + c.trim,
                                       c.length, c.fadeIn, c.fadeOut});
    }
    t.engine->setPlaylist(playlist.get());
    Retired old;
    old.playlist = std::move(t.playlist);
    old.block = t.engine->blocks();
    t.playlist = std::move(playlist);
    for (auto it = t.cache.begin(); it != t.cache.end();) { // takes no longer needed go with the old playlist
        if (isWanted(it->first)) {
            ++it;
        } else {
            old.takes.push_back(std::move(it->second.take));
            it = t.cache.erase(it);
        }
    }
    if (old.playlist || !old.takes.empty())
        t.retired.push_back(std::move(old));

    // The next take to load (one at a time).
    if (t.loading.valid())
        return;
    for (const QString &file : wanted) {
        const int index = takeIndex(t, file);
        if (t.cache.count(file) || index < 0)
            continue;
        t.loadingFile = file;
        const std::string path = QDir(trackDir(track)).filePath(file).toStdString();
        t.loading = std::async(std::launch::async, [path] {
            Loaded loaded;
            krate::WavData data;
            if (!krate::readWavFile(path, &data) || data.left.empty())
                return loaded;
            auto take = std::make_unique<krate::Take>();
            take->left = std::move(data.left);
            take->right = std::move(data.right);
            take->rate = data.rate;
            // The waveform: each column's loudest sample, both sides.
            const std::size_t n = take->frames();
            loaded.peaks.assign(kPeakBins, 0.0f);
            for (std::size_t i = 0; i < n; ++i) {
                float v = std::fabs(take->left[i]);
                if (!take->right.empty())
                    v = std::max(v, std::fabs(take->right[i]));
                float &bin = loaded.peaks[std::min<std::size_t>(i * kPeakBins / n, kPeakBins - 1)];
                bin = std::max(bin, std::min(v, 1.0f));
            }
            loaded.take = std::move(take);
            return loaded;
        });
        return;
    }
}

void AudioTracksController::bump()
{
    ++m_revision;
    emit changed();
}

void AudioTracksController::load()
{
    QFile file(QDir(m_dir).filePath(QStringLiteral("tracks.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray tracks = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("tracks")).toArray();
    for (int i = 0; i < kTracks && i < tracks.size(); ++i) {
        const QJsonObject o = tracks[i].toObject();
        Track &t = m_tracks[std::size_t(i)];
        const auto params = t.engine->parameters();
        for (std::size_t p = 0; p < params.size(); ++p) {
            const QString key = QString::fromUtf8(params[p].key.data(), qsizetype(params[p].key.size()));
            if (o.contains(key))
                t.engine->setParam(int(p), float(o.value(key).toDouble()));
        }
        for (const QJsonValue &v : o.value(QStringLiteral("takes")).toArray()) {
            const QJsonObject j = v.toObject();
            TakeInfo take;
            take.file = j.value(QStringLiteral("file")).toString();
            if (take.file.isEmpty() || take.file.contains(u'/') || !QFileInfo::exists(QDir(trackDir(i)).filePath(take.file)))
                continue; // gone (deleted by hand)
            take.startBeat = j.value(QStringLiteral("startBeat")).toDouble();
            take.offset = j.value(QStringLiteral("offset")).toInteger();
            take.rate = j.value(QStringLiteral("rate")).toInt(48000);
            take.frames = j.value(QStringLiteral("frames")).toInteger();
            take.channels = j.value(QStringLiteral("channels")).toInt(2);
            t.takes.push_back(take);
        }
        if (o.contains(QStringLiteral("clips"))) {
            for (const QJsonValue &v : o.value(QStringLiteral("clips")).toArray()) {
                LaneClip clip = clipFromJson(v.toObject());
                if (takeIndex(t, clip.file) >= 0)
                    t.clips.push_back(std::move(clip));
            }
        } else if (const int active = takeIndex(t, o.value(QStringLiteral("active")).toString()); active >= 0) {
            t.clips.push_back({t.takes[std::size_t(active)].file, t.takes[std::size_t(active)].startBeat}); // before the lanes
        }
        t.selected = t.takes.empty() ? -1 : int(t.takes.size()) - 1;
    }
}

void AudioTracksController::save()
{
    QJsonArray tracks;
    for (int i = 0; i < kTracks; ++i) {
        const Track &t = m_tracks[std::size_t(i)];
        QJsonObject o;
        const auto params = t.engine->parameters();
        for (std::size_t p = 0; p < params.size(); ++p)
            o.insert(QString::fromUtf8(params[p].key.data(), qsizetype(params[p].key.size())), t.engine->param(int(p)));
        QJsonArray takes;
        for (const TakeInfo &take : t.takes)
            takes.append(QJsonObject{
                {QStringLiteral("file"), take.file},
                {QStringLiteral("startBeat"), take.startBeat},
                {QStringLiteral("offset"), take.offset},
                {QStringLiteral("rate"), take.rate},
                {QStringLiteral("frames"), take.frames},
                {QStringLiteral("channels"), take.channels},
            });
        o.insert(QStringLiteral("takes"), takes);
        QJsonArray clips;
        for (const LaneClip &clip : t.clips)
            clips.append(clipToJson(clip));
        o.insert(QStringLiteral("clips"), clips);
        tracks.append(o);
    }
    QDir().mkpath(m_dir);
    QSaveFile file(QDir(m_dir).filePath(QStringLiteral("tracks.json")));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("tracks"), tracks}}).toJson());
    file.commit();
}

QList<krate::InstrumentEntry> audioTrackEntries(AudioTracksController &tracks)
{
    QList<krate::InstrumentEntry> entries;
    for (int i = 0; i < AudioTracksController::kTracks; ++i) {
        krate::InstrumentEntry entry;
        entry.id = QStringLiteral("audio%1").arg(i + 1);
        entry.name = QStringLiteral("Audio %1").arg(i + 1);
        entry.playsNotes = false;
        entry.inRackByDefault = false;
        entry.qmlModule = QStringLiteral("KrateRack");
        entry.panelComponent = QStringLiteral("AudioTrackPanel");
        entry.create = [&tracks, i](const krate::InstrumentContext &) -> std::unique_ptr<krate::InstrumentInstance> {
            return std::make_unique<AudioTrackInstance>(tracks.engine(i));
        };
        entries.append(entry);
    }
    return entries;
}
