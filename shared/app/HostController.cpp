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
    for (int i = 0; i < m_host.inputCount(); ++i) {
        m_inputPorts << QString();
        m_inputLevels << 0.0;
    }

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
    refreshCapturePorts();
    connectOutputs();

    if (m_requestedBufferSize > 0)
        m_host.setBufferSize(std::uint32_t(m_requestedBufferSize));
    m_lastXruns = -1; // force a status refresh
    connectMidiInput();
    connectInputs();
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

int HostController::inputCount() const { return m_host.inputCount(); }

void HostController::setInputPorts(const QStringList &ports)
{
    for (int i = 0; i < m_inputPorts.size(); ++i)
        m_inputPorts[i] = ports.value(i);
    emit inputPortsChanged();
    connectInputs();
}

void HostController::setInputPort(int input, const QString &port)
{
    if (input < 0 || input >= m_inputPorts.size() || m_inputPorts[input] == port)
        return;
    m_inputPorts[input] = port;
    emit inputPortsChanged();
    if (m_host.isOpen())
        m_host.connectInput(input, port.toStdString());
    m_lastXruns = -1; // refresh "connected"
}

QString HostController::portLabel(const QString &port)
{
    if (port.isEmpty())
        return QStringLiteral("None");
    QString device = port.section(u':', 0, 0);
    QString channel = port.section(u':', 1);
    // PipeWire's JACK ports carry the device's description ("USB Audio Interface");
    // some setups use its node name instead (alsa_input.usb-Maker_Device-00.analog-stereo).
    if (device.startsWith(u"alsa_input.")) {
        device = device.mid(11).section(u'.', 0, 0);
        for (const auto prefix : {u"usb-", u"pci-"})
            if (device.startsWith(prefix))
                device = device.mid(4);
        static const QRegularExpression serial(QStringLiteral("-\\d+$"));
        device.remove(serial);
        device.replace(u'_', u' ');
    }
    if (channel == u"capture_MONO") // a source for one input: its name says which
        return device;
    static const QRegularExpression stereo(QStringLiteral("^capture_F([LR])$"));
    static const QRegularExpression aux(QStringLiteral("^capture_AUX(\\d+)$"));
    if (const auto m = stereo.match(channel); m.hasMatch())
        channel = m.captured(1) == u"L" ? QStringLiteral("in 1") : QStringLiteral("in 2");
    else if (const auto n = aux.match(channel); n.hasMatch())
        channel = QStringLiteral("in %1").arg(n.captured(1).toInt() + 1);
    else
        channel.replace(QStringLiteral("capture_"), QStringLiteral("in "));
    return device + QStringLiteral(" · ") + channel;
}

QVariantList HostController::inputsConnected() const
{
    QVariantList result;
    for (int i = 0; i < m_inputPorts.size(); ++i)
        result << (m_host.isOpen() && m_host.inputConnected(i));
    return result;
}

QVariantList HostController::inputLevels() const
{
    QVariantList result;
    for (qreal level : m_inputLevels)
        result << level;
    return result;
}

void HostController::setRequestedBufferSize(int frames)
{
    frames = std::max(0, frames);
    if (frames == m_requestedBufferSize)
        return;
    m_requestedBufferSize = frames;
    emit requestedBufferSizeChanged();
    if (m_host.isOpen() && frames > 0)
        m_host.setBufferSize(std::uint32_t(frames));
}

qreal HostController::latencyMs() const
{
    const int rate = sampleRate();
    return rate > 0 ? 1000.0 * m_host.roundTripFrames() / rate : 0.0;
}

void HostController::refreshCapturePorts()
{
    QStringList ports, devices;
    if (m_host.isOpen()) {
        for (const auto &port : m_host.captureSourcePorts())
            ports << QString::fromStdString(port);
        for (const auto &device : m_host.playbackDevices())
            devices << QString::fromStdString(device);
    }
    if (ports != m_capturePorts || devices != m_outputDevices) {
        m_capturePorts = ports;
        m_outputDevices = devices;
        emit capturePortsChanged();
    }
}

void HostController::setOutputDevice(const QString &device)
{
    if (device == m_outputDevice)
        return;
    m_outputDevice = device;
    emit outputDeviceChanged();
    connectOutputs();
}

void HostController::connectOutputs()
{
    if (!m_host.isOpen())
        return;
    // The chosen device once it's there; until then (or with none chosen) the first hardware outputs.
    const bool present = m_outputDevices.contains(m_outputDevice);
    const auto ports = m_host.connectToPlayback(present ? m_outputDevice.toStdString() : std::string());
    if (ports.empty())
        qCWarning(lcHost) << "found no playback ports to connect to";
    for (const auto &port : ports)
        qCInfo(lcHost) << "output ->" << port.c_str();
    m_connectedOutput = present ? m_outputDevice : QString();
}

void HostController::connectInputs()
{
    if (!m_host.isOpen())
        return;
    // Only ports that exist: a device plugged in later connects on its port change.
    for (int i = 0; i < m_inputPorts.size(); ++i)
        if (m_inputPorts[i].isEmpty() || m_capturePorts.contains(m_inputPorts[i]))
            m_host.connectInput(i, m_inputPorts[i].toStdString());
    m_lastXruns = -1; // refresh "connected"
}

namespace {
qreal meterLevel(float peak, qreal previous)
{
    // Peak in dB mapped to 0..1 over kMeterFloorDb..0, with a steady fall.
    const qreal db = peak > 0.0f ? 20.0 * std::log10(qreal(peak)) : kMeterFloorDb;
    const qreal target = qBound(0.0, 1.0 - db / kMeterFloorDb, 1.0);
    return std::max(target, previous - kMeterFallPerPoll);
}
} // namespace

void HostController::poll()
{
    if (m_host.isOpen() && m_host.serverGone()) {
        qCWarning(lcHost) << "audio server went away";
        m_host.close();
        m_midiConnected = false;
        setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
        m_retryTimer.start();
    }

    if (m_host.isOpen() && m_host.takePortsChanged()) {
        connectMidiInput();
        refreshCapturePorts();
        connectInputs();
        if (m_connectedOutput != m_outputDevice && m_outputDevices.contains(m_outputDevice))
            connectOutputs(); // the chosen device came back
    }

    if (m_host.isOpen()) {
        const bool midi = m_host.midiInputConnected();
        if (midi != m_midiConnected) {
            m_midiConnected = midi;
            m_lastXruns = -1; // refresh the status line
        }
        const int xruns = xrunCount();
        const int rate = sampleRate();
        const int buffer = bufferSize();
        const std::uint32_t latency = m_host.roundTripFrames();
        if (xruns != m_lastXruns || rate != m_lastSampleRate || buffer != m_lastBufferSize || latency != m_lastLatency) {
            m_lastLatency = latency;
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

    bool inputsMoved = false;
    for (int i = 0; i < m_inputLevels.size(); ++i) {
        const qreal level = meterLevel(m_host.takeInputPeak(i), m_inputLevels[i]);
        if (!qFuzzyCompare(level + 1.0, m_inputLevels[i] + 1.0)) {
            m_inputLevels[i] = level;
            inputsMoved = true;
        }
    }
    if (inputsMoved)
        emit inputLevelsChanged();

    const qreal level = meterLevel(m_host.takePeak(), m_outputLevel); // master output, after the soft clip
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
