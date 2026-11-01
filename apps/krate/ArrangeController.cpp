#include "ArrangeController.h"

#include <algorithm>

#include <QJSEngine>

#include "AudioTracks.h"
#include "SequencerController.h"
#include "core/Transport.h"

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
    apply();
    m_pollTimer.setInterval(33);
    connect(&m_pollTimer, &QTimer::timeout, this, &ArrangeController::poll);
    m_pollTimer.start();
}

ArrangeController::~ArrangeController()
{
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

void ArrangeController::poll()
{
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
