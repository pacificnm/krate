#include "SongController.h"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QJSEngine>
#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QLoggingCategory>

#include "RackController.h"
#include "app/InstrumentRegistry.h"
#include "core/Instrument.h"
#include "song/Arrange.h"
#include "song/GpFile.h"
#include "song/MidiFile.h"

Q_LOGGING_CATEGORY(lcSong, "krate.song")

namespace s = krate::song;

namespace {

constexpr int kPollIntervalMs = 33;

bool isMidi(const QString &path)
{
    return path.endsWith(u".mid", Qt::CaseInsensitive) || path.endsWith(u".midi", Qt::CaseInsensitive);
}

bool isTab(const QString &path) { return path.endsWith(u".gp", Qt::CaseInsensitive); }

QString clock(double seconds)
{
    const int s = int(std::max(0.0, seconds));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

const char *noteName(int pitch)
{
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[((pitch % 12) + 12) % 12];
}

} // namespace

SongController::SongController(s::SongPlayer &player, RackController &rack, const QList<krate::InstrumentEntry> &entries,
                               std::vector<krate::InstrumentInstance *> instances, SongLibrary &library, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_rack(rack)
    , m_instances(std::move(instances))
    , m_library(library)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    for (const krate::InstrumentEntry &e : entries)
        m_ids << e.id;
    m_choices = noteTargets(entries); // every instrument that plays notes

    m_timer.setInterval(kPollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &SongController::pollPosition);
    m_timer.start();
}

SongController::~SongController()
{
    if (m_backing)
        m_backing->setClip(nullptr); // the host has stopped by now; nothing plays it
    for (QThread *reader : m_readers) {
        reader->wait();
        delete reader;
    }
    restoreOwned(); // quitting with a song open: the instruments it changed keep (and save) the sounds from before it
    setSequencers(true);
    s_instance = nullptr;
}

SongController *SongController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString SongController::title() const
{
    if (!m_details.title.isEmpty())
        return m_details.title;
    if (m_score && !m_score->title.empty())
        return QString::fromStdString(m_score->title);
    if (m_midi && !m_midi->title.empty())
        return QString::fromStdString(m_midi->title);
    const QString path = m_tabPath.isEmpty() ? m_midiPath : m_tabPath;
    return path.isEmpty() ? QString() : QFileInfo(path).completeBaseName();
}

QString SongController::artist() const { return m_details.artist; }

QString SongController::files() const
{
    QStringList names;
    if (!m_tabPath.isEmpty())
        names << QFileInfo(m_tabPath).fileName();
    if (!m_midiPath.isEmpty())
        names << QFileInfo(m_midiPath).fileName();
    return names.join(QStringLiteral(" + "));
}

QStringList SongController::instrumentChoices() const
{
    QStringList out{QStringLiteral("—")};
    for (const Choice &c : m_choices)
        out << c.label;
    return out;
}

QVariantList SongController::tracks() const
{
    QVariantList out;
    if (!m_song)
        return out;
    for (std::size_t t = 0; t < m_song->tracks.size() && t < m_tracks.size(); ++t) {
        const s::Track &track = m_song->tracks[t];
        const s::ScoreTrack *score = m_score && t < m_score->tracks.size() ? &m_score->tracks[t] : nullptr;
        QString tuning;
        if (score)
            for (const int pitch : score->tuning) // low to high, as players name it ("E A D G B E")
                tuning += (tuning.isEmpty() ? QString() : QStringLiteral(" ")) + QString::fromLatin1(noteName(pitch));
        const TrackSettings &st = m_tracks[t];
        out << QVariantMap{
            {QStringLiteral("name"), QString::fromStdString(track.name)},
            {QStringLiteral("role"), QString::fromLatin1(s::roleName(s::guessRole(track, score)))},
            {QStringLiteral("notes"), track.noteCount},
            {QStringLiteral("tuning"), tuning},
            {QStringLiteral("strings"), score ? int(score->tuning.size()) : 0},
            {QStringLiteral("choice"), st.choice},
            {QStringLiteral("level"), st.level},
            {QStringLiteral("mute"), st.mute},
            {QStringLiteral("solo"), st.solo},
            {QStringLiteral("fromMidi"), st.fromMidi && lineUp(int(t))},
            {QStringLiteral("canMidi"), m_tab.has_value() && lineUp(int(t))},
            {QStringLiteral("mine"), int(t) == m_myTrack},
            {QStringLiteral("viewed"), int(t) == m_viewTrack},
            {QStringLiteral("hasTab"), score && !score->drums && !score->tuning.empty()},
            {QStringLiteral("drums"), score ? score->drums : track.drums},
        };
    }
    return out;
}

void SongController::setViewTrack(int track)
{
    if (!m_song || track < -1 || track >= int(m_song->tracks.size()) || track == m_viewTrack)
        return;
    m_viewTrack = track;
    emit tracksChanged();
}

void SongController::setMyTrack(int track)
{
    if (!m_song || track < -1 || track >= int(m_song->tracks.size()) || track == m_myTrack)
        return;
    m_myTrack = track;
    if (track >= 0)
        m_viewTrack = track; // your part comes into view
    emit tracksChanged();
    saveSettings();
}

int SongController::barCount() const
{
    if (!m_song)
        return 0;
    return m_song->tempo.barAt(std::max<s::Tick>(0, m_song->length - 1)).bar + 1;
}

void SongController::setError(const QString &error)
{
    if (error == m_error)
        return;
    m_error = error;
    if (!error.isEmpty())
        qCWarning(lcSong) << error;
    emit errorChanged();
}

// --- Opening ---------------------------------------------------------------------------

void SongController::open(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const QUrl &url : urls)
        if (url.isLocalFile())
            paths << url.toLocalFile();
    openPaths(paths);
}

