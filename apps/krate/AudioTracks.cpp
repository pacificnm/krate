#include "AudioTracks.h"

#include <algorithm>
#include <cmath>

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

namespace {
constexpr int kPeakBins = 600; // the waveform's columns

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
                       [&](const ClipInfo &c) { return c.file == t.takes[std::size_t(index)].file; });
}

void AudioTracksController::setOnLane(int track, int index, bool on)
{
    if (track < 0 || track >= kTracks || on == onLane(track, index))
        return;
    Track &t = m_tracks[std::size_t(track)];
    const TakeInfo &take = t.takes[std::size_t(index)];
    if (on)
        t.clips.push_back({take.file, take.startBeat}); // on top
    else
        std::erase_if(t.clips, [&](const ClipInfo &c) { return c.file == take.file; });
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
    std::erase_if(t.clips, [&](const ClipInfo &c) { return c.file == file; });
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
    for (const ClipInfo &clip : t.clips) {
        const int index = takeIndex(t, clip.file);
        if (index < 0)
            continue;
        const TakeInfo &take = t.takes[std::size_t(index)];
        list.append(QVariantMap{
            {QStringLiteral("take"), index},
            {QStringLiteral("name"), QFileInfo(take.file).completeBaseName()},
            {QStringLiteral("startBeat"), clip.startBeat},
            {QStringLiteral("seconds"), take.rate > 0 ? double(take.frames - take.offset) / take.rate : 0.0},
            {QStringLiteral("loaded"), t.cache.count(clip.file) > 0},
        });
    }
    return list;
}

void AudioTracksController::removeClip(int track, int clip)
{
    if (track < 0 || track >= kTracks)
        return;
    Track &t = m_tracks[std::size_t(track)];
    if (clip < 0 || clip >= int(t.clips.size()))
        return;
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
    m_tracks[std::size_t(track)].engine->setParam(index, float(value));
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
        if (t.loading.valid() && t.loading.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            Loaded loaded = t.loading.get();
            if (loaded.take)
                t.cache[t.loadingFile] = std::move(loaded);
            else
                qWarning("krate: couldn't read the take %s", qPrintable(t.loadingFile));
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
    if (!f.ok || f.frames == 0) {
        if (f.frames == 0)
            QFile::remove(QString::fromStdString(f.path));
        if (!f.ok)
            qWarning("krate: couldn't write the take %s", f.path.c_str());
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
    t.takes.push_back(take);
    t.clips.push_back({take.file, take.startBeat}); // on the lane where it was recorded, on top
    t.selected = int(t.takes.size()) - 1;
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
    for (const ClipInfo &c : t.clips)
        wanted.push_back(c.file);
    if (t.selected >= 0)
        wanted.push_back(t.takes[std::size_t(t.selected)].file);
    const auto isWanted = [&](const QString &file) { return std::find(wanted.begin(), wanted.end(), file) != wanted.end(); };

    // The engine's new playlist: the clips whose takes are loaded.
    auto playlist = std::make_unique<krate::Playlist>();
    for (const ClipInfo &c : t.clips) {
        const auto it = t.cache.find(c.file);
        const int index = takeIndex(t, c.file);
        if (it != t.cache.end() && index >= 0)
            playlist->clips.push_back({it->second.take.get(), c.startBeat, t.takes[std::size_t(index)].offset, 0});
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
                const QJsonObject j = v.toObject();
                const QString file = j.value(QStringLiteral("file")).toString();
                if (takeIndex(t, file) >= 0)
                    t.clips.push_back({file, j.value(QStringLiteral("startBeat")).toDouble()});
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
        for (const ClipInfo &clip : t.clips)
            clips.append(QJsonObject{{QStringLiteral("file"), clip.file}, {QStringLiteral("startBeat"), clip.startBeat}});
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
