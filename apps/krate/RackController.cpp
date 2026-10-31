#include "RackController.h"

#include <algorithm>

#include "RackRunControl.h"

#include <cmath>

#include <QJSEngine>

#include "app/InstrumentRegistry.h"
#include "core/Transport.h"
#include "rack/Rack.h"

namespace {
constexpr int kPollIntervalMs = 33;   // ~30 Hz
constexpr qreal kMeterFloorDb = -60.0;
constexpr qreal kMeterFall = 0.03;    // per poll: ~1 s from full to empty

qreal meterLevel(float peak, qreal previous)
{
    const qreal db = peak > 0.0f ? 20.0 * std::log10(qreal(peak)) : kMeterFloorDb;
    const qreal target = qBound(0.0, 1.0 - db / kMeterFloorDb, 1.0);
    return std::max(target, previous - kMeterFall);
}
} // namespace

RackController::RackController(krate::Rack &rack, krate::Transport &transport,
                               const QList<krate::InstrumentEntry> &entries, QObject *parent)
    : QObject(parent)
    , m_rack(rack)
    , m_transport(transport)
    , m_entries(entries)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_rack.setKeysChannel(kKeysAnyChannel); // your keyboard plays whatever is in focus
    for (const auto &e : entries) {
        m_names << e.name;
        m_modules << e.qmlModule;
        m_components << e.panelComponent;
        if (e.panelComponent.isEmpty()) // no panel to show: focus passes it by
            rack.setFocusable(int(m_modules.size() - 1), false);
        m_strips.append(Strip{});
        m_meters.append(0.0);
    }
    m_focus = m_rack.focus();
    applyEffects(); // the returns' defaults

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &RackController::poll);
    m_pollTimer.start();
}

RackController::~RackController()
{
    s_instance = nullptr;
}

RackController *RackController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int RackController::stepFocus(int step) const
{
    const int count = deviceCount();
    if (count == 0)
        return m_focus;
    int next = m_focus;
    for (int i = 0; i < count; ++i) {
        next = ((next + step) % count + count) % count;
        if (!m_components.value(next).isEmpty())
            return next;
    }
    return m_focus;
}

void RackController::setFocus(int device)
{
    if (device < 0 || device >= deviceCount() || device == m_focus || m_components.value(device).isEmpty())
        return;
    m_rack.setFocus(device);
    m_focus = device;
    emit focusChanged();
    setMixerShown(false);
    setSongShown(false);
    setSeqShown(false);
}

int RackController::keysChannel() const
{
    return m_rack.keysChannel();
}

void RackController::setKeysChannel(int channel)
{
    channel = std::clamp(channel, -1, kKeysAnyChannel);
    if (channel == m_rack.keysChannel())
        return;
    m_rack.setKeysChannel(channel);
    emit keysChannelChanged();
}

void RackController::setMixerShown(bool shown)
{
    if (shown == m_mixerShown)
        return;
    if (shown) {
        setSongShown(false);
        setSeqShown(false);
    }
    m_mixerShown = shown;
    emit mixerShownChanged();
}

void RackController::setSongShown(bool shown)
{
    if (shown == m_songShown)
        return;
    if (shown) {
        setMixerShown(false);
        setSeqShown(false);
    }
    m_songShown = shown;
    emit songShownChanged();
}

void RackController::setSeqShown(bool shown)
{
    if (shown == m_seqShown)
        return;
    if (shown) {
        setMixerShown(false);
        setSongShown(false);
    }
    m_seqShown = shown;
    emit seqShownChanged();
}

bool RackController::playing() const { return m_transport.playing(); }

void RackController::setPlaying(bool playing)
{
    if (playing == m_transport.playing())
        return;
    if (playing && m_run) // the rack's PLAY runs every sequencer
        m_run->playAll();
    m_transport.setPlaying(playing);
    emit playingChanged();
}

qreal RackController::bpm() const { return m_transport.bpm(); }

void RackController::setBpm(qreal bpm)
{
    bpm = qBound(kMinBpm, std::round(bpm), kMaxBpm);
    if (bpm == m_transport.bpm())
        return;
    m_transport.setBpm(bpm);
    emit bpmChanged();
}

qreal RackController::fader(int device) const
{
    if (device < 0)
        return m_masterFader;
    return device < m_strips.size() ? m_strips[device].fader : 0.0;
}

