#include "SequencerController.h"

#include <algorithm>

#include <QJSEngine>

#include "RackRunControl.h"
#include "app/JsonFile.h"

using namespace krate::seq;

namespace {

constexpr int kSaveDelayMs = 1000;
constexpr int kPollIntervalMs = 30;
constexpr int kFileVersion = 1;

// K909's voices, top to bottom, each with the note that plays it (k909::Engine::drumForNote).
struct DrumLane
{
    const char *name;
    int note;
};
constexpr DrumLane kDrumLanes[] = {
    {"Bass drum", 36}, {"Snare", 38},   {"Low tom", 41},   {"Mid tom", 45},  {"High tom", 48}, {"Rim shot", 37},
    {"Clap", 39},      {"Closed hat", 42}, {"Open hat", 46}, {"Crash", 49},   {"Ride", 51},     {"Metal", 56},
    {"Clank", 54},     {"Noise", 52},   {"Boom", 34},      {"Sample", 60},
};

} // namespace

SequencerController *SequencerController::s_instance = nullptr;

SequencerController::SequencerController(Sequencer &sequencer, std::vector<NoteTarget> targets, QString path,
                                         QObject *parent)
    : QObject(parent)
    , m_sequencer(sequencer)
    , m_targets(std::move(targets))
    , m_path(std::move(path))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &SequencerController::save);
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &SequencerController::poll);
    m_pollTimer.start();
}

SequencerController::~SequencerController()
{
    flush();
    s_instance = nullptr;
}

SequencerController *SequencerController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void SequencerController::load()
{
    QJsonObject json;
    const krate::files::LoadResult result = krate::files::readVersionedJson(m_path, kFileVersion, &json);
    if (result == krate::files::LoadResult::Loaded)
        m_state = sequencerFromJson(json);
    m_readOnly = result == krate::files::LoadResult::NewerVersion; // a newer Krate's file: leave it be
    applyTargets();
    for (int t = 0; t < kTracks; ++t)
        m_sequencer.setMute(t, m_state.mutes[std::size_t(t)]);
    m_sequencer.setRunning(m_state.running);
    m_sequencer.setChainMode(m_state.chainMode);
    m_sequencer.cue(m_state.pattern);
    m_sequencer.setBank(std::make_unique<Bank>(*m_state.bank));
    ++m_revision;
    emit changed();
}

void SequencerController::flush()
{
    if (m_dirty)
        save();
}

// --- Properties -----------------------------------------------------------------------

void SequencerController::setPattern(int pattern)
{
    pattern = std::clamp(pattern, 0, kPatterns - 1);
    if (pattern == m_state.pattern)
        return;
    m_state.pattern = pattern;
    m_sequencer.cue(pattern);
    m_dirty = true;
    m_saveTimer.start();
    ++m_revision;
    emit changed();
}

void SequencerController::setTrack(int track)
{
    track = std::clamp(track, 0, kTracks - 1);
    if (track == m_track)
        return;
    m_track = track;
    ++m_revision;
    emit changed();
}

void SequencerController::setRunning(bool on)
{
    if (on == m_state.running)
        return;
    m_state.running = on;
    m_sequencer.setRunning(on);
    m_dirty = true;
    m_saveTimer.start();
    ++m_revision;
    emit changed();
}

void SequencerController::toggleRun()
{
    if (!m_run) {
        setRunning(!m_state.running);
        return;
    }
    if (!m_run->transportPlaying()) { // from silence: play, the sequencer alone
        setRunning(true);
        m_run->playWithoutInstruments();
    } else if (m_state.running) { // stop it; the rack too when nothing else runs
        setRunning(false);
        if (!m_run->anyInstrumentRunning())
            m_run->stopTransport();
    } else { // the rack is playing: join it
        setRunning(true);
    }
}

int SequencerController::length() const { return current().length; }
int SequencerController::grid() const { return current().grid; }
int SequencerController::gridTicks() const { return current().gridTicks(); }

