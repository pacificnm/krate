#include "ArrangeController.h"

#include <algorithm>

#include <QJSEngine>

#include "AudioTracks.h"
#include "SequencerController.h"
#include "core/Transport.h"
#include "core/Instrument.h"
#include "rack/Automation.h"
#include "rack/MidiCapture.h"

#include <cmath>

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
        if (m_player && m_sequencer.state().automation != m_handed)
            handOverAutomation();
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
    m_writeTimer.setInterval(15); // knobs recorded about 66 times a second
    connect(&m_writeTimer, &QTimer::timeout, this, &ArrangeController::writeTick);
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
    const double origin = -double(clip.offsetBars) * kBeatsPerBar; // its content's start, from the clip's
    const auto add = [&](double x, double w, int pitch) {
        if (x >= 0.0 && x < clipBeats && list.size() < kMaxDrawnNotes)
            list.append(QVariantMap{{QStringLiteral("x"), x},
                                    {QStringLiteral("w"), std::min(w, clipBeats - x)},
                                    {QStringLiteral("pitch"), pitch}});
    };
    if (clip.pattern >= 0) {
        const Pattern &p = state.bank->patterns[std::size_t(clip.pattern)];
        const TrackNotes &notes = p.tracks[std::size_t(track)];
        const double length = double(std::max<int>(p.length, 1)) / kTicksPerBeat;
        for (double pass = origin + std::floor(-origin / length) * length; pass < clipBeats; pass += length)
            for (int n = 0; n < notes.count; ++n) {
                const Note &note = notes.notes[std::size_t(n)];
                add(pass + double(note.start) / kTicksPerBeat, double(note.length) / kTicksPerBeat, note.pitch);
            }
    } else if (clip.midi >= 0 && clip.midi < int(state.arrangement.midi.size())) {
        for (const ClipNote &note : state.arrangement.midi[std::size_t(clip.midi)].notes)
            add(origin + double(note.start) / kTicksPerBeat, double(note.length) / kTicksPerBeat, note.pitch);
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
    return {m_sequencer.state().arrangement, m_tracks ? m_tracks->lanes() : AudioTracksController::Lanes{},
            m_sequencer.state().automation};
}