void RackController::setFader(int device, qreal position)
{
    position = qBound(0.0, position, 1.0);
    if (device < 0) {
        m_masterFader = position;
        m_rack.setMasterLevel(float(gainFor(position)));
    } else if (device < m_strips.size()) {
        m_strips[device].fader = position;
        m_rack.setLevel(device, float(gainFor(position)));
    } else {
        return;
    }
    ++m_mixerRevision;
    emit mixerChanged();
}

QString RackController::faderText(qreal position) const
{
    const qreal gain = gainFor(qBound(0.0, position, 1.0));
    return gain <= 0.0 ? QStringLiteral("-inf dB") : QStringLiteral("%1 dB").arg(20.0 * std::log10(gain), 0, 'f', 1);
}

qreal RackController::pan(int device) const
{
    return device >= 0 && device < m_strips.size() ? m_strips[device].pan : 0.0;
}

void RackController::setPan(int device, qreal pan)
{
    if (device < 0 || device >= m_strips.size())
        return;
    m_strips[device].pan = qBound(-1.0, pan, 1.0);
    m_rack.setPan(device, float(m_strips[device].pan));
    ++m_mixerRevision;
    emit mixerChanged();
}

namespace {
// Each EQ knob's range: low, high, logarithmic.
struct EqRange
{
    double low, high;
    bool log;
};
constexpr EqRange kEqRanges[krate::ChannelEq::Count] = {
    {-krate::ChannelEq::kMaxGainDb, krate::ChannelEq::kMaxGainDb, false}, // low gain
    {30.0, 600.0, true},                                                    // low freq
    {-krate::ChannelEq::kMaxGainDb, krate::ChannelEq::kMaxGainDb, false}, // mid gain
    {100.0, 10000.0, true},                                                 // mid freq
    {0.3, 8.0, true},                                                       // mid Q
    {-krate::ChannelEq::kMaxGainDb, krate::ChannelEq::kMaxGainDb, false}, // high gain
    {1500.0, 16000.0, true},                                                // high freq
};
} // namespace

double RackController::eqValue(int band, qreal position)
{
    if (band < 0 || band >= krate::ChannelEq::Count)
        return 0.0;
    const EqRange &r = kEqRanges[band];
    const double p = qBound(0.0, position, 1.0);
    return r.log ? r.low * std::pow(r.high / r.low, p) : r.low + (r.high - r.low) * p;
}

qreal RackController::eqPosition(int band, double value)
{
    if (band < 0 || band >= krate::ChannelEq::Count)
        return 0.0;
    const EqRange &r = kEqRanges[band];
    const double p = r.log ? std::log(value / r.low) / std::log(r.high / r.low) : (value - r.low) / (r.high - r.low);
    return qBound(0.0, p, 1.0);
}

std::array<qreal, krate::ChannelEq::Count> RackController::defaultEq()
{
    std::array<qreal, krate::ChannelEq::Count> eq{};
    for (int b = 0; b < krate::ChannelEq::Count; ++b)
        eq[std::size_t(b)] = eqPosition(b, krate::ChannelEq::kFlat[std::size_t(b)]);
    return eq;
}

qreal RackController::eqDefault(int band) const
{
    return band >= 0 && band < krate::ChannelEq::Count ? defaultEq()[std::size_t(band)] : 0.0;
}

qreal RackController::eq(int device, int band) const
{
    if (device < 0 || device >= m_strips.size() || band < 0 || band >= krate::ChannelEq::Count)
        return 0.0;
    return m_strips[device].eq[std::size_t(band)];
}

void RackController::setEq(int device, int band, qreal position)
{
    if (device < 0 || device >= m_strips.size() || band < 0 || band >= krate::ChannelEq::Count)
        return;
    qreal p = qBound(0.0, position, 1.0);
    const bool gain = band == krate::ChannelEq::LowGain || band == krate::ChannelEq::MidGain || band == krate::ChannelEq::HighGain;
    if (gain && std::fabs(p - 0.5) < 0.005)
        p = 0.5; // a gain near the middle is 0 dB exactly (so a strip can be flat again)
    m_strips[device].eq[std::size_t(band)] = p;
    m_rack.setEq(device, band, float(eqValue(band, p)));
    ++m_mixerRevision;
    emit mixerChanged();
}

void RackController::resetEq(int device)
{
    for (int b = 0; b < krate::ChannelEq::Count; ++b)
        setEq(device, b, eqDefault(b));
}

