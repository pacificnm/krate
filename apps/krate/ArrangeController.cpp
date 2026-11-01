#include "ArrangeController.h"

#include <algorithm>

#include <QJSEngine>

#include "AudioTracks.h"
#include "SequencerController.h"
#include "core/Transport.h"
#include "rack/MidiCapture.h"

using namespace krate::seq;

namespace {
constexpr int kMaxDrawnNotes = 2000; // per clip
}

ArrangeController::ArrangeController(SequencerController &sequencer, krate::Transport &transport,
                                     AudioTracksController *tracks, QObject *parent)
    : QObject(parent)
    , m_sequencer(sequencer)
    , m_transport(transport)
    , m_tracks(tracks)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    // A sequence opened, a pattern edited: the transport and the lanes follow.
    connect(&m_sequencer, &SequencerController::changed, this, [this] {
        apply();
        ++m_revision;
        emit changed();
    });
    connect(&m_sequencer, &SequencerController::stateReplaced, this, [this] {
        m_undo.clear(); // another sequence: its own history
        m_redo.clear();
        emit historyChanged();
    });
    if (m_tracks) // a recording, the panel, an edit: undoable too
        m_tracks->setBeforeLaneEdit([this] { beforeEdit(); });
    apply();
    m_pollTimer.setInterval(33);
    connect(&m_pollTimer, &QTimer::timeout, this, &ArrangeController::poll);
    m_pollTimer.start();
}

ArrangeController::~ArrangeController()
{
    if (m_tracks)
        m_tracks->setBeforeLaneEdit({});
    s_instance = nullptr;
}

ArrangeController *ArrangeController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool ArrangeController::songMode() const { return m_sequencer.state().songMode; }
int ArrangeController::startBar() const { return m_sequencer.state().startBar; }
bool ArrangeController::loopOn() const { return m_sequencer.state().loopOn; }
int ArrangeController::loopFrom() const { return m_sequencer.state().loopFrom; }
int ArrangeController::loopTo() const { return m_sequencer.state().loopTo; }
bool ArrangeController::punchOn() const { return m_sequencer.state().punchOn; }
int ArrangeController::punchFrom() const { return m_sequencer.state().punchFrom; }
int ArrangeController::punchTo() const { return m_sequencer.state().punchTo; }
int ArrangeController::endBar() const { return m_sequencer.state().arrangement.endBar(); }
int ArrangeController::maxBars() const { return kMaxBars; }

void ArrangeController::setSongMode(bool on)
{
    if (on == songMode())
        return;
    m_sequencer.arrangerState().songMode = on;
    edited(false);
}

void ArrangeController::setStartBar(int bar)
{
    bar = std::clamp(bar, 0, kMaxBars - 1);
    if (bar == startBar())
        return;
    m_sequencer.arrangerState().startBar = bar;
    edited(false);
}

void ArrangeController::setLoopOn(bool on)
{
    if (on == loopOn())
        return;
    m_sequencer.arrangerState().loopOn = on;
    edited(false);
}

void ArrangeController::setPunchOn(bool on)
{
    if (on == punchOn())
        return;
    m_sequencer.arrangerState().punchOn = on;
    edited(false);
}

void ArrangeController::setLoop(int from, int to)
{
    if (from > to)
        std::swap(from, to);
    from = std::clamp(from, 0, kMaxBars - 1);
    to = std::clamp(to, from + 1, kMaxBars);
    SequencerState &state = m_sequencer.arrangerState();
    state.loopFrom = from;
    state.loopTo = to;
    edited(false);
}

void ArrangeController::setPunch(int from, int to)
{
    if (from > to)
        std::swap(from, to);
    from = std::clamp(from, 0, kMaxBars - 1);
    to = std::clamp(to, from + 1, kMaxBars);
    SequencerState &state = m_sequencer.arrangerState();
    state.punchFrom = from;
    state.punchTo = to;
    edited(false);
}

