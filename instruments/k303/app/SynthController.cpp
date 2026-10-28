#include "app/SynthController.h"

#include "app/PanelFile.h"

#include <QJSEngine>
#include <QLoggingCategory>
#include <QtMath>

Q_LOGGING_CATEGORY(lcSynth, "k303.synth")

namespace {
constexpr int kPollIntervalMs = 16; // ~60 Hz
} // namespace

SynthController::SynthController(const QString &panelFilePath, krate::Transport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_panelFilePath(panelFilePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Knob positions from last time (missing or unreadable file: defaults).
    krate::files::PanelValues values;
    if (krate::files::loadPanel(m_panelFilePath, k303::kParams, &values) == krate::files::LoadResult::NewerVersion)
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
}

SynthController::~SynthController()
{
    if (m_saveTimer.isActive())
        savePanel();
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
    if (!m_transport || playing == m_transport->playing())
        return;
    m_transport->setPlaying(playing);
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
    if (m_transport)
        m_transport->setBpm(k303::units::tempoBpm(m_engine.params().get(k303::ParamId::Tempo)));
}

void SynthController::setParamByKey(const QString &key, qreal value)
{
    if (const auto id = k303::paramFromKey(key.toStdString()))
        setParam(*id, value);
    else
        qCWarning(lcSynth) << "unknown parameter" << key;
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

void SynthController::savePanel()
{
    m_saveTimer.stop();
    krate::files::PanelValues values(std::size_t(k303::ParamCount), 0.0f);
    for (int i = 0; i < k303::ParamCount; ++i)
        values[std::size_t(i)] = m_engine.param(i);
    if (!krate::files::savePanel(m_panelFilePath, k303::kParams, values))
        qCWarning(lcSynth) << "could not save panel settings to" << m_panelFilePath;
}

void SynthController::poll()
{
    const int step = m_engine.currentStep();
    const bool beat = step >= 0 && m_engine.onBeat();
    if (step != m_currentStep || beat != m_beat) {
        m_currentStep = step;
        m_beat = beat;
        emit currentStepChanged();
    }

    // The engine's own audio->GUI events (MIDI keys).
    while (const auto ev = m_engine.popEvent())
        emit notePressed(ev->a, ev->b);

    // Values the audio thread changed (MIDI CCs) need the knobs to follow.
    if (const auto changed = m_engine.params().takeChangedByAudio(); changed != 0) {
        if (changed & (std::uint64_t(1) << int(k303::ParamId::Tempo)))
            syncTempo(); // a MIDI-mapped tempo knob
        emit paramsChanged();
    }
}