bool RackController::eqFlat(int device) const
{
    if (device < 0 || device >= m_strips.size())
        return true;
    krate::ChannelEq::Settings s{};
    for (int b = 0; b < krate::ChannelEq::Count; ++b)
        s[std::size_t(b)] = float(eqValue(b, m_strips[device].eq[std::size_t(b)]));
    return krate::ChannelEq::flat(s);
}

QString RackController::eqText(int band, qreal position) const
{
    const double v = eqValue(band, position);
    switch (band) {
    case krate::ChannelEq::LowGain:
    case krate::ChannelEq::MidGain:
    case krate::ChannelEq::HighGain:
        return std::fabs(v) < 0.05 ? QStringLiteral("0 dB") : QStringLiteral("%1%2 dB").arg(v > 0 ? "+" : "").arg(v, 0, 'f', 1);
    case krate::ChannelEq::MidQ:
        return QStringLiteral("Q %1").arg(v, 0, 'f', v < 1.0 ? 2 : 1);
    default:
        return v >= 1000.0 ? QStringLiteral("%1 kHz").arg(v / 1000.0, 0, 'f', v >= 10000.0 ? 1 : 2)
                           : QStringLiteral("%1 Hz").arg(qRound(v));
    }
}

QJsonArray RackController::eqJson(int device) const
{
    QJsonArray a;
    for (int b = 0; b < krate::ChannelEq::Count; ++b)
        a.append(eq(device, b));
    return a;
}

void RackController::setEqJson(int device, const QJsonArray &values)
{
    for (int b = 0; b < krate::ChannelEq::Count; ++b)
        setEq(device, b, b < values.size() ? values[b].toDouble(eqDefault(b)) : eqDefault(b));
}

// --- Effects ----------------------------------------------------------------------------

qreal RackController::send(int device, int effect) const
{
    if (device < 0 || device >= m_strips.size() || effect < 0 || effect > 1)
        return 0.0;
    return m_strips[device].sends[std::size_t(effect)];
}

void RackController::setSend(int device, int effect, qreal position)
{
    if (device < 0 || device >= m_strips.size() || effect < 0 || effect > 1)
        return;
    m_strips[device].sends[std::size_t(effect)] = qBound(0.0, position, 1.0);
    m_rack.setSend(device, effect, float(m_strips[device].sends[std::size_t(effect)]));
    ++m_mixerRevision;
    emit mixerChanged();
}

QString RackController::sendText(qreal position) const
{
    return position <= 0.0 ? QStringLiteral("off") : QStringLiteral("%1%").arg(qRound(position * 100.0));
}

qreal RackController::effect(int control) const
{
    return control >= 0 && control < kEffectControls ? m_effects[std::size_t(control)] : 0.0;
}

qreal RackController::effectDefault(int control) const
{
    static constexpr std::array<qreal, kEffectControls> defaults{0.7, 0.5, 0.7, 0.5, 0.35 / 0.9};
    return control >= 0 && control < kEffectControls ? defaults[std::size_t(control)] : 0.0;
}

void RackController::setEffect(int control, qreal position)
{
    if (control < 0 || control >= kEffectControls)
        return;
    position = qBound(0.0, position, 1.0);
    if (control == 3) // delay time: one of its positions
        position = qRound(position * (kDelayTimes - 1)) / qreal(kDelayTimes - 1);
    m_effects[std::size_t(control)] = position;
    applyEffects();
    ++m_mixerRevision;
    emit mixerChanged();
}

void RackController::applyEffects()
{
    m_rack.setReverb(float(m_effects[0]), float(m_effects[1]));
    m_rack.setDelay(float(m_effects[2]), qRound(m_effects[3] * (kDelayTimes - 1)), float(m_effects[4] * 0.9));
}

QString RackController::effectText(int control, qreal position) const
{
    switch (control) {
    case 1:
        return QStringLiteral("%1 s").arg(krate::Reverb::decaySeconds(position), 0, 'f', 1);
    case 3: {
        static const char *names[kDelayTimes] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
        return QString::fromLatin1(names[qBound(0, qRound(position * (kDelayTimes - 1)), kDelayTimes - 1)]);
    }
    case 4:
        return QStringLiteral("%1%").arg(qRound(position * 90.0));
    default:
        return QStringLiteral("%1%").arg(qRound(position * 100.0));
    }
}

bool RackController::mute(int device) const
{
    return device >= 0 && device < m_strips.size() && m_strips[device].mute;
}

