#include "RackController.h"

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
    for (const auto &e : entries) {
        m_names << e.name;
        m_modules << e.qmlModule;
        m_components << e.panelComponent;
        m_strips.append(Strip{});
        m_meters.append(0.0);
    }
    m_focus = m_rack.focus();

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

void RackController::setFocus(int device)
{
    if (device < 0 || device >= deviceCount() || device == m_focus)
        return;
    m_rack.setFocus(device);
    m_focus = device;
    emit focusChanged();
}

bool RackController::playing() const { return m_transport.playing(); }

void RackController::setPlaying(bool playing)
{
    if (playing == m_transport.playing())
        return;
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

void RackController::setSolo(int device, bool solo)
{
    if (device < 0 || device >= m_strips.size())
        return;
    m_strips[device].solo = solo;
    m_rack.setSolo(device, solo);
    ++m_mixerRevision;
    emit mixerChanged();
}

void RackController::poll()
{
    // Focus can change from the controller (Program Change, learned buttons).
    if (const int focus = m_rack.focus(); focus != m_focus) {
        m_focus = focus;
        emit focusChanged();
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