void SongController::openPaths(const QStringList &paths)
{
    QString tabPath, midiPath;
    for (const QString &p : paths) {
        if (isTab(p))
            tabPath = p;
        else if (isMidi(p))
            midiPath = p;
    }
    if (tabPath.isEmpty() && midiPath.isEmpty()) {
        setError(tr("Open a .gp tab file or a .mid file"));
        return;
    }
    // A lone file brings its namesake from the same folder.
    const auto sibling = [](const QString &path, const QStringList &extensions) {
        const QFileInfo info(path);
        for (const QString &ext : extensions) {
            const QString candidate = info.dir().filePath(info.completeBaseName() + ext);
            if (QFileInfo::exists(candidate))
                return candidate;
        }
        return QString();
    };
    if (midiPath.isEmpty())
        midiPath = sibling(tabPath, {QStringLiteral(".mid"), QStringLiteral(".midi"), QStringLiteral(".MID")});
    if (tabPath.isEmpty())
        tabPath = sibling(midiPath, {QStringLiteral(".gp"), QStringLiteral(".GP")});

    // Read them first: a file that doesn't read leaves the open song alone.
    QStringList problems;
    std::optional<s::Score> score;
    if (!tabPath.isEmpty()) {
        auto result = s::readGpFile(tabPath.toStdString());
        if (result.score)
            score = std::move(result.score);
        else {
            problems << QFileInfo(tabPath).fileName() + QStringLiteral(": ") + QString::fromStdString(result.error);
            tabPath.clear();
        }
    }
    if (!midiPath.isEmpty()) {
        auto result = s::readMidiFile(midiPath.toStdString());
        if (!result.song) {
            problems << QFileInfo(midiPath).fileName() + QStringLiteral(": ") + QString::fromStdString(result.error);
            midiPath.clear();
        }
    }
    setError(problems.join(u'\n'));
    if (tabPath.isEmpty() && midiPath.isEmpty())
        return;

    // Into the library (or the song already there with these files), and play its copies.
    SongLibrary::Details details;
    if (score) {
        details.title = QString::fromStdString(score->title);
        details.artist = QString::fromStdString(score->artist);
        details.album = QString::fromStdString(score->album);
    }
    if (details.title.isEmpty() || details.artist.isEmpty()) {
        const SongLibrary::Details fromName = SongLibrary::detailsFromFileName(midiPath.isEmpty() ? tabPath : midiPath);
        if (details.title.isEmpty())
            details.title = fromName.title;
        if (details.artist.isEmpty())
            details.artist = fromName.artist;
    }
    if (const auto added = m_library.add(tabPath, midiPath, details)) {
        load(added->tabFile, added->midiFile, added->id, added->details);
    } else {
        if (m_library.isOpen())
            setError(m_library.lastError());
        load(tabPath, midiPath, 0, details); // play it anyway, unsaved
    }
}