void RackController::setMute(int device, bool mute)
{
    if (device < 0 || device >= m_strips.size())
        return;
    m_strips[device].mute = mute;
    m_rack.setMute(device, mute);
    ++m_mixerRevision;
    emit mixerChanged();
}

bool RackController::solo(int device) const
{
    return device >= 0 && device < m_strips.size() && m_strips[device].solo;
}

int RackController::midiChannel(int device) const
{
    for (int ch = 0; ch < krate::Rack::kChannels; ++ch)
        if (m_rack.channelDevice(ch) == device)
            return ch + 1;
    return 0;
}

int RackController::midiChannelCount(int device) const
{
    int n = 0;
    for (int ch = 0; ch < krate::Rack::kChannels; ++ch)
        n += m_rack.channelDevice(ch) == device ? 1 : 0;
    return n;
}

void RackController::restoreMidiChannels(int device)
{
    if (device < 0 || device >= m_strips.size() || device >= m_entries.size())
        return;
    const krate::InstrumentEntry &entry = m_entries[device];
    setMidiChannel(device, entry.defaultMidiChannel + 1);
    for (int channel : entry.extraMidiChannels)
        m_rack.setChannelDevice(channel, device);
    ++m_mixerRevision;
    emit mixerChanged();
}

void RackController::setMidiChannel(int device, int channel)
{
    if (device < 0 || device >= m_strips.size() || channel < 0 || channel > krate::Rack::kChannels)
        return;
    for (int ch = 0; ch < krate::Rack::kChannels; ++ch)
        if (m_rack.channelDevice(ch) == device)
            m_rack.setChannelDevice(ch, -1);
    if (channel > 0)
        m_rack.setChannelDevice(channel - 1, device);
    ++m_mixerRevision;
    emit mixerChanged(); // the session saves it
}

bool RackController::takesInput(int device) const
{
    return m_rack.takesInput(device);
}

int RackController::inputSource(int device) const
{
    return m_rack.inputSource(device);
}

void RackController::setInputSource(int device, int source)
{
    if (!takesInput(device) || source == inputSource(device))
        return;
    m_rack.setInputSource(device, source);
    ++m_mixerRevision;
    emit mixerChanged(); // the session saves it
}

void RackController::stepInputSource(int device, int delta)
{
    if (!takesInput(device))
        return;
    // The interface, then every instrument that can feed it (not itself, nor another with inputs).
    QList<int> sources{-1};
    for (int d = 0; d < m_strips.size(); ++d)
        if (d != device && !takesInput(d))
            sources << d;
    const int at = std::max(0, int(sources.indexOf(inputSource(device))));
    const int n = int(sources.size());
    setInputSource(device, sources.at(((at + delta) % n + n) % n));
}

QString RackController::inputSourceName(int device) const
{
    const int source = inputSource(device);
    return source < 0 ? QStringLiteral("INPUT") : m_names.value(source);
}

QString RackController::inputSourceId(int device) const
{
    const int source = inputSource(device);
    return source < 0 || source >= m_entries.size() ? QString() : m_entries[source].id;
}

void RackController::setInputSourceId(int device, const QString &id)
{
    int source = -1;
    for (qsizetype i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].id == id)
            source = int(i);
    if (takesInput(device))
        setInputSource(device, source);
}

void RackController::setSolo(int device, bool solo)
{
    if (device < 0 || device >= m_strips.size())
        return;
    m_strips[device].solo = solo;
    m_rack.setSolo(device, solo);
    ++m_mixerRevision;
    emit mixerChanged();
}

QJsonArray RackController::effectsJson() const
{
    QJsonArray a;
    for (qreal v : m_effects)
        a.append(v);
    return a;
}

void RackController::setEffectsJson(const QJsonArray &values)
{
    for (int c = 0; c < kEffectControls; ++c)
        setEffect(c, c < values.size() ? values[c].toDouble(effectDefault(c)) : effectDefault(c));
}

QJsonObject RackController::mixerState() const
{
    QJsonObject strips;
    for (qsizetype i = 0; i < m_strips.size() && i < m_entries.size(); ++i)
        strips[m_entries[i].id] = QJsonObject{{QStringLiteral("fader"), m_strips[i].fader},
                                              {QStringLiteral("pan"), m_strips[i].pan},
                                              {QStringLiteral("mute"), m_strips[i].mute},
                                              {QStringLiteral("solo"), m_strips[i].solo},
                                              {QStringLiteral("eq"), eqJson(int(i))},
                                              {QStringLiteral("sends"), QJsonArray{m_strips[i].sends[0], m_strips[i].sends[1]}},
                                              {QStringLiteral("input"), inputSourceId(int(i))}};
    return QJsonObject{{QStringLiteral("strips"), strips},
                       {QStringLiteral("master"), m_masterFader},
                       {QStringLiteral("effects"), effectsJson()}};
}