void SequencerController::setGrid(int grid)
{
    grid = std::clamp(grid, 0, int(kGridTicks.size()) - 1);
    if (grid == current().grid)
        return;
    current().grid = std::uint8_t(grid);
    edited();
}

QStringList SequencerController::gridNames() const
{
    return {QStringLiteral("1/16"), QStringLiteral("1/32"), QStringLiteral("1/16T"), QStringLiteral("1/8T")};
}

QStringList SequencerController::targetChoices() const
{
    QStringList list{QStringLiteral("None")};
    for (const NoteTarget &t : m_targets)
        list << t.label;
    return list;
}

int SequencerController::choiceOf(int track) const
{
    const QString &key = m_state.targets[std::size_t(track)];
    for (std::size_t i = 0; i < m_targets.size(); ++i)
        if (m_targets[i].key == key)
            return int(i) + 1;
    return 0;
}

QVariantList SequencerController::tracks() const
{
    QVariantList list;
    for (int t = 0; t < kTracks; ++t) {
        const int choice = choiceOf(t);
        list << QVariantMap{{QStringLiteral("choice"), choice},
                            {QStringLiteral("mute"), m_state.mutes[std::size_t(t)]},
                            {QStringLiteral("notes"), int(current().tracks[std::size_t(t)].count)},
                            {QStringLiteral("label"), choice > 0 ? m_targets[std::size_t(choice - 1)].label : QString()},
                            {QStringLiteral("drums"), choice > 0 && m_targets[std::size_t(choice - 1)].drumMap}};
    }
    return list;
}

QVariantList SequencerController::notes() const
{
    QVariantList list;
    const TrackNotes &tn = current().tracks[std::size_t(m_track)];
    for (int n = 0; n < tn.count; ++n) {
        const Note &note = tn.notes[std::size_t(n)];
        list << QVariantMap{{QStringLiteral("start"), int(note.start)},
                            {QStringLiteral("length"), int(note.length)},
                            {QStringLiteral("pitch"), int(note.pitch)},
                            {QStringLiteral("velocity"), int(note.velocity)}};
    }
    return list;
}

QVariantList SequencerController::patternsUsed() const
{
    QVariantList list;
    for (const Pattern &p : m_state.bank->patterns)
        list << !p.empty();
    return list;
}

QVariantList SequencerController::drumLanes() const
{
    QVariantList list;
    for (const DrumLane &lane : kDrumLanes)
        list << QVariantMap{{QStringLiteral("name"), QString::fromLatin1(lane.name)}, {QStringLiteral("note"), lane.note}};
    return list;
}

// --- Edits ----------------------------------------------------------------------------

void SequencerController::setTarget(int track, int choice)
{
    if (track < 0 || track >= kTracks || choice < 0 || choice > int(m_targets.size()))
        return;
    m_state.targets[std::size_t(track)] = choice > 0 ? m_targets[std::size_t(choice - 1)].key : QString();
    applyTargets();
    m_dirty = true;
    m_saveTimer.start();
    ++m_revision;
    emit changed();
}

void SequencerController::setMute(int track, bool mute)
{
    if (track < 0 || track >= kTracks)
        return;
    m_state.mutes[std::size_t(track)] = mute;
    m_sequencer.setMute(track, mute);
    m_dirty = true;
    m_saveTimer.start();
    ++m_revision;
    emit changed();
}

void SequencerController::setLength(int ticks)
{
    const int grid = current().gridTicks();
    const int steps = std::max(1, int(std::lround(double(ticks) / grid)));
    const int length = std::min(steps * grid, kMaxTicks);
    if (length == current().length)
        return;
    current().length = std::uint16_t(length);
    edited();
}

