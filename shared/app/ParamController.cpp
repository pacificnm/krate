#include "app/ParamController.h"

#include <QLoggingCategory>

#include "app/PanelFile.h"
#include "core/Instrument.h"

Q_LOGGING_CATEGORY(lcParams, "krate.params")

namespace krate {

namespace {
constexpr int kPollIntervalMs = 33; // ~30 Hz: knobs following MIDI
constexpr int kSaveDelayMs = 2000;
} // namespace

ParamController::ParamController(IInstrument &instrument, const QString &panelFilePath, QObject *parent)
    : QObject(parent)
    , m_instrument(instrument)
    , m_panelFilePath(panelFilePath)
{
    files::PanelValues values;
    if (files::loadPanel(m_panelFilePath, m_instrument.parameters(), &values) == files::LoadResult::NewerVersion)
        m_panelFilePath += QStringLiteral(".v1"); // leave a newer file alone
    for (std::size_t i = 0; i < values.size(); ++i)
        m_instrument.setParam(int(i), values[i]);

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ParamController::poll);
    m_pollTimer.start();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &ParamController::save);
}

ParamController::~ParamController()
{
    if (m_saveTimer.isActive())
        save();
}

int ParamController::indexOf(const QString &key) const
{
    const auto params = m_instrument.parameters();
    for (std::size_t i = 0; i < params.size(); ++i)
        if (key == QLatin1StringView(params[i].key.data(), qsizetype(params[i].key.size())))
            return int(i);
    return -1;
}

qreal ParamController::value(const QString &key) const
{
    const int i = indexOf(key);
    return i >= 0 ? m_instrument.param(i) : 0.0;
}

void ParamController::setValue(const QString &key, qreal value)
{
    const int i = indexOf(key);
    if (i < 0) {
        qCWarning(lcParams) << "unknown parameter" << key;
        return;
    }
    const float v = float(qBound(0.0, value, 1.0));
    if (v == m_instrument.param(i))
        return;
    m_instrument.setParam(i, v);
    changed();
}

qreal ParamController::defaultValue(const QString &key) const
{
    const int i = indexOf(key);
    return i >= 0 ? m_instrument.parameters()[std::size_t(i)].defaultValue : 0.0;
}

QString ParamController::label(const QString &key) const
{
    const int i = indexOf(key);
    if (i < 0)
        return {};
    const std::string_view l = m_instrument.parameters()[std::size_t(i)].label;
    return QString::fromUtf8(l.data(), qsizetype(l.size()));
}

QString ParamController::displayText(const QString &key, qreal value) const
{
    const int i = indexOf(key);
    if (i < 0)
        return {};
    return QString::fromStdString(m_instrument.parameters()[std::size_t(i)].format(float(qBound(0.0, value, 1.0))));
}

void ParamController::poll()
{
    if (m_instrument.takeParamsChangedByAudio() != 0)
        changed(); // a MIDI-mapped knob moved
}

void ParamController::changed()
{
    ++m_revision;
    emit paramsChanged();
    m_saveTimer.start();
}

void ParamController::save()
{
    m_saveTimer.stop();
    const auto params = m_instrument.parameters();
    files::PanelValues values(params.size(), 0.0f);
    for (std::size_t i = 0; i < params.size(); ++i)
        values[i] = m_instrument.param(int(i));
    if (!files::savePanel(m_panelFilePath, params, values))
        qCWarning(lcParams) << "could not save panel settings to" << m_panelFilePath;
}

} // namespace krate