void SongController::openSong(qint64 id)
{
    const auto song = m_library.song(id);
    if (!song) {
        setError(tr("That song isn't in the library any more"));
        return;
    }
    setError({});
    if (!load(song->tabFile, song->midiFile, song->id, song->details))
        setError(tr("The library's copy of %1 can't be read").arg(song->details.title));
}

bool SongController::load(const QString &tabPath, const QString &midiPath, qint64 songId, const SongLibrary::Details &details)
{
    std::optional<s::Score> score;
    std::optional<s::Song> tab, midi;
    if (!tabPath.isEmpty())
        if (auto result = s::readGpFile(tabPath.toStdString()); result.song) {
            score = std::move(result.score);
            tab = std::move(result.song);
        }
    if (!midiPath.isEmpty())
        if (auto result = s::readMidiFile(midiPath.toStdString()); result.song)
            midi = std::move(result.song);
    if (!tab && !midi)
        return false;

    m_rack.setPlaying(false);
    // Sounds: what the last song changed goes back first, then this song's
    // saved ones go on (the instruments and mixer it has sounds for).
    restoreOwned();
    m_before = capture();
    const auto entry = songId ? m_library.song(songId) : std::nullopt;
    m_soundsSaved = entry && entry->soundsSaved;
    if (m_soundsSaved) {
        const Snapshot saved{m_library.sounds(songId), m_library.mixer(songId)};
        m_ownedSounds = saved.sounds.keys();
        m_ownsMixer = !saved.mixer.isEmpty();
        restore(saved);
    }

    m_score = std::move(score);
    m_tab = std::move(tab);
    m_midi = std::move(midi);
    m_tabPath = m_tab ? QFileInfo(tabPath).absoluteFilePath() : QString();
    m_midiPath = m_midi ? QFileInfo(midiPath).absoluteFilePath() : QString();
    m_songId = songId;
    m_details = details;
    const std::size_t count = (m_tab ? m_tab->tracks.size() : m_midi->tracks.size());
    m_tracks.assign(std::min<std::size_t>(count, s::SongPlayer::kMaxTracks), TrackSettings{});
    for (std::size_t t = 0; t < m_tracks.size(); ++t)
        m_tracks[t].fromMidi = m_tab && lineUp(int(t)); // the MIDI file plays slides and velocities as exported
    m_myTrack = -1;
    m_song.reset();
    rebuild(); // sets m_song
    suggest();
    loadSettings();
    for (int t = 0; t < int(m_tracks.size()); ++t)
        applyTrack(t);
    rebuild();
    m_player.cue(0);
    setSequencers(false);
    m_viewTrack = m_myTrack;
    m_bar = m_beat = 1;
    pollPosition();
    if (m_songId)
        m_library.markOpened(m_songId);
    // Its backing track, if it has one.
    const auto entry2 = m_songId ? m_library.song(m_songId) : std::nullopt;
    m_backingOffset = entry2 ? entry2->backingOffset : 0.0;
    if (m_backing)
        m_backing->setOffset(m_backingOffset);
    startBacking(entry2 ? entry2->backingFile : QString());
    ++m_libraryRevision; // a new song in it, or another one open
    emit libraryChanged();
    emit songChanged();
    emit tracksChanged();
    saveSettings();
    return true;
}

