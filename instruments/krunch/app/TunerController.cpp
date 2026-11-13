#include "app/TunerController.h"

#include <cmath>

#include <QJsonObject>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "dsp/Krunch.h"

Q_LOGGING_CATEGORY(lcTuner, "krunch.tuner")

namespace {
constexpr int kVersion = 1;
constexpr int kPollMs = 33;          // ~30 readings a second
constexpr int kHoldPolls = 12;       // ~0.4 s: the reading stays up between a note's dips
constexpr double kSmoothing = 0.35;  // of each new reading, when it's near the last
constexpr double kNewNote = 0.4;     // semitones: further than this, a new note (no smoothing)
} // namespace

TunerController::TunerController(krunch::Krunch &engine, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_filePath(filePath)
    , m_tuning(&krunch::tuner::tuning(krunch::tuner::kDefaultTuning))
{
    m_pollTimer.setInterval(kPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &TunerController::poll);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(1000);
    connect(&m_saveTimer, &QTimer::timeout, this, &TunerController::saveFile);

    QJsonObject json;
    if (krate::files::readVersionedJson(m_filePath, kVersion, &json) == krate::files::LoadResult::Loaded) {
        m_tuning = &krunch::tuner::tuning(json.value(u"tuning").toString().toStdString());
        m_referenceHz = qBound(minReferenceHz(), json.value(u"referenceHz").toDouble(m_referenceHz), maxReferenceHz());
        m_mute = json.value(u"mute").toBool(m_mute);
    }
}

TunerController::~TunerController()
{
    m_engine.tunerTap().setOpen(false);
    m_engine.setMuted(false);
    if (m_saveTimer.isActive())
        saveFile();
}

void TunerController::setOpen(bool open)
{
    if (open == m_open)
        return;
    m_open = open;
    m_engine.tunerTap().setOpen(open);
    updateMute();
    m_smoothed = -1.0;
    m_misses = kHoldPolls;
    if (open) {
        m_pollTimer.start();
    } else {
        m_pollTimer.stop();
        if (m_hasPitch) {
            m_hasPitch = false;
            emit readingChanged();
        }
    }
    emit openChanged();
}

void TunerController::setMute(bool mute)
{
    if (mute == m_mute)
        return;
    m_mute = mute;
    updateMute();
    settingsEdited();
}

void TunerController::updateMute() { m_engine.setMuted(m_open && m_mute); }

void TunerController::setReferenceHz(qreal hz)
{
    hz = std::round(qBound(minReferenceHz(), hz, maxReferenceHz()) * 10.0) / 10.0;
    if (qFuzzyCompare(hz, m_referenceHz))
        return;
    m_referenceHz = hz;
    settingsEdited();
}

void TunerController::setTuning(const QString &id)
{
    const krunch::tuner::Tuning *t = &krunch::tuner::tuning(id.toStdString());
    if (t == m_tuning)
        return;
    m_tuning = t;
    settingsEdited();
}

void TunerController::settingsEdited()
{
    m_smoothed = -1.0; // read afresh against the new strings or reference
    m_saveTimer.start();
    emit settingsChanged();
}

QVariantList TunerController::tunings() const
{
    QVariantList result;
    for (const krunch::tuner::Tuning &t : krunch::tuner::tunings())
        result << QVariantMap{{QStringLiteral("id"), QString::fromUtf8(t.id)},
                              {QStringLiteral("name"), QString::fromUtf8(t.name)},
                              {QStringLiteral("group"), QString::fromUtf8(t.group)}};
    return result;
}

QVariantList TunerController::strings() const
{
    QVariantList result;
    for (int s = 0; s < m_tuning->count; ++s) {
        const int note = m_tuning->notes[std::size_t(s)];
        result << QVariantMap{{QStringLiteral("name"), QString::fromStdString(krunch::tuner::noteName(note, m_tuning->flats))},
                              {QStringLiteral("octave"), krunch::tuner::octave(note)},
                              {QStringLiteral("hz"), krunch::tuner::noteHz(note, m_referenceHz)}};
    }
    return result;
}

void TunerController::poll()
{
    const double rate = m_engine.sampleRate();
    if (rate != m_detector.sampleRate()) {
        m_detector.prepare(rate);
        m_samples.assign(m_detector.windowSize(), 0.0f);
    }
    double hz = 0.0;
    if (m_engine.tunerTap().read(m_samples))
        hz = m_detector.detect(m_samples).hz;

    if (hz <= 0.0) {
        if (++m_misses >= kHoldPolls) {
            m_smoothed = -1.0;
            if (m_hasPitch) {
                m_hasPitch = false;
                emit readingChanged();
            }
        }
        return;
    }
    m_misses = 0;
    const double played = krunch::tuner::noteOf(hz, m_referenceHz);
    if (m_smoothed < 0.0 || std::fabs(played - m_smoothed) > kNewNote)
        m_smoothed = played;
    else
        m_smoothed += kSmoothing * (played - m_smoothed);

    m_frequency = krunch::tuner::noteHz(m_smoothed, m_referenceHz);
    const krunch::tuner::Reading r = krunch::tuner::read(*m_tuning, m_frequency, m_referenceHz);
    m_note = QString::fromStdString(krunch::tuner::noteName(r.note, m_tuning->flats));
    m_octave = krunch::tuner::octave(r.note);
    m_string = r.string;
    m_cents = r.cents;
    m_hasPitch = true;
    emit readingChanged();
}

void TunerController::saveFile()
{
    m_saveTimer.stop();
    if (!krate::files::writeJsonFile(m_filePath, QJsonObject{{QStringLiteral("version"), kVersion},
                                                             {QStringLiteral("tuning"), tuning()},
                                                             {QStringLiteral("referenceHz"), m_referenceHz},
                                                             {QStringLiteral("mute"), m_mute}}))
        qCWarning(lcTuner) << "could not save" << m_filePath;
}