QVariantList ArrangeController::lane(int track) const
{
    QVariantList list;
    if (track < 0 || track >= kTracks)
        return list;
    for (const Clip &c : m_sequencer.state().arrangement.lanes[std::size_t(track)])
        list.append(QVariantMap{{QStringLiteral("startBar"), c.startBar},
                                {QStringLiteral("bars"), c.bars},
                                {QStringLiteral("pattern"), c.pattern},
                                {QStringLiteral("midi"), c.midi}});
    return list;
}

int ArrangeController::clipAt(int track, int bar) const
{
    if (track < 0 || track >= kTracks)
        return -1;
    const auto &lane = m_sequencer.state().arrangement.lanes[std::size_t(track)];
    for (std::size_t i = 0; i < lane.size(); ++i)
        if (bar >= lane[i].startBar && bar < lane[i].endBar())
            return int(i);
    return -1;
}

int ArrangeController::patternBars(int pattern) const
{
    if (pattern < 0 || pattern >= kPatterns)
        return 1;
    const int ticksPerBar = kBeatsPerBar * kTicksPerBeat;
    return std::max(1, (int(m_sequencer.state().bank->patterns[std::size_t(pattern)].length) + ticksPerBar - 1) / ticksPerBar);
}

bool ArrangeController::placePattern(int track, int bar, int pattern, int bars)
{
    if (track < 0 || track >= kTracks || pattern < 0 || pattern >= kPatterns || bar < 0 || bar >= kMaxBars
        || clipAt(track, bar) >= 0)
        return false;
    beforeEdit();
    auto &lane = m_sequencer.arrangerState().arrangement.lanes[std::size_t(track)];
    const auto next = std::find_if(lane.begin(), lane.end(), [bar](const Clip &c) { return c.startBar > bar; });
    const int room = (next == lane.end() ? kMaxBars : next->startBar) - bar;
    const int length = std::min(bars > 0 ? bars : patternBars(pattern), room);
    lane.insert(next, Clip{bar, length, pattern, -1});
    edited(true);
    return true;
}

void ArrangeController::removeClip(int track, int index)
{
    if (track < 0 || track >= kTracks)
        return;
    Arrangement &song = m_sequencer.arrangerState().arrangement;
    auto &lane = song.lanes[std::size_t(track)];
    if (index < 0 || index >= int(lane.size()))
        return;
    beforeEdit();
    const int midi = lane[std::size_t(index)].midi;
    lane.erase(lane.begin() + index);
    if (midi >= 0) { // its notes go too; later MIDI clips move down one
        song.midi.erase(song.midi.begin() + midi);
        for (auto &l : song.lanes)
            for (Clip &c : l)
                if (c.midi > midi)
                    --c.midi;
    }
    edited(true);
}

QVariantList ArrangeController::clipNotes(int track, int index) const
{
    QVariantList list;
    if (track < 0 || track >= kTracks)
        return list;
    const SequencerState &state = m_sequencer.state();
    const auto &lane = state.arrangement.lanes[std::size_t(track)];
    if (index < 0 || index >= int(lane.size()))
        return list;
    const Clip &clip = lane[std::size_t(index)];
    const double clipBeats = double(clip.bars) * kBeatsPerBar;
    const auto add = [&](double x, double w, int pitch) {
        if (x < clipBeats && list.size() < kMaxDrawnNotes)
            list.append(QVariantMap{{QStringLiteral("x"), x},
                                    {QStringLiteral("w"), std::min(w, clipBeats - x)},
                                    {QStringLiteral("pitch"), pitch}});
    };
    if (clip.pattern >= 0) {
        const Pattern &p = state.bank->patterns[std::size_t(clip.pattern)];
        const TrackNotes &notes = p.tracks[std::size_t(track)];
        const double length = double(std::max<int>(p.length, 1)) / kTicksPerBeat;
        for (double pass = 0.0; pass < clipBeats; pass += length)
            for (int n = 0; n < notes.count; ++n) {
                const Note &note = notes.notes[std::size_t(n)];
                add(pass + double(note.start) / kTicksPerBeat, double(note.length) / kTicksPerBeat, note.pitch);
            }
    } else if (clip.midi >= 0 && clip.midi < int(state.arrangement.midi.size())) {
        for (const ClipNote &note : state.arrangement.midi[std::size_t(clip.midi)].notes)
            add(double(note.start) / kTicksPerBeat, double(note.length) / kTicksPerBeat, note.pitch);
    }
    return list;
}