void SongController::setDetails(const QString &artist, const QString &album, const QString &title, int trackNumber)
{
    if (!m_song)
        return;
    m_details = {artist.trimmed(), album.trimmed(), title.trimmed(), std::max(0, trackNumber)};
    if (m_songId)
        m_library.setDetails(m_songId, m_details);
    emit songChanged();
    ++m_libraryRevision;
    emit libraryChanged();
}

void SongController::close()
{
    m_rack.setPlaying(false);
    m_player.load(nullptr);
    for (int t = 0; t < s::SongPlayer::kMaxTracks; ++t)
        m_player.setAssignment(t, {});
    m_score.reset();
    m_tab.reset();
    m_midi.reset();
    m_song.reset();
    m_tabPath.clear();
    m_midiPath.clear();
    m_tracks.clear();
    m_myTrack = -1;
    m_viewTrack = -1;
    m_songId = 0;
    m_details = {};
    m_soundsSaved = false;
    restoreOwned(); // your sounds and mixer, as they were before the song
    startBacking({});
    setSequencers(true);
    setError({});
    m_library.setLastSong(0); // nothing reopens next time
    ++m_libraryRevision; // no song is "open" in the list
    emit libraryChanged();
    emit songChanged();
    emit tracksChanged();
}

bool SongController::lineUp(int track) const
{
    if (!m_midi || track < 0 || track >= int(m_midi->tracks.size()))
        return false;
    if (!m_tab)
        return true;
    if (track >= int(m_tab->tracks.size()))
        return false;
    const s::Track &a = m_tab->tracks[std::size_t(track)];
    const s::Track &b = m_midi->tracks[std::size_t(track)];
    if (a.name == b.name)
        return true;
    // Same position and about the same number of notes (the MIDI export adds slide runs).
    return a.noteCount > 0 && std::abs(a.noteCount - b.noteCount) <= std::max(8, a.noteCount / 5);
}

void SongController::rebuild()
{
    s::Song song = m_tab ? *m_tab : *m_midi;
    if (m_tab && m_midi) {
        // Tracks from the MIDI file on the tab's timeline (both have the repeats played out).
        const double scale = double(m_tab->tempo.ppq()) / m_midi->tempo.ppq();
        for (std::size_t t = 0; t < song.tracks.size() && t < m_tracks.size(); ++t) {
            if (!m_tracks[t].fromMidi || !lineUp(int(t)))
                continue;
            const s::Track &from = m_midi->tracks[t];
            s::Track &to = song.tracks[t];
            to.events = from.events;
            for (s::Event &e : to.events)
                e.tick = s::Tick(std::llround(double(e.tick) * scale));
            to.noteCount = from.noteCount;
            to.lowestNote = from.lowestNote;
            to.highestNote = from.highestNote;
            // The tab's length stands: the player ends notes still ringing there.
        }
    }
    if (song.tracks.size() > std::size_t(s::SongPlayer::kMaxTracks))
        song.tracks.resize(std::size_t(s::SongPlayer::kMaxTracks));
    m_song = song;
    m_player.load(std::make_unique<s::Song>(std::move(song)));
}

void SongController::applyTrack(int track)
{
    if (track < 0 || track >= int(m_tracks.size()))
        return;
    const TrackSettings &st = m_tracks[std::size_t(track)];
    s::Assignment a;
    if (st.choice > 0 && st.choice <= int(m_choices.size())) {
        const Choice &c = m_choices[std::size_t(st.choice - 1)];
        a = {c.device, c.channel, c.drumMap};
    }
    m_player.setAssignment(track, a);
    m_player.setLevel(track, float(st.level));
    m_player.setMute(track, st.mute);
    m_player.setSolo(track, st.solo);
}