int SequencerController::addNote(int start, int pitch, int length, int velocity)
{
    TrackNotes &tn = currentNotes();
    if (start < 0 || start >= current().length || pitch < 0 || pitch > 127)
        return -1;
    if (!tn.add({std::uint16_t(start), std::uint16_t(std::clamp(length, 1, kMaxTicks)), std::uint8_t(pitch),
                 std::uint8_t(std::clamp(velocity, 1, 127))}))
        return -1;
    edited();
    return tn.count - 1;
}

void SequencerController::moveNote(int index, int start, int pitch)
{
    TrackNotes &tn = currentNotes();
    if (index < 0 || index >= tn.count)
        return;
    Note &n = tn.notes[std::size_t(index)];
    const auto s = std::uint16_t(std::clamp(start, 0, current().length - 1));
    const auto p = std::uint8_t(std::clamp(pitch, 0, 127));
    if (s == n.start && p == n.pitch)
        return;
    n.start = s;
    n.pitch = p;
    edited();
}

void SequencerController::resizeNote(int index, int length)
{
    TrackNotes &tn = currentNotes();
    if (index < 0 || index >= tn.count)
        return;
    const auto l = std::uint16_t(std::clamp(length, 1, kMaxTicks));
    if (l == tn.notes[std::size_t(index)].length)
        return;
    tn.notes[std::size_t(index)].length = l;
    edited();
}

void SequencerController::setVelocity(int index, int velocity)
{
    TrackNotes &tn = currentNotes();
    if (index < 0 || index >= tn.count)
        return;
    const auto v = std::uint8_t(std::clamp(velocity, 1, 127));
    if (v == tn.notes[std::size_t(index)].velocity)
        return;
    tn.notes[std::size_t(index)].velocity = v;
    edited();
}

void SequencerController::deleteNote(int index)
{
    TrackNotes &tn = currentNotes();
    if (index < 0 || index >= tn.count)
        return;
    std::copy(tn.notes.begin() + index + 1, tn.notes.begin() + tn.count, tn.notes.begin() + index);
    --tn.count;
    edited();
}

void SequencerController::clearTrack()
{
    if (currentNotes().count == 0)
        return;
    currentNotes().count = 0;
    edited();
}

void SequencerController::clearPattern()
{
    current() = Pattern{};
    edited();
}

void SequencerController::copyPattern()
{
    m_clipboard = current();
    ++m_revision;
    emit changed();
}

void SequencerController::pastePattern()
{
    if (!m_clipboard)
        return;
    current() = *m_clipboard;
    edited();
}

int SequencerController::importChunks(int firstPattern, int track, const std::vector<songimport::Chunk> &chunks,
                                      int choice)
{
    if (track < 0 || track >= kTracks || firstPattern < 0)
        return 0;
    int written = 0;
    for (std::size_t i = 0; i < chunks.size() && firstPattern + int(i) < kPatterns; ++i) {
        Pattern &p = m_state.bank->patterns[std::size_t(firstPattern) + i];
        const songimport::Chunk &chunk = chunks[i];
        p.length = std::uint16_t(std::clamp<int>(chunk.length, 1, kMaxTicks));
        p.grid = std::uint8_t(std::min<int>(chunk.grid, int(kGridTicks.size()) - 1));
        TrackNotes &tn = p.tracks[std::size_t(track)];
        tn.count = 0;
        for (const Note &n : chunk.notes)
            if (n.start < p.length)
                tn.add(n);
        ++written;
    }
    if (choice > 0 && choice <= int(m_targets.size())) {
        m_state.targets[std::size_t(track)] = m_targets[std::size_t(choice - 1)].key;
        applyTargets();
    }
    m_state.pattern = std::clamp(firstPattern, 0, kPatterns - 1);
    m_sequencer.cue(m_state.pattern);
    m_track = track;
    edited();
    return written;
}

QList<int> SequencerController::patternsWithNotes(int firstPattern, int count, int track) const
{
    QList<int> list;
    if (track < 0 || track >= kTracks)
        return list;
    for (int p = std::max(firstPattern, 0); p < std::min(firstPattern + count, kPatterns); ++p)
        if (m_state.bank->patterns[std::size_t(p)].tracks[std::size_t(track)].count > 0)
            list << p;
    return list;
}

