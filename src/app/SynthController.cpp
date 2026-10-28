#include "app/SynthController.h"

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

SynthController::SynthController(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &SynthController::poll);
    m_pollTimer.start();

    m_retryTimer.setInterval(kRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &SynthController::connectAudio);

    connectAudio();
}

SynthController::~SynthController()
{
    m_engine.setRunning(false);
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
    if (playing == m_engine.isRunning())
        return;
    m_engine.setRunning(playing);
    emit playingChanged();
}

void SynthController::setParam(k303::ParamId id, qreal value)
{
    const float v = float(qBound(0.0, value, 1.0));
    if (v == m_engine.params().get(id))
        return;
    m_engine.params().set(id, v);
    emit paramsChanged();
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
    return id ? k303::paramInfo(*id).defaultValue : 0.0;
}

QString SynthController::displayText(const QString &key, qreal value) const
{
    namespace u = k303::units;
    const auto id = k303::paramFromKey(key.toStdString());
    if (!id)
        return {};
    const float v = float(qBound(0.0, value, 1.0));

    switch (*id) {
    case k303::ParamId::Tuning: {
        const double st = u::tuningSemitones(v);
        QString text = QString::number(st, 'f', 1);
        if (st >= 0.05)
            text.prepend(u'+');
        return text + QStringLiteral(" st");
    }
    case k303::ParamId::Cutoff: {
        const double hz = u::cutoffHz(v);
        return hz >= 1000.0 ? QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 2)
                            : QStringLiteral("%1 Hz").arg(qRound(hz));
    }
    case k303::ParamId::EnvMod:
        return QStringLiteral("%1 oct").arg(u::envModOctaves(v), 0, 'f', 1);
    case k303::ParamId::Decay: {
        const double s = u::decaySeconds(v);
        return s >= 1.0 ? QStringLiteral("%1 s").arg(s, 0, 'f', 2)
                        : QStringLiteral("%1 ms").arg(qRound(s * 1000.0));
    }
    case k303::ParamId::Waveform:
        return u::waveformIsSquare(v) ? QStringLiteral("Square") : QStringLiteral("Saw");
    case k303::ParamId::Volume: {
        const double gain = u::volumeGain(v);
        return gain <= 0.0 ? QStringLiteral("-inf dB")
                           : QStringLiteral("%1 dB").arg(20.0 * std::log10(gain), 0, 'f', 1);
    }
    case k303::ParamId::Tempo:
        return QStringLiteral("%1 BPM").arg(qRound(u::tempoBpm(v)));
    case k303::ParamId::Resonance:
    case k303::ParamId::Accent:
    case k303::ParamId::Count:
        break;
    }
    return QStringLiteral("%1%").arg(qRound(v * 100.0f));
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
    if (step != m_currentStep) {
        m_currentStep = step;
        emit currentStepChanged();
    }

    // Values the audio thread changed (MIDI CCs, from M6) need the knobs to follow.
    if (m_engine.params().takeChangedByAudio() != 0)
        emit paramsChanged();

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