void SongController::suggest()
{
    if (!m_song)
        return;
    std::vector<const s::ScoreTrack *> score;
    if (m_score)
        for (const s::ScoreTrack &t : m_score->tracks)
            score.push_back(&t);
    const auto choiceOf = [this](const QString &key) {
        for (std::size_t i = 0; i < m_choices.size(); ++i)
            if (m_choices[i].key == key)
                return int(i) + 1;
        return 0;
    };
    // Polysynth parts in order: Kalliope, then Krimson's slots.
    const QStringList poly{QStringLiteral("kalliope"), QStringLiteral("krimson:A"), QStringLiteral("krimson:B"),
                           QStringLiteral("krimson:C"), QStringLiteral("krimson:D")};
    const auto suggested = s::suggestInstruments(*m_song, score);
    for (std::size_t t = 0; t < m_tracks.size() && t < suggested.size(); ++t) {
        switch (suggested[t].kind) {
        case s::Suggestion::Kind::Drums: m_tracks[t].choice = choiceOf(QStringLiteral("k909")); break;
        case s::Suggestion::Kind::Bass: m_tracks[t].choice = choiceOf(QStringLiteral("k303")); break;
        case s::Suggestion::Kind::Poly:
            m_tracks[t].choice = suggested[t].part < poly.size() ? choiceOf(poly[suggested[t].part]) : 0;
            break;
        case s::Suggestion::Kind::None: break;
        }
        if (m_myTrack < 0 && s::guessRole(m_song->tracks[t], t < score.size() ? score[t] : nullptr) == s::TrackRole::Guitar)
            m_myTrack = int(t); // the first guitar is probably yours
    }
}

// --- Track settings ----------------------------------------------------------------------

void SongController::setInstrument(int track, int choice)
{
    if (track < 0 || track >= int(m_tracks.size()) || choice < 0 || choice > int(m_choices.size()))
        return;
    m_tracks[std::size_t(track)].choice = choice;
    applyTrack(track);
    emit tracksChanged();
    saveSettings();
}

void SongController::setLevel(int track, qreal level)
{
    if (track < 0 || track >= int(m_tracks.size()))
        return;
    m_tracks[std::size_t(track)].level = std::clamp(double(level), 0.0, 1.0);
    applyTrack(track);
    emit tracksChanged();
    saveSettings();
}

void SongController::setMute(int track, bool mute)
{
    if (track < 0 || track >= int(m_tracks.size()))
        return;
    m_tracks[std::size_t(track)].mute = mute;
    applyTrack(track);
    emit tracksChanged();
    saveSettings();
}

void SongController::setSolo(int track, bool solo)
{
    if (track < 0 || track >= int(m_tracks.size()))
        return;
    m_tracks[std::size_t(track)].solo = solo;
    applyTrack(track);
    emit tracksChanged();
    saveSettings();
}

void SongController::setFromMidi(int track, bool fromMidi)
{
    if (track < 0 || track >= int(m_tracks.size()) || !m_tab || !lineUp(track)
        || m_tracks[std::size_t(track)].fromMidi == fromMidi)
        return;
    m_tracks[std::size_t(track)].fromMidi = fromMidi;
    const s::Tick at = m_player.position();
    rebuild(); // assignments are per track and stay
    m_player.cue(at);
    emit tracksChanged();
    saveSettings();
}

void SongController::backToStart()
{
    m_player.cue(0);
    m_bar = m_beat = 1;
    pollPosition();
}

void SongController::jumpToBar(int writtenBar)
{
    if (!m_score || m_score->playOrder.empty())
        return;
    const auto &order = m_score->playOrder;
    const auto &starts = m_score->playStarts;
    const auto now = std::size_t(std::upper_bound(starts.begin(), starts.end(), m_player.position()) - starts.begin());
    std::size_t pick = order.size();
    for (std::size_t k = now; k < order.size() && pick == order.size(); ++k) // its next time through
        if (order[k] == writtenBar)
            pick = k;
    for (std::size_t k = 0; k < order.size() && pick == order.size(); ++k) // else its first
        if (order[k] == writtenBar)
            pick = k;
    if (pick == order.size())
        return;
    m_player.cue(starts[pick]);
}

