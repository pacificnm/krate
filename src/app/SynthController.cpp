#include "app/SynthController.h"

#include "app/Persistence.h"

#include <QJSEngine>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QtMath>

Q_LOGGING_CATEGORY(lcAudio, "k303.audio")

namespace {
constexpr int kPollIntervalMs = 16;    // ~60 Hz
constexpr int kRetryIntervalMs = 2000;
constexpr qreal kMeterFloorDb = -60.0;
constexpr qreal kMeterFallPerPoll = 0.015; // ~1 s from full scale to empty
} // namespace

SynthController::SynthController(const QString &panelFilePath, QObject *parent)
    : QObject(parent)
    , m_panelFilePath(panelFilePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Knob positions from last time (missing or unreadable file: defaults).
    k303::persistence::PanelValues values;
    if (k303::persistence::loadPanel(m_panelFilePath, &values) == k303::persistence::LoadResult::NewerVersion)
        m_panelFilePath += QStringLiteral(".v1"); // leave a newer file alone
    for (int i = 0; i < k303::ParamCount; ++i)
        m_engine.setParam(i, values[std::size_t(i)]);
    syncTempo();

    // Save 2 s after the last change, from the panel or from MIDI.
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &SynthController::savePanel);
    connect(this, &SynthController::paramsChanged, &m_saveTimer, qOverload<>(&QTimer::start));

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &SynthController::poll);
    m_pollTimer.start();

    m_retryTimer.setInterval(kRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &SynthController::connectAudio);

    connectAudio();
}

SynthController::~SynthController()
{
    if (m_saveTimer.isActive())
        savePanel();
    m_transport.setPlaying(false);
    m_client.close();
    s_instance = nullptr;
}

SynthController *SynthController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void SynthController::setPlaying(bool playing)
{
    if (playing == m_transport.playing())
        return;
    m_transport.setPlaying(playing);
    emit playingChanged();
}

void SynthController::setParam(k303::ParamId id, qreal value)
{
    const float v = float(qBound(0.0, value, 1.0));
    if (v == m_engine.params().get(id))
        return;
    m_engine.params().set(id, v);
    if (id == k303::ParamId::Tempo)
        syncTempo();
    emit paramsChanged();
}

void SynthController::syncTempo()
{
    m_transport.setBpm(k303::units::tempoBpm(m_engine.params().get(k303::ParamId::Tempo)));
}

void SynthController::setParamByKey(const QString &key, qreal value)
{
    if (const auto id = k303::paramFromKey(key.toStdString()))
        setParam(*id, value);
    else
        qCWarning(lcAudio) << "unknown parameter" << key;
}

qreal SynthController::defaultValue(const QString &key) const
{
    const auto id = k303::paramFromKey(key.toStdString());
    return id ? k303::paramDescriptor(*id).defaultValue : 0.0;
}

QString SynthController::displayText(const QString &key, qreal value) const
{
    const auto id = k303::paramFromKey(key.toStdString());
    if (!id)
        return {};
    return QString::fromStdString(k303::paramDescriptor(*id).format(float(qBound(0.0, value, 1.0))));
}

void SynthController::connectAudio()
{
    if (m_client.isOpen())
        return;

    std::string error;
    if (!m_client.open("k303", &error)) {
        setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
        if (!m_retryTimer.isActive()) {
            qCWarning(lcAudio) << "audio offline:" << error.c_str() << "- retrying every"
                               << kRetryIntervalMs / 1000 << "s";
            m_retryTimer.start();
        }
        return;
    }

    m_retryTimer.stop();
    const auto ports = m_client.connectToPlayback();
    if (ports.empty())
        qCWarning(lcAudio) << "connected to JACK, but found no playback ports to connect to";
    for (const auto &port : ports)
        qCInfo(lcAudio) << "output ->" << port.c_str();

    m_lastXruns = -1; // force a status refresh
    poll();
}

void SynthController::savePanel()
{
    m_saveTimer.stop();
    k303::persistence::PanelValues values{};
    for (int i = 0; i < k303::ParamCount; ++i)
        values[std::size_t(i)] = m_engine.param(i);
    if (!k303::persistence::savePanel(m_panelFilePath, values))
        qCWarning(lcAudio) << "could not save panel settings to" << m_panelFilePath;
}

void SynthController::setMidiInputMatch(const QString &pattern)
{
    m_midiMatch = pattern;
    connectMidiInput();
}

void SynthController::connectMidiInput()
{
    if (!m_client.isOpen() || m_midiMatch.isEmpty() || m_client.midiInputConnected())
        return;
    const QRegularExpression match(m_midiMatch, QRegularExpression::CaseInsensitiveOption);
    if (!match.isValid()) {
        qCWarning(lcAudio) << "invalid MIDI input pattern" << m_midiMatch;
        return;
    }
    for (const auto &port : m_client.midiSourcePorts()) {
        const QString name = QString::fromStdString(port);
        if (name.contains(match) && m_client.connectMidiFrom(port)) {
            qCInfo(lcAudio) << "midi_in <-" << name;
            return;
        }
    }
}

void SynthController::disconnectAudio()
{
    m_client.close();
    m_midiConnected = false;
    setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
}

void SynthController::poll()
{
    if (m_client.isOpen() && m_client.serverGone()) {
        qCWarning(lcAudio) << "audio server went away";
        m_client.close();
        m_midiConnected = false;
        setAudioStatus(QStringLiteral("AUDIO OFFLINE"));
        m_retryTimer.start();
    }

    if (m_client.isOpen() && m_client.takePortsChanged())
        connectMidiInput();

    if (m_client.isOpen()) {
        const bool midi = m_client.midiInputConnected();
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

    // While offline the engine doesn't run, so it can't report a step.
    const int step = m_client.isOpen() ? m_engine.currentStep() : -1;
    const bool beat = step >= 0 && m_engine.onBeat();
    if (step != m_currentStep || beat != m_beat) {
        m_currentStep = step;
        m_beat = beat;
        emit currentStepChanged();
    }

    // Values the audio thread changed (MIDI CCs, from M6) need the knobs to follow.
    if (const auto changed = m_engine.params().takeChangedByAudio(); changed != 0) {
        if (changed & (std::uint64_t(1) << int(k303::ParamId::Tempo)))
            syncTempo(); // a MIDI-mapped tempo knob
        emit paramsChanged();
    }

    // Meter: peak in dB mapped to 0..1 over kMeterFloorDb..0, with a steady fall.
    const float peak = m_engine.takePeak();
    const qreal db = peak > 0.0f ? 20.0 * std::log10(qreal(peak)) : kMeterFloorDb;
    const qreal target = qBound(0.0, 1.0 - db / kMeterFloorDb, 1.0);
    const qreal level = std::max(target, m_outputLevel - kMeterFallPerPoll);
    if (!qFuzzyCompare(level + 1.0, m_outputLevel + 1.0)) {
        m_outputLevel = level;
        emit outputLevelChanged();
    }
}

void SynthController::setAudioStatus(const QString &status)
{
    // Connection state, rate and xruns are all covered by this one notify signal.
    m_audioStatus = status;
    emit audioStatusChanged();
}