void ArrangeController::restore(const Snapshot &s)
{
    m_restoring = true;
    m_sequencer.arrangerState().arrangement = s.song;
    m_sequencer.arrangerState().automation = s.automation;
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

void ArrangeController::setAutomationTargets(krate::AutomationPlayer *player, QStringList ids, QStringList names,
                                             std::vector<krate::IInstrument *> instruments)
{
    m_player = player;
    m_deviceIds = std::move(ids);
    m_deviceNames = std::move(names);
    m_instruments = std::move(instruments);
    if (m_player)
        m_player->setInstruments(m_instruments);
    apply();
    handOverAutomation();
}

void ArrangeController::setAutoWrite(bool on)
{
    if (on == m_autoWrite)
        return;
    m_autoWrite = on;
    ++m_revision;
    emit changed();
}

int ArrangeController::deviceOf(const QString &id) const
{
    return int(m_deviceIds.indexOf(id));
}

int ArrangeController::paramOf(int device, const QString &key) const
{
    if (device < 0 || device >= int(m_instruments.size()) || !m_instruments[std::size_t(device)])
        return -1;
    const auto params = m_instruments[std::size_t(device)]->parameters();
    for (std::size_t i = 0; i < params.size(); ++i)
        if (key == QLatin1StringView(params[i].key.data(), qsizetype(params[i].key.size())))
            return int(i);
    return -1;
}

void ArrangeController::handOverAutomation()
{
    m_handed = m_sequencer.state().automation;
    if (!m_player)
        return;
    // Lane for lane (one not found plays nothing), so indices match the state's.
    auto automation = std::make_unique<krate::Automation>();
    for (const AutomationData &lane : m_handed) {
        const int device = deviceOf(lane.device), param = paramOf(device, lane.param);
        automation->lanes.push_back({param >= 0 ? device : -1, param, lane.points});
    }
    m_player->setAutomation(std::move(automation));
}

QVariantList ArrangeController::automationLanes() const
{
    QVariantList list;
    for (const AutomationData &lane : m_sequencer.state().automation) {
        const int device = deviceOf(lane.device), param = paramOf(device, lane.param);
        QVariantList points;
        for (const krate::AutomationPoint &p : lane.points)
            points.append(QVariantMap{{QStringLiteral("beat"), p.beat}, {QStringLiteral("value"), double(p.value)}});
        const auto params = param >= 0 ? m_instruments[std::size_t(device)]->parameters() : std::span<const krate::ParamDescriptor>{};
        list.append(QVariantMap{
            {QStringLiteral("device"), device},
            {QStringLiteral("deviceName"), device >= 0 ? m_deviceNames.value(device) : lane.device},
            {QStringLiteral("param"), param},
            {QStringLiteral("label"), param >= 0 ? QString::fromUtf8(params[std::size_t(param)].label.data(),
                                                                    qsizetype(params[std::size_t(param)].label.size()))
                                                 : lane.param},
            {QStringLiteral("points"), points},
        });
    }
    return list;
}

QVariantList ArrangeController::automatableDevices() const
{
    QVariantList list;
    for (int d = 0; d < int(m_instruments.size()); ++d) {
        const QString id = m_deviceIds.value(d);
        if (!m_instruments[std::size_t(d)] || m_instruments[std::size_t(d)]->parameters().empty()
            || id.startsWith(QStringLiteral("audio")))
            continue; // (an audio track's ARM / MONITOR / INPUT aren't for automating)
        list.append(QVariantMap{{QStringLiteral("device"), d}, {QStringLiteral("name"), m_deviceNames.value(d)}});
    }
    return list;
}

QStringList ArrangeController::paramLabels(int device) const
{
    QStringList list;
    if (device < 0 || device >= int(m_instruments.size()) || !m_instruments[std::size_t(device)])
        return list;
    for (const krate::ParamDescriptor &p : m_instruments[std::size_t(device)]->parameters())
        list << QString::fromUtf8(p.label.data(), qsizetype(p.label.size()));
    return list;
}

int ArrangeController::addAutomationLane(int device, int param)
{
    if (device < 0 || device >= int(m_instruments.size()) || !m_instruments[std::size_t(device)])
        return -1;
    const auto params = m_instruments[std::size_t(device)]->parameters();
    if (param < 0 || param >= int(params.size()))
        return -1;
    const QString id = m_deviceIds.value(device);
    const QString key = QString::fromUtf8(params[std::size_t(param)].key.data(), qsizetype(params[std::size_t(param)].key.size()));
    auto &lanes = m_sequencer.arrangerState().automation;
    for (std::size_t i = 0; i < lanes.size(); ++i)
        if (lanes[i].device == id && lanes[i].param == key)
            return int(i);
    beforeEdit();
    // It starts where the knob is.
    lanes.push_back({id, key, {{0.0, m_instruments[std::size_t(device)]->param(param)}}});
    edited(false);
    return int(lanes.size()) - 1;
}

void ArrangeController::removeAutomationLane(int lane)
{
    auto &lanes = m_sequencer.arrangerState().automation;
    if (lane < 0 || lane >= int(lanes.size()))
        return;
    beforeEdit();
    lanes.erase(lanes.begin() + lane);
    edited(false);
}

namespace {
void sortPoints(std::vector<krate::AutomationPoint> &points)
{
    std::stable_sort(points.begin(), points.end(),
                     [](const krate::AutomationPoint &a, const krate::AutomationPoint &b) { return a.beat < b.beat; });
}
} // namespace

int ArrangeController::addPoint(int lane, double beat, double value)
{
    auto &lanes = m_sequencer.arrangerState().automation;
    if (lane < 0 || lane >= int(lanes.size()))
        return -1;
    beforeEdit();
    auto &points = lanes[std::size_t(lane)].points;
    const krate::AutomationPoint p{std::max(0.0, beat), float(std::clamp(value, 0.0, 1.0))};
    const auto at = std::upper_bound(points.begin(), points.end(), p.beat,
                                     [](double b, const krate::AutomationPoint &q) { return b < q.beat; });
    const int index = int(at - points.begin());
    points.insert(at, p);
    edited(false);
    return index;
}

int ArrangeController::movePoint(int lane, int point, double beat, double value)
{
    auto &lanes = m_sequencer.arrangerState().automation;
    if (lane < 0 || lane >= int(lanes.size()))
        return -1;
    auto &points = lanes[std::size_t(lane)].points;
    if (point < 0 || point >= int(points.size()))
        return -1;
    beforeEdit();
    krate::AutomationPoint p{std::max(0.0, beat), float(std::clamp(value, 0.0, 1.0))};
    points.erase(points.begin() + point);
    const auto at = std::upper_bound(points.begin(), points.end(), p.beat,
                                     [](double b, const krate::AutomationPoint &q) { return b < q.beat; });
    const int index = int(at - points.begin());
    points.insert(at, p);
    edited(false);
    return index;
}

void ArrangeController::removePoint(int lane, int point)
{
    auto &lanes = m_sequencer.arrangerState().automation;
    if (lane < 0 || lane >= int(lanes.size()))
        return;
    auto &points = lanes[std::size_t(lane)].points;
    if (point < 0 || point >= int(points.size()))
        return;
    beforeEdit();
    points.erase(points.begin() + point);
    edited(false);
}

QString ArrangeController::valueText(int lane, double value) const
{
    const auto &lanes = m_sequencer.state().automation;
    if (lane < 0 || lane >= int(lanes.size()))
        return {};
    const int device = deviceOf(lanes[std::size_t(lane)].device), param = paramOf(device, lanes[std::size_t(lane)].param);
    if (param < 0)
        return QString::number(value, 'f', 2);
    return QString::fromStdString(m_instruments[std::size_t(device)]->parameters()[std::size_t(param)].format(float(value)));
}

void ArrangeController::writeTick()
{
    const double beat = m_transport.beatPosition();
    for (int d = 0; d < int(m_instruments.size()); ++d) {
        krate::IInstrument *instrument = m_instruments[std::size_t(d)];
        if (!instrument || m_deviceIds.value(d).startsWith(QStringLiteral("audio")))
            continue;
        auto &seen = m_seen[std::size_t(d)];
        for (int p = 0; p < int(seen.size()); ++p) {
            const float v = instrument->param(p);
            if (v == seen[std::size_t(p)])
                continue;
            seen[std::size_t(p)] = v;
            auto rec = std::find_if(m_recorded.begin(), m_recorded.end(),
                                    [&](const Recorded &r) { return r.device == d && r.param == p; });
            if (rec == m_recorded.end()) {
                // Its lane playing it isn't someone turning it.
                int lane = -1;
                for (std::size_t i = 0; i < m_handed.size() && lane < 0; ++i)
                    if (deviceOf(m_handed[i].device) == d && paramOf(d, m_handed[i].param) == p)
                        lane = int(i);
                if (lane >= 0) {
                    const float played = m_player->lastValue(lane);
                    if (!std::isnan(played) && std::fabs(v - played) < 1e-6f)
                        continue;
                    m_player->setHeld(lane, true); // touched: recorded from here, not played
                }
                m_recorded.push_back({d, p, {}});
                rec = m_recorded.end() - 1;
            }
            auto &segments = rec->segments;
            if (segments.empty() || beat < segments.back().back().beat - 1e-9)
                segments.emplace_back(); // a loop went round
            segments.back().push_back({beat, v});
        }
    }
}

void ArrangeController::finishWriting()
{
    writeTick(); // the last moves
    m_writing = false;
    m_writeTimer.stop();
    if (!m_recorded.empty()) {
        beforeEdit();
        auto &lanes = m_sequencer.arrangerState().automation;
        constexpr double kEdge = 1.0 / kTicksPerBeat; // the old line, just outside what was recorded
        for (const Recorded &rec : m_recorded) {
            const QString id = m_deviceIds.value(rec.device);
            const auto &params = m_instruments[std::size_t(rec.device)]->parameters();
            const QString key = QString::fromUtf8(params[std::size_t(rec.param)].key.data(),
                                                  qsizetype(params[std::size_t(rec.param)].key.size()));
            auto lane = std::find_if(lanes.begin(), lanes.end(),
                                     [&](const AutomationData &l) { return l.device == id && l.param == key; });
            if (lane == lanes.end()) {
                lanes.push_back({id, key, {}});
                lane = lanes.end() - 1;
            }
            for (const auto &segment : rec.segments) {
                const double first = segment.front().beat, last = segment.back().beat;
                auto &points = lane->points;
                const bool had = !points.empty();
                const krate::AutomationLane old{-1, -1, points};
                std::erase_if(points, [&](const krate::AutomationPoint &p) {
                    return p.beat >= first - kEdge && p.beat <= last + kEdge;
                });
                points.insert(points.end(), segment.begin(), segment.end());
                if (had) { // the lane as it was, either side
                    if (first - kEdge >= 0.0)
                        points.push_back({first - kEdge, old.valueAt(first - kEdge)});
                    points.push_back({last + kEdge, old.valueAt(last + kEdge)});
                }
                sortPoints(points);
            }
            // Fewer points: those a straight line through their neighbours already gives.
            auto &points = lane->points;
            for (std::size_t i = 1; i + 1 < points.size();) {
                const krate::AutomationLane line{-1, -1, {points[i - 1], points[i + 1]}};
                if (std::fabs(line.valueAt(points[i].beat) - points[i].value) < 0.002f)
                    points.erase(points.begin() + std::ptrdiff_t(i));
                else
                    ++i;
            }
        }
        m_recorded.clear();
        edited(false);
    }
    if (m_player)
        m_player->releaseAll();
}

void ArrangeController::poll()
{
    // Recording automation: AUTO, SONG and playing (and Krate keeping its own time).
    const bool write = m_player && m_autoWrite && songMode() && m_transport.playing() && !m_transport.following();
    if (write && !m_writing) {
        m_seen.assign(m_instruments.size(), {});
        for (std::size_t d = 0; d < m_instruments.size(); ++d)
            if (m_instruments[d])
                for (int p = 0; p < int(m_instruments[d]->parameters().size()); ++p)
                    m_seen[d].push_back(m_instruments[d]->param(p));
        m_recorded.clear();
        m_writing = true;
        m_writeTimer.start();
    } else if (!write && m_writing) {
        finishWriting();
    }
    if (m_player)
        m_player->collectGarbage();
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
    if (m_player)
        m_player->setActive(state.songMode);
    if (m_tracks)
        m_tracks->setPunch(state.songMode && state.punchOn, state.punchFrom, state.punchTo);
}

void ArrangeController::edited(bool arrangement)
{
    m_sequencer.arrangerEdited(arrangement); // its changed() applies and tells the screen
}