void SongController::pollPosition()
{
    if (!m_retiredClips.empty() && m_backing) // the audio thread has moved on from old clips
        std::erase_if(m_retiredClips, [this](const RetiredClip &r) { return m_backing->blocksProcessed() >= r.after + 2; });
    if (!m_song)
        return;
    if (m_player.ended() && m_rack.playing()) { // the end: stop, ready to play again from the top
        m_rack.setPlaying(false);
        m_player.cue(0);
    }
    const s::Tick tick = std::min(m_player.position(), m_song->length);
    if (tick != m_tick) {
        m_tick = tick;
        emit tickChanged();
    }
    const s::BarPosition p = m_song->tempo.barAt(tick);
    const int bar = p.bar + 1;
    const int beat = int(p.beat) + 1;
    const QString time = clock(m_song->tempo.secondsAt(tick)) + QStringLiteral(" / ") + clock(m_song->tempo.secondsAt(m_song->length));
    if (bar != m_bar || beat != m_beat || time != m_time) {
        m_bar = bar;
        m_beat = beat;
        m_time = time;
        emit positionChanged();
    }
}

void SongController::setSequencers(bool enabled)
{
    for (krate::InstrumentInstance *instance : m_instances)
        if (instance)
            instance->instrument().setSequencerSuspended(!enabled);
}

// --- Backing track -------------------------------------------------------------------

QString SongController::backingName() const
{
    return m_backingPath.isEmpty() ? QString() : QFileInfo(m_backingPath).fileName();
}

qreal SongController::beatSeconds() const
{
    return m_song ? 60.0 / m_song->tempo.bpmAt(0) : 0.5;
}

void SongController::setBackingOffset(qreal seconds)
{
    if (!m_song)
        return;
    seconds = std::round(seconds * 1000.0) / 1000.0; // to the millisecond
    if (seconds == m_backingOffset)
        return;
    m_backingOffset = seconds;
    if (m_backing)
        m_backing->setOffset(seconds);
    if (m_songId)
        m_library.setBackingOffset(m_songId, seconds);
    emit backingChanged();
}

void SongController::loadBacking(const QUrl &url)
{
    if (!m_song || !m_songId)
        return;
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (!path.endsWith(u".mp3", Qt::CaseInsensitive)) {
        setError(QStringLiteral("%1: a backing track is an MP3 file").arg(QFileInfo(path).fileName()));
        return;
    }
    const QString copy = m_library.setBacking(m_songId, path);
    if (copy.isEmpty()) {
        setError(m_library.lastError());
        return;
    }
    setError({});
    startBacking(copy);
}

void SongController::clearBacking()
{
    if (m_songId)
        m_library.clearBacking(m_songId);
    m_backingOffset = 0.0;
    if (m_backing)
        m_backing->setOffset(0.0);
    startBacking({});
}

void SongController::startBacking(const QString &path)
{
    const int generation = ++m_backingGeneration;
    useBacking(nullptr, {}); // the old one stops now
    m_backingPath = path;
    if (path.isEmpty()) {
        emit backingChanged();
        return;
    }
    // Decoding a whole song takes a moment: off the GUI thread.
    ++m_backingLoading;
    emit backingChanged();
    QPointer<SongController> self(this);
    QThread *reader = QThread::create([self, path, generation] {
        auto clip = std::make_unique<s::AudioClip>();
        std::string error;
        const bool ok = s::decodeMp3(path.toStdString(), clip.get(), &error);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(), // back on the GUI thread
            [self, path, generation, ok, error, clip = std::shared_ptr<s::AudioClip>(clip.release())]() mutable {
                if (!self)
                    return;
                --self->m_backingLoading;
                if (generation != self->m_backingGeneration) { // a newer one was asked for meanwhile
                    emit self->backingChanged();
                    return;
                }
                if (!ok) {
                    self->setError(QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), QString::fromStdString(error)));
                    self->m_backingPath.clear();
                    emit self->backingChanged();
                    return;
                }
                self->useBacking(std::make_unique<s::AudioClip>(std::move(*clip)), path);
            },
            Qt::QueuedConnection);
    });
    m_readers.push_back(reader);
    connect(reader, &QThread::finished, this, [this, reader] {
        std::erase(m_readers, reader);
        reader->deleteLater();
    });
    reader->start();
}

