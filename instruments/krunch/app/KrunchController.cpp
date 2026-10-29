#include "app/KrunchController.h"

#include <cmath>

#include <QJSEngine>

#include "dsp/Krunch.h"

namespace {
constexpr int kPollIntervalMs = 16;         // ~60 Hz
constexpr qreal kMeterFloorDb = -60.0;
constexpr qreal kMeterFallPerPoll = 0.015;  // ~1 s from full scale to empty
constexpr float kClipPeak = 0.98f;          // about -0.2 dBFS: the interface is clipping
constexpr int kClipHoldPolls = 60;          // ~1 s

qreal meterLevel(float peak, qreal previous)
{
    const qreal db = peak > 0.0f ? 20.0 * std::log10(qreal(peak)) : kMeterFloorDb;
    const qreal target = qBound(0.0, 1.0 - db / kMeterFloorDb, 1.0);
    return std::max(target, previous - kMeterFallPerPoll);
}
} // namespace

KrunchController::KrunchController(krunch::Krunch &engine, const QString &panelFilePath, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_meterTimer.setInterval(kPollIntervalMs);
    connect(&m_meterTimer, &QTimer::timeout, this, &KrunchController::pollMeters);
    m_meterTimer.start();
}

KrunchController::~KrunchController()
{
    s_instance = nullptr;
}

KrunchController *KrunchController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void KrunchController::pollMeters()
{
    const float inPeak = m_engine.takeInputPeak();
    const qreal in = meterLevel(inPeak, m_inputLevel);
    const qreal out = meterLevel(m_engine.takeOutputPeak(), m_outputLevel);
    const bool wasClipping = clipping();
    m_clipFrames = inPeak >= kClipPeak ? kClipHoldPolls : std::max(0, m_clipFrames - 1);
    const bool gate = m_engine.gateClosed();
    if (!qFuzzyCompare(in + 1.0, m_inputLevel + 1.0) || !qFuzzyCompare(out + 1.0, m_outputLevel + 1.0)
        || clipping() != wasClipping || gate != m_gateClosed) {
        m_gateClosed = gate;
        m_inputLevel = in;
        m_outputLevel = out;
        emit metersChanged();
    }
}