namespace {
// Room on a lane for bars [from, to), leaving out clip `except`.
bool fits(const std::vector<Clip> &lane, int from, int to, int except)
{
    if (from < 0 || to > kMaxBars || to <= from)
        return false;
    for (std::size_t i = 0; i < lane.size(); ++i)
        if (int(i) != except && lane[i].startBar < to && lane[i].endBar() > from)
            return false;
    return true;
}
void insertInOrder(std::vector<Clip> &lane, const Clip &clip)
{
    lane.insert(std::find_if(lane.begin(), lane.end(), [&](const Clip &c) { return c.startBar > clip.startBar; }), clip);
}
} // namespace

bool ArrangeController::moveClip(int track, int index, int toTrack, int startBar)
{
    if (track < 0 || track >= kTracks || toTrack < 0 || toTrack >= kTracks)
        return false;
    Arrangement &song = m_sequencer.arrangerState().arrangement;
    auto &from = song.lanes[std::size_t(track)];
    if (index < 0 || index >= int(from.size()))
        return false;
    Clip clip = from[std::size_t(index)];
    if ((toTrack == track && startBar == clip.startBar)
        || !fits(song.lanes[std::size_t(toTrack)], startBar, startBar + clip.bars, toTrack == track ? index : -1))
        return false;
    beforeEdit();
    from.erase(from.begin() + index);
    clip.startBar = startBar;
    insertInOrder(song.lanes[std::size_t(toTrack)], clip);
    edited(true);
    return true;
}

bool ArrangeController::trimClipStart(int track, int index, int startBar)
{
    if (track < 0 || track >= kTracks)
        return false;
    auto &lane = m_sequencer.arrangerState().arrangement.lanes[std::size_t(track)];
    if (index < 0 || index >= int(lane.size()))
        return false;
    const Clip clip = lane[std::size_t(index)];
    const int offset = clip.offsetBars + startBar - clip.startBar; // no further back than its content's start
    if (startBar == clip.startBar || offset < 0 || startBar >= clip.endBar() || !fits(lane, startBar, clip.endBar(), index))
        return false;
    beforeEdit();
    Clip &c = lane[std::size_t(index)];
    c.bars = clip.endBar() - startBar;
    c.startBar = startBar;
    c.offsetBars = offset;
    edited(true);
    return true;
}

bool ArrangeController::trimClipEnd(int track, int index, int endBar)
{
    if (track < 0 || track >= kTracks)
        return false;
    auto &lane = m_sequencer.arrangerState().arrangement.lanes[std::size_t(track)];
    if (index < 0 || index >= int(lane.size()))
        return false;
    const Clip clip = lane[std::size_t(index)];
    if (endBar == clip.endBar() || !fits(lane, clip.startBar, endBar, index))
        return false;
    beforeEdit();
    lane[std::size_t(index)].bars = endBar - clip.startBar;
    edited(true);
    return true;
}

bool ArrangeController::splitClip(int track, int index, int bar)
{
    if (track < 0 || track >= kTracks)
        return false;
    Arrangement &song = m_sequencer.arrangerState().arrangement;
    auto &lane = song.lanes[std::size_t(track)];
    if (index < 0 || index >= int(lane.size()))
        return false;
    const Clip clip = lane[std::size_t(index)];
    if (bar <= clip.startBar || bar >= clip.endBar())
        return false;
    beforeEdit();
    Clip second = clip;
    second.startBar = bar;
    second.bars = clip.endBar() - bar;
    second.offsetBars = clip.offsetBars + (bar - clip.startBar);
    if (clip.midi >= 0) { // its own copy of the notes, so either can go without the other
        song.midi.push_back(song.midi[std::size_t(clip.midi)]);
        second.midi = int(song.midi.size()) - 1;
    }
    lane[std::size_t(index)].bars = bar - clip.startBar;
    lane.insert(lane.begin() + index + 1, second);
    edited(true);
    return true;
}