void SongController::useBacking(std::unique_ptr<s::AudioClip> clip, const QString &path)
{
    if (m_backing)
        m_backing->setClip(clip.get());
    const std::uint64_t blocks = m_backing ? m_backing->blocksProcessed() : 0;
    if (m_backingClip) // the audio thread may still be reading it: free it two blocks on
        m_retiredClips.push_back({std::move(m_backingClip), blocks});
    m_backingClip = std::move(clip);
    if (!path.isEmpty())
        m_backingPath = path;
    std::erase_if(m_retiredClips, [this](const RetiredClip &r) {
        return !m_backing || m_backing->blocksProcessed() >= r.after + 2;
    });
    emit backingChanged();
}

SongController::Snapshot SongController::capture() const
{
    Snapshot s;
    for (std::size_t i = 0; i < m_instances.size() && i < std::size_t(m_ids.size()); ++i)
        if (m_instances[i])
            s.sounds[m_ids[qsizetype(i)]] = m_instances[i]->saveSound();
    s.mixer = m_rack.mixerState();
    return s;
}

void SongController::restore(const Snapshot &snapshot)
{
    for (std::size_t i = 0; i < m_instances.size() && i < std::size_t(m_ids.size()); ++i)
        if (m_instances[i] && snapshot.sounds.contains(m_ids[qsizetype(i)]))
            m_instances[i]->loadSound(snapshot.sounds.value(m_ids[qsizetype(i)]).toObject());
    if (!snapshot.mixer.isEmpty())
        m_rack.setMixerState(snapshot.mixer);
}

void SongController::restoreOwned()
{
    if (m_before) {
        Snapshot owned;
        for (const QString &id : std::as_const(m_ownedSounds))
            if (m_before->sounds.contains(id))
                owned.sounds.insert(id, m_before->sounds.value(id));
        if (m_ownsMixer)
            owned.mixer = m_before->mixer;
        restore(owned);
    }
    m_before.reset();
    m_ownedSounds.clear();
    m_ownsMixer = false;
}

void SongController::saveSounds()
{
    if (!m_songId || !m_song)
        return;
    const Snapshot now = capture();
    m_library.saveSounds(m_songId, now.sounds, now.mixer);
    m_ownedSounds = now.sounds.keys(); // the song now has a sound for every instrument
    m_ownsMixer = true;
    m_soundsSaved = true;
    emit songChanged();
    ++m_libraryRevision;
    emit libraryChanged();
}

QVariantList SongController::libraryRows(const QString &search) const
{
    QVariantList rows;
    QString artist, album;
    bool first = true;
    for (const SongLibrary::Song &s : m_library.songs(search)) {
        const QString a = s.details.artist.isEmpty() ? tr("Unknown artist") : s.details.artist;
        const QString b = s.details.album.isEmpty() ? tr("Unknown album") : s.details.album;
        if (first || a.compare(artist, Qt::CaseInsensitive) != 0) {
            rows << QVariantMap{{QStringLiteral("kind"), QStringLiteral("artist")}, {QStringLiteral("text"), a}};
            artist = a;
            album.clear();
            first = false;
        }
        if (album.isNull() || b.compare(album, Qt::CaseInsensitive) != 0) {
            rows << QVariantMap{{QStringLiteral("kind"), QStringLiteral("album")}, {QStringLiteral("text"), b}};
            album = b;
        }
        rows << QVariantMap{{QStringLiteral("kind"), QStringLiteral("song")},
                            {QStringLiteral("text"), s.details.title.isEmpty() ? tr("Untitled") : s.details.title},
                            {QStringLiteral("id"), s.id},
                            {QStringLiteral("track"), s.details.trackNumber},
                            {QStringLiteral("soundsSaved"), s.soundsSaved},
                            {QStringLiteral("open"), s.id == m_songId}};
    }
    return rows;
}

