#include "app/HostController.h"

#include <cmath>

#include <QJSEngine>
#include <QLoggingCategory>
#include <QRegularExpression>

#include "host/AudioHost.h"

Q_LOGGING_CATEGORY(lcHost, "krate.host")

namespace krate {

namespace {
constexpr int kPollIntervalMs = 16;    // ~60 Hz
constexpr int kRetryIntervalMs = 2000;
constexpr qreal kMeterFloorDb = -60.0;
constexpr qreal kMeterFallPerPoll = 0.015; // ~1 s from full scale to empty
} // namespace

HostController::HostController(AudioHost &host, const QString &clientName, QObject *parent)
    : QObject(parent)
    , m_host(host)
    , m_clientName(clientName.toUtf8())
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &HostController::poll);
    m_pollTimer.start();

    m_retryTimer.setInterval(kRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &HostController::connectAudio);

    connectAudio();
}

HostController::~HostController()
{
    m_host.close();
    s_instance = nullptr;
}

HostController *HostController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool HostController::audioConnected() const { return m_host.isOpen(); }
int HostController::sampleRate() const { return int(m_host.sampleRate()); }
int HostController::bufferSize() const { return int(m_host.bufferSize()); }
int HostController::xrunCount() const { return int(m_host.xrunCount()); }

void HostController::connectAudio()
{
    if (m_host.isOpen())
        return;

    std::string error;
    if (!m_host.open(m_clientName.constData(), &error)) {
        setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
        if (!m_retryTimer.isActive()) {
            qCWarning(lcHost) << "audio offline:" << error.c_str() << "- retrying every"
                              << kRetryIntervalMs / 1000 << "s";
            m_retryTimer.start();
        }
        return;
    }

    m_retryTimer.stop();
    const auto ports = m_host.connectToPlayback();
    if (ports.empty())
        qCWarning(lcHost) << "connected to JACK, but found no playback ports to connect to";
    for (const auto &port : ports)
        qCInfo(lcHost) << "output ->" << port.c_str();

    m_lastXruns = -1; // force a status refresh
    connectMidiInput();
    poll();
}

void HostController::disconnectAudio()
{
    m_host.close();
    m_midiConnected = false;
    setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
}

void HostController::setMidiInputMatch(const QString &pattern)
{
    m_midiMatch = pattern;
    connectMidiInput();
}

void HostController::connectMidiInput()
{
    if (!m_host.isOpen() || m_midiMatch.isEmpty())
        return;
    const QRegularExpression match(m_midiMatch, QRegularExpression::CaseInsensitiveOption);
    if (!match.isValid()) {
        qCWarning(lcHost) << "invalid MIDI input pattern" << m_midiMatch;
        return;
    }
    // Every matching port: a controller can have several (the LX49 mk3 sends its
    // keys and knobs on MIDI1 and its control buttons on MIDI2). Ports already
    // connected are skipped by JACK, so this is safe to repeat on every port change.
    for (const auto &port : m_host.midiSourcePorts()) {
        const QString name = QString::fromStdString(port);
        if (name.contains(match) && m_host.connectMidiFrom(port))
            qCInfo(lcHost) << "midi_in <-" << name;
    }
}

void HostController::poll()
{
    if (m_host.isOpen() && m_host.serverGone()) {
        qCWarning(lcHost) << "audio server went away";
        m_host.close();
        m_midiConnected = false;
        setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
        m_retryTimer.start();
    }

    if (m_host.isOpen() && m_host.takePortsChanged())
        connectMidiInput();

    if (m_host.isOpen()) {
        const bool midi = m_host.midiInputConnected();
        if (midi != m_midiConnected) {
            m_midiConnected = midi;
            m_lastXruns = -1; // refresh the status line
        }
        const int xruns = xrunCount();
        const int rate = sampleRate();
        const int buffer = bufferSize();
        if (xruns != m_lastXruns || rate != m_lastSampleRate || buffer != m_lastBufferSize) {
            m_lastXruns = xruns;
            m_lastSampleRate = rate;
            m_lastBufferSize = buffer;
            setAudioStatus(QStringLiteral("%1 kHz · %2 · %3 xruns · %4")
                               .arg(rate / 1000.0, 0, 'g', 3)
                               .arg(buffer)
                               .arg(xruns)
                               .arg(m_midiConnected ? QStringLiteral("MIDI in") : QStringLiteral("no MIDI")));
        }
    }

    // Meter: peak in dB mapped to 0..1 over kMeterFloorDb..0, with a steady fall.
    const float peak = m_host.takePeak(); // master output, after the soft clip
    const qreal db = peak > 0.0f ? 20.0 * std::log10(qreal(peak)) : kMeterFloorDb;
    const qreal target = qBound(0.0, 1.0 - db / kMeterFloorDb, 1.0);
    const qreal level = std::max(target, m_outputLevel - kMeterFallPerPoll);
    if (!qFuzzyCompare(level + 1.0, m_outputLevel + 1.0)) {
        m_outputLevel = level;
        emit outputLevelChanged();
    }
}

void HostController::setAudioStatus(const QString &status)
{
    // Connection state, rate and xruns are all covered by this one notify signal.
    m_audioStatus = status;
    emit audioStatusChanged();
}

} // namespace krate