bool ArrangeController::duplicateClip(int track, int index)
{
    if (track < 0 || track >= kTracks)
        return false;
    Arrangement &song = m_sequencer.arrangerState().arrangement;
    auto &lane = song.lanes[std::size_t(track)];
    if (index < 0 || index >= int(lane.size()))
        return false;
    Clip copy = lane[std::size_t(index)];
    copy.startBar = copy.endBar();
    if (!fits(lane, copy.startBar, copy.endBar(), -1))
        return false;
    beforeEdit();
    if (copy.midi >= 0) {
        song.midi.push_back(song.midi[std::size_t(copy.midi)]);
        copy.midi = int(song.midi.size()) - 1;
    }
    insertInOrder(lane, copy);
    edited(true);
    return true;
}

ArrangeController::Snapshot ArrangeController::snapshot() const
{
    return {m_sequencer.state().arrangement, m_tracks ? m_tracks->lanes() : AudioTracksController::Lanes{}};
}

void ArrangeController::restore(const Snapshot &s)
{
    m_restoring = true;
    m_sequencer.arrangerState().arrangement = s.song;
    edited(true);
    if (m_tracks)
        m_tracks->setLanes(s.audio);
    m_restoring = false;
}

void ArrangeController::beforeEdit()
{
    if (m_restoring)
        return;
    m_undo.push_back(snapshot());
    if (m_undo.size() > 100)
        m_undo.erase(m_undo.begin());
    m_redo.clear();
    emit historyChanged();
}

void ArrangeController::undo()
{
    if (m_undo.empty())
        return;
    m_redo.push_back(snapshot());
    const Snapshot back = std::move(m_undo.back());
    m_undo.pop_back();
    restore(back);
    emit historyChanged();
}

void ArrangeController::redo()
{
    if (m_redo.empty())
        return;
    m_undo.push_back(snapshot());
    const Snapshot again = std::move(m_redo.back());
    m_redo.pop_back();
    restore(again);
    emit historyChanged();
}

bool ArrangeController::armed(int track) const
{
    return track >= 0 && track < kTracks && m_armed[std::size_t(track)];
}

void ArrangeController::setArmed(int track, bool on)
{
    if (track < 0 || track >= kTracks || on == armed(track))
        return;
    m_armed[std::size_t(track)] = on;
    ++m_revision;
    emit changed();
}

void ArrangeController::setRecording(bool on)
{
    if (on == m_recording)
        return;
    m_recording = on;
    ++m_revision;
    emit changed();
}

void ArrangeController::takeCaptured()
{
    const bool anyArmed = std::find(m_armed.begin(), m_armed.end(), true) != m_armed.end();
    const bool active = songMode() && m_recording && anyArmed && m_transport.playing();
    const SequencerState &state = m_sequencer.state();
    const double loopEnd = state.loopOn ? double(state.loopTo) * kBeatsPerBar : -1.0;
    // A note's end before its start: the loop went round while it was held.
    const auto endOf = [loopEnd](double on, double off) {
        return off > on ? off : loopEnd > on ? loopEnd : on + 1.0 / kTicksPerBeat;
    };
    while (const auto e = m_capture ? m_capture->take() : std::nullopt) {
        if (!active && !m_taking)
            continue; // not recording: dropped
        m_taking = true;
        const int pitch = e->data1 & 0x7F;
        Held &held = m_held[std::size_t(pitch)];
        const bool on = (e->status & 0xF0) == 0x90 && e->data2 > 0;
        if (on) {
            if (state.punchOn && (e->beat < double(state.punchFrom) * kBeatsPerBar
                                  || e->beat >= double(state.punchTo) * kBeatsPerBar))
                continue; // outside the punch range
            if (held.down) // pressed again without a release: the first one ends here
                m_take.push_back({held.beat, endOf(held.beat, e->beat), pitch, held.velocity});
            held = {true, e->beat, e->data2};
        } else if (held.down) {
            m_take.push_back({held.beat, endOf(held.beat, e->beat), pitch, held.velocity});
            held.down = false;
        }
        m_lastBeat = e->beat;
    }
    if (active)
        m_lastBeat = std::max(m_lastBeat, m_transport.beatPosition());
    else if (m_taking)
        finishTake();
}