void SongController::removeFromLibrary(qint64 id)
{
    if (id == m_songId)
        close();
    if (m_library.remove(id)) {
        ++m_libraryRevision;
        emit libraryChanged();
    }
}

// --- The library ---------------------------------------------------------------------

void SongController::loadSettings()
{
    if (!m_songId)
        return;
    const QList<SongLibrary::TrackSetup> setup = m_library.setup(m_songId);
    if (setup.isEmpty())
        return; // first time: the suggestions stand
    m_myTrack = m_library.myTrack(m_songId);
    if (m_myTrack >= int(m_tracks.size()))
        m_myTrack = -1;
    for (qsizetype t = 0; t < setup.size() && t < qsizetype(m_tracks.size()); ++t) {
        const SongLibrary::TrackSetup &saved = setup[t];
        TrackSettings &st = m_tracks[std::size_t(t)];
        st.choice = 0;
        for (std::size_t i = 0; i < m_choices.size(); ++i)
            if (m_choices[i].key == saved.instrument)
                st.choice = int(i) + 1;
        st.level = std::clamp(saved.level, 0.0, 1.0);
        st.mute = saved.mute;
        st.solo = saved.solo;
        st.fromMidi = saved.fromMidi && m_tab && lineUp(int(t));
    }
}

void SongController::saveSettings() const
{
    if (!m_songId || !m_song)
        return;
    QList<SongLibrary::TrackSetup> setup;
    for (const TrackSettings &st : m_tracks)
        setup << SongLibrary::TrackSetup{st.choice > 0 ? m_choices[std::size_t(st.choice - 1)].key : QString(), st.level,
                                         st.mute, st.solo, st.fromMidi};
    m_library.saveSetup(m_songId, m_myTrack, setup);
    m_library.setLastSong(m_songId);
}

void SongController::importSongsJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    const QJsonObject last = root.value(u"last").toObject();
    const QString tab = last.value(u"tab").toString(), midi = last.value(u"midi").toString();
    QStringList paths;
    for (const QString &p : {tab, midi})
        if (!p.isEmpty() && QFileInfo::exists(p))
            paths << p;
    if (!paths.isEmpty()) {
        openPaths(paths);
        const QJsonObject saved = root.value(u"songs").toObject().value(tab.isEmpty() ? midi : tab).toObject();
        if (m_song && !saved.isEmpty()) {
            const QJsonArray tracks = saved.value(u"tracks").toArray();
            for (qsizetype t = 0; t < tracks.size() && t < qsizetype(m_tracks.size()); ++t) {
                const QJsonObject o = tracks[t].toObject();
                TrackSettings &st = m_tracks[std::size_t(t)];
                st.choice = 0;
                for (std::size_t i = 0; i < m_choices.size(); ++i)
                    if (m_choices[i].key == o.value(u"instrument").toString())
                        st.choice = int(i) + 1;
                st.level = std::clamp(o.value(u"level").toDouble(1.0), 0.0, 1.0);
                st.mute = o.value(u"mute").toBool();
                st.solo = o.value(u"solo").toBool();
                applyTrack(int(t));
            }
            m_myTrack = std::clamp(saved.value(u"myTrack").toInt(-1), -1, int(m_tracks.size()) - 1);
            m_viewTrack = m_myTrack;
            emit tracksChanged();
            saveSettings();
        }
    }
    QFile::remove(path + QStringLiteral(".old"));
    QFile::rename(path, path + QStringLiteral(".old"));
}

void SongController::reopenLast()
{
    if (const qint64 id = m_library.lastSong())
        openSong(id);
}
