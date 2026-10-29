#include "SongController.h"

#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>

#include "RackController.h"
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
                               std::vector<krate::IInstrument *> instruments, const QString &settingsPath,
                               QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_rack(rack)
    , m_instruments(std::move(instruments))
    , m_settingsPath(settingsPath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Every instrument that plays notes; one choice per channel for
    // instruments with several (Krimson's slots A-D). The metronome plays none.
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const krate::InstrumentEntry &e = entries[i];
        if (e.id == u"klick")
            continue;
        if (e.extraMidiChannels.isEmpty()) {
            m_choices.push_back({e.name, e.id, int(i), std::max(0, e.defaultMidiChannel), e.id == u"k909"});
            continue;
        }
        QList<int> channels{e.defaultMidiChannel};
        channels += e.extraMidiChannels;
        for (qsizetype k = 0; k < channels.size(); ++k) {
            const QChar slot(char16_t(u'A' + k));
            m_choices.push_back({e.name + u' ' + slot, e.id + u':' + slot, int(i), std::max(0, channels[k]), false});
        }
    }

    m_timer.setInterval(kPollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &SongController::pollPosition);
    m_timer.start();
}

SongController::~SongController()
{
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
    if (m_score && !m_score->title.empty())
        return QString::fromStdString(m_score->title);
    if (m_midi && !m_midi->title.empty())
        return QString::fromStdString(m_midi->title);
    const QString path = m_tabPath.isEmpty() ? m_midiPath : m_tabPath;
    return path.isEmpty() ? QString() : QFileInfo(path).completeBaseName();
}

QString SongController::artist() const { return m_score ? QString::fromStdString(m_score->artist) : QString(); }

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
        };
    }
    return out;
}

void SongController::setMyTrack(int track)
{
    if (!m_song || track < -1 || track >= int(m_song->tracks.size()) || track == m_myTrack)
        return;
    m_myTrack = track;
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

    std::optional<s::Score> score;
    std::optional<s::Song> tab, midi;
    QStringList problems;
    if (!tabPath.isEmpty()) {
        auto result = s::readGpFile(tabPath.toStdString());
        if (result.song) {
            score = std::move(result.score);
            tab = std::move(result.song);
        } else {
            problems << QFileInfo(tabPath).fileName() + QStringLiteral(": ") + QString::fromStdString(result.error);
            tabPath.clear();
        }
    }
    if (!midiPath.isEmpty()) {
        auto result = s::readMidiFile(midiPath.toStdString());
        if (result.song) {
            midi = std::move(result.song);
        } else {
            problems << QFileInfo(midiPath).fileName() + QStringLiteral(": ") + QString::fromStdString(result.error);
            midiPath.clear();
        }
    }
    setError(problems.join(u'\n'));
    if (!tab && !midi)
        return; // the song that was open stays

    m_rack.setPlaying(false);
    m_score = std::move(score);
    m_tab = std::move(tab);
    m_midi = std::move(midi);
    m_tabPath = tabPath.isEmpty() ? QString() : QFileInfo(tabPath).absoluteFilePath();
    m_midiPath = midiPath.isEmpty() ? QString() : QFileInfo(midiPath).absoluteFilePath();
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
    m_bar = m_beat = 1;
    pollPosition();
    emit songChanged();
    emit tracksChanged();
    saveSettings();
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
    setSequencers(true);
    setError({});
    emit songChanged();
    emit tracksChanged();
    saveSettings(); // no last song
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

void SongController::pollPosition()
{
    if (!m_song)
        return;
    if (m_player.ended() && m_rack.playing()) { // the end: stop, ready to play again from the top
        m_rack.setPlaying(false);
        m_player.cue(0);
    }
    const s::Tick tick = std::min(m_player.position(), m_song->length);
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
    for (krate::IInstrument *instrument : m_instruments)
        if (instrument)
            instrument->setSequencerEnabled(enabled);
}

// --- songs.json ----------------------------------------------------------------------------

QString SongController::songKey() const { return m_tabPath.isEmpty() ? m_midiPath : m_tabPath; }

void SongController::loadSettings()
{
    QFile file(m_settingsPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject song = QJsonDocument::fromJson(file.readAll()).object().value(u"songs").toObject().value(songKey()).toObject();
    if (song.isEmpty())
        return; // first time: the suggestions stand
    m_myTrack = song.value(u"myTrack").toInt(-1);
    if (m_myTrack >= int(m_tracks.size()))
        m_myTrack = -1;
    const QJsonArray tracks = song.value(u"tracks").toArray();
    for (qsizetype t = 0; t < tracks.size() && t < qsizetype(m_tracks.size()); ++t) {
        const QJsonObject o = tracks[t].toObject();
        TrackSettings &st = m_tracks[std::size_t(t)];
        const QString key = o.value(u"instrument").toString();
        st.choice = 0;
        for (std::size_t i = 0; i < m_choices.size(); ++i)
            if (m_choices[i].key == key)
                st.choice = int(i) + 1;
        st.level = std::clamp(o.value(u"level").toDouble(1.0), 0.0, 1.0);
        st.mute = o.value(u"mute").toBool(false);
        st.solo = o.value(u"solo").toBool(false);
        st.fromMidi = o.value(u"midi").toBool(st.fromMidi) && m_tab && lineUp(int(t));
    }
}

void SongController::saveSettings() const
{
    QJsonObject root;
    {
        QFile file(m_settingsPath);
        if (file.open(QIODevice::ReadOnly))
            root = QJsonDocument::fromJson(file.readAll()).object();
    }
    root[QStringLiteral("version")] = 1;
    if (m_song) {
        QJsonArray tracks;
        for (const TrackSettings &st : m_tracks)
            tracks.append(QJsonObject{
                {QStringLiteral("instrument"), st.choice > 0 ? m_choices[std::size_t(st.choice - 1)].key : QString()},
                {QStringLiteral("level"), st.level},
                {QStringLiteral("mute"), st.mute},
                {QStringLiteral("solo"), st.solo},
                {QStringLiteral("midi"), st.fromMidi},
            });
        QJsonObject songs = root.value(u"songs").toObject();
        songs[songKey()] = QJsonObject{{QStringLiteral("myTrack"), m_myTrack}, {QStringLiteral("tracks"), tracks}};
        root[QStringLiteral("songs")] = songs;
        root[QStringLiteral("last")] = QJsonObject{{QStringLiteral("tab"), m_tabPath}, {QStringLiteral("midi"), m_midiPath}};
    } else {
        root.remove(QStringLiteral("last"));
    }
    QDir().mkpath(QFileInfo(m_settingsPath).absolutePath());
    QSaveFile out(m_settingsPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(root).toJson());
        out.commit();
    }
}

void SongController::reopenLast()
{
    QFile file(m_settingsPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject last = QJsonDocument::fromJson(file.readAll()).object().value(u"last").toObject();
    QStringList paths;
    for (const QString &key : {QStringLiteral("tab"), QStringLiteral("midi")})
        if (const QString p = last.value(key).toString(); !p.isEmpty() && QFileInfo::exists(p))
            paths << p;
    if (!paths.isEmpty())
        openPaths(paths);
}