void ArrangeController::finishTake()
{
    const SequencerState &state = m_sequencer.state();
    const double loopEnd = state.loopOn ? double(state.loopTo) * kBeatsPerBar : -1.0;
    for (int pitch = 0; pitch < 128; ++pitch) { // still held when play stopped
        Held &held = m_held[std::size_t(pitch)];
        if (held.down) {
            const double off = m_lastBeat > held.beat ? m_lastBeat : loopEnd > held.beat ? loopEnd : held.beat + 1.0;
            m_take.push_back({held.beat, off, pitch, held.velocity});
            held.down = false;
        }
    }
    m_taking = false;
    if (m_take.empty())
        return;
    double first = m_take.front().on, last = 0.0;
    for (const TakeNote &n : m_take) {
        first = std::min(first, n.on);
        last = std::max(last, n.off);
    }
    const int startBar = std::clamp(int(std::floor(first / kBeatsPerBar)), 0, kMaxBars - 1);
    const int endBar = std::clamp(int(std::ceil(last / kBeatsPerBar - 1e-9)), startBar + 1, kMaxBars);
    const double origin = double(startBar) * kBeatsPerBar;
    MidiClip clip;
    for (const TakeNote &n : m_take) {
        const auto start = std::uint32_t(std::lround((n.on - origin) * kTicksPerBeat));
        const auto length = std::uint32_t(std::max<long>(1, std::lround((n.off - n.on) * kTicksPerBeat)));
        clip.notes.push_back({start, std::min<std::uint32_t>(length, std::uint32_t(endBar * kBeatsPerBar * kTicksPerBeat) - start),
                              std::uint8_t(n.pitch), std::uint8_t(std::clamp(n.velocity, 1, 127))});
    }
    std::stable_sort(clip.notes.begin(), clip.notes.end(), [](const ClipNote &a, const ClipNote &b) { return a.start < b.start; });
    m_take.clear();
    beforeEdit();

    Arrangement &song = m_sequencer.arrangerState().arrangement;
    for (int t = 0; t < kTracks; ++t) {
        if (!m_armed[std::size_t(t)])
            continue;
        // The clips it overlaps go (and their MIDI, if theirs).
        auto &lane = song.lanes[std::size_t(t)];
        for (int i = int(lane.size()) - 1; i >= 0; --i) {
            const Clip c = lane[std::size_t(i)];
            if (c.startBar >= endBar || c.endBar() <= startBar)
                continue;
            lane.erase(lane.begin() + i);
            if (c.midi >= 0) {
                song.midi.erase(song.midi.begin() + c.midi);
                for (auto &l : song.lanes)
                    for (Clip &other : l)
                        if (other.midi > c.midi)
                            --other.midi;
            }
        }
        song.midi.push_back(clip);
        const auto at = std::find_if(lane.begin(), lane.end(), [&](const Clip &c) { return c.startBar > startBar; });
        lane.insert(at, Clip{startBar, endBar - startBar, -1, int(song.midi.size()) - 1});
    }
    edited(true);
}

void ArrangeController::poll()
{
    takeCaptured();
    const bool playing = m_transport.playing();
    const bool following = m_transport.following();
    const qreal position = playing ? m_transport.beatPosition() : (songMode() ? startBar() * kBeatsPerBar : 0.0);
    if (playing == m_playing && following == m_following && position == m_position)
        return;
    m_playing = playing;
    m_following = following;
    m_position = position;
    emit positionChanged();
}

void ArrangeController::apply()
{
    const SequencerState &state = m_sequencer.state();
    // Outside SONG mode the patterns play as before: from the top, no loop, no punch.
    m_transport.setStartBeat(state.songMode ? double(state.startBar) * kBeatsPerBar : 0.0);
    m_transport.setLoop(state.songMode && state.loopOn, state.loopFrom, state.loopTo);
    if (m_tracks)
        m_tracks->setPunch(state.songMode && state.punchOn, state.punchFrom, state.punchTo);
}

void ArrangeController::edited(bool arrangement)
{
    m_sequencer.arrangerEdited(arrangement); // its changed() applies and tells the screen
}