void RackController::setMixerState(const QJsonObject &state)
{
    const QJsonObject strips = state.value(u"strips").toObject();
    for (qsizetype i = 0; i < m_strips.size() && i < m_entries.size(); ++i) {
        if (!strips.contains(m_entries[i].id))
            continue; // an instrument added since: as it is
        const QJsonObject s = strips.value(m_entries[i].id).toObject();
        setFader(int(i), s.value(u"fader").toDouble(kUnityFader));
        setPan(int(i), s.value(u"pan").toDouble(0.0));
        setMute(int(i), s.value(u"mute").toBool(false));
        setSolo(int(i), s.value(u"solo").toBool(false));
        setEqJson(int(i), s.value(u"eq").toArray()); // none saved (older files): flat
        const QJsonArray sends = s.value(u"sends").toArray();
        for (int e = 0; e < 2; ++e)
            setSend(int(i), e, sends.at(e).toDouble(0.0)); // none saved: no sends
        setInputSourceId(int(i), s.value(u"input").toString()); // none saved: the interface
    }
    if (state.contains(u"master"))
        setFader(-1, state.value(u"master").toDouble(kUnityFader));
    setEffectsJson(state.value(u"effects").toArray());
}

void RackController::applyMixerMoves()
{
    while (const auto move = m_rack.popMixerMove()) {
        const int device = move->device == krate::Rack::kRackDevice ? -1 : move->device;
        if (device >= m_strips.size())
            continue;
        switch (move->control) {
        case krate::Rack::MixerFader:
            setFader(device, move->value / 127.0);
            break;
        case krate::Rack::MixerPan: // 64 is centre
            setPan(device, move->value == 64 ? 0.0 : (move->value - 64) / (move->value < 64 ? 64.0 : 63.0));
            break;
        case krate::Rack::MixerMute:
            setMute(device, !mute(device));
            break;
        case krate::Rack::MixerSolo:
            setSolo(device, !solo(device));
            break;
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::LowGain):
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::LowFreq):
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::MidGain):
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::MidFreq):
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::MidQ):
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::HighGain):
        case krate::Rack::MixerEqFirst + int(krate::ChannelEq::HighFreq):
            setEq(device, move->control - krate::Rack::MixerEqFirst, move->value / 127.0);
            break;
        case krate::Rack::MixerReverbSend:
        case krate::Rack::MixerDelaySend:
            setSend(device, move->control - krate::Rack::MixerReverbSend, move->value / 127.0);
            break;
        case krate::Rack::MixerReverbLevel:
        case krate::Rack::MixerReverbSize:
        case krate::Rack::MixerDelayLevel:
        case krate::Rack::MixerDelayTime:
        case krate::Rack::MixerDelayFeedback:
            setEffect(move->control - krate::Rack::MixerReverbLevel, move->value / 127.0);
            break;
        default:
            break;
        }
    }
}

void RackController::poll()
{
    applyMixerMoves();

    // Focus can change from the controller (Program Change, learned buttons).
    if (const int focus = m_rack.focus(); focus != m_focus) {
        m_focus = focus;
        emit focusChanged();
        setMixerShown(false);
        setSongShown(false);
        setSeqShown(false);
    }
    if (const bool playing = m_transport.playing(); playing != m_shownPlaying) { // an instrument's RUN, or a song's end
        m_shownPlaying = playing;
        emit playingChanged();
    }
    if (const double bpm = m_transport.bpm(); bpm != m_shownBpm) { // set by a song's tempo map too
        m_shownBpm = bpm;
        emit bpmChanged();
    }

    // Beat LED: lit for the first quarter of each beat while playing.
    const double beat = m_transport.beatPosition();
    const bool lit = m_transport.playing() && beat - std::floor(beat) < 0.25;
    if (lit != m_beat) {
        m_beat = lit;
        emit beatChanged();
    }

    for (int i = 0; i < m_meters.size(); ++i)
        m_meters[i] = meterLevel(m_rack.takeDevicePeak(i), m_meters[i].toReal());
    emit metersChanged();
}