void SequencerController::setChainMode(bool on)
{
    if (on == m_state.chainMode)
        return;
    m_state.chainMode = on;
    m_sequencer.setChainMode(on);
    m_dirty = true;
    m_saveTimer.start();
    ++m_revision;
    emit changed();
}

QVariantList SequencerController::chain() const
{
    QVariantList list;
    for (int i = 0; i < m_state.bank->chainLength; ++i)
        list << QVariantMap{{QStringLiteral("pattern"), int(m_state.bank->chain[std::size_t(i)].pattern)},
                            {QStringLiteral("repeats"), int(m_state.bank->chain[std::size_t(i)].repeats)}};
    return list;
}

void SequencerController::addToChain(int pattern, int after)
{
    Bank &bank = *m_state.bank;
    if (pattern < 0 || pattern >= kPatterns || bank.chainLength >= kMaxChain)
        return;
    const int at = after < 0 || after >= bank.chainLength ? bank.chainLength : after + 1;
    std::copy_backward(bank.chain.begin() + at, bank.chain.begin() + bank.chainLength,
                       bank.chain.begin() + bank.chainLength + 1);
    bank.chain[std::size_t(at)] = {std::uint8_t(pattern), 1};
    ++bank.chainLength;
    edited();
}

void SequencerController::removeChainStep(int step)
{
    Bank &bank = *m_state.bank;
    if (step < 0 || step >= bank.chainLength)
        return;
    std::copy(bank.chain.begin() + step + 1, bank.chain.begin() + bank.chainLength, bank.chain.begin() + step);
    --bank.chainLength;
    edited();
}

void SequencerController::setChainRepeats(int step, int repeats)
{
    Bank &bank = *m_state.bank;
    if (step < 0 || step >= bank.chainLength)
        return;
    const auto r = std::uint8_t(std::clamp(repeats, 1, kMaxRepeats));
    if (r == bank.chain[std::size_t(step)].repeats)
        return;
    bank.chain[std::size_t(step)].repeats = r;
    edited();
}

void SequencerController::clearChain()
{
    if (m_state.bank->chainLength == 0)
        return;
    m_state.bank->chainLength = 0;
    edited();
}

void SequencerController::stepPattern(int delta)
{
    setPattern((m_state.pattern + delta + kPatterns) % kPatterns);
}

// --- Internals ------------------------------------------------------------------------

void SequencerController::applyTargets()
{
    for (int t = 0; t < kTracks; ++t) {
        const int choice = choiceOf(t);
        if (choice > 0) {
            const NoteTarget &target = m_targets[std::size_t(choice - 1)];
            m_sequencer.setTarget(t, {target.device, target.channel});
        } else {
            m_sequencer.setTarget(t, {});
        }
    }
}

void SequencerController::edited()
{
    m_sequencer.setBank(std::make_unique<Bank>(*m_state.bank));
    m_dirty = true;
    m_saveTimer.start();
    ++m_revision;
    emit changed();
}

void SequencerController::save()
{
    m_saveTimer.stop();
    m_dirty = false;
    if (!m_readOnly)
        krate::files::writeJsonFile(m_path, sequencerToJson(m_state));
}

void SequencerController::poll()
{
    m_sequencer.collectGarbage();
    const bool playing = m_sequencer.playing();
    const int pattern = m_sequencer.playingPattern();
    const int tick = m_sequencer.tick();
    const int step = m_sequencer.chainStep(), pass = m_sequencer.chainPass();
    if (playing == m_playing && pattern == m_playingPattern && tick == m_tick && step == m_chainStep && pass == m_chainPass)
        return;
    m_playing = playing;
    m_playingPattern = pattern;
    m_tick = tick;
    m_chainStep = step;
    m_chainPass = pass;
    emit positionChanged();
}
