#include "ExportController.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QUrl>

#include "ArrangeController.h"
#include "AudioTracks.h"
#include "RackController.h"
#include "SequencerController.h"
#include "core/Transport.h"
#include "host/JackProcessor.h"
#include "rack/Automation.h"
#include "rack/Rack.h"
#include "rec/Bounce.h"
#include "rec/WavWriter.h"
#include "seq/Sequencer.h"

namespace {
constexpr float kSilentStem = 1e-5f; // a stem quieter than this throughout (-100 dB) isn't kept

// A name fit for a file: no slashes or the like.
QString fileName(QString name)
{
    static const QString bad = QStringLiteral("/\\:*?\"<>|");
    for (QChar &c : name)
        if (bad.contains(c) || c.unicode() < 32)
            c = u'-';
    return name.trimmed();
}
} // namespace

ExportController::ExportController(krate::Rack &rack, krate::JackProcessor &processor, krate::Transport &transport,
                                   krate::seq::Sequencer &sequencer, krate::AutomationPlayer *automation,
                                   RackController &rackController, SequencerController &sequencerController,
                                   ArrangeController &arranger, AudioTracksController *tracks, QString folder, QObject *parent)
    : QObject(parent)
    , m_rack(rack)
    , m_processor(processor)
    , m_transport(transport)
    , m_sequencer(sequencer)
    , m_automation(automation)
    , m_rackController(rackController)
    , m_sequencerController(sequencerController)
    , m_arranger(arranger)
    , m_tracks(tracks)
    , m_folder(std::move(folder))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_klick = int(m_rackController.deviceIds().indexOf(QStringLiteral("klick")));
    m_pollTimer.setInterval(50);
    connect(&m_pollTimer, &QTimer::timeout, this, &ExportController::poll);
}

ExportController::~ExportController()
{
    if (m_worker.joinable()) { // quitting mid-export
        m_cancel.store(true);
        m_worker.join();
        finish();
    }
    s_instance = nullptr;
}

ExportController *ExportController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

qreal ExportController::songSeconds() const
{
    const double bpm = std::max(m_transport.bpm(), 1.0);
    return m_arranger.songEndBeat(bpm) * 60.0 / bpm;
}

void ExportController::openFolder() const
{
    QDir().mkpath(m_folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_folder));
}

bool ExportController::start(bool loopRange, bool stems, bool metronome)
{
    if (m_busy)
        return false;
    const double from = loopRange ? double(m_arranger.loopFrom()) * 4.0 : 0.0;
    const double to = loopRange ? double(m_arranger.loopTo()) * 4.0 : m_arranger.songEndBeat(m_transport.bpm());
    if (to <= from) {
        m_message = QStringLiteral("Nothing to export: the arrangement is empty. Lay out the song in the arranger first.");
        emit changed();
        return false;
    }

    // Where: ~/Music/Krate/<sequence> <date time>.wav, the stems in a folder beside it.
    QString base = fileName(m_sequencerController.state().sequenceName);
    if (base.isEmpty())
        base = QStringLiteral("Krate song");
    base += QDateTime::currentDateTime().toString(QStringLiteral(" yyyy-MM-dd HHmmss"));
    if (loopRange)
        base += QStringLiteral(" bars %1-%2").arg(m_arranger.loopFrom() + 1).arg(m_arranger.loopTo());
    std::vector<Job> jobs{{QDir(m_folder).filePath(base + QStringLiteral(".wav")), -1}};
    if (stems) {
        const QStringList names = m_rackController.deviceNames();
        int n = 0;
        for (int d = 0; d < m_rack.deviceCount(); ++d) {
            if (!m_rack.present(d) || (d == m_klick && !metronome))
                continue;
            jobs.push_back({QDir(m_folder).filePath(QStringLiteral("%1 stems/%2 %3.wav")
                                                        .arg(base)
                                                        .arg(++n, 2, 10, QLatin1Char('0'))
                                                        .arg(fileName(names.value(d)))),
                            d});
        }
    }

    // Live play stops; recording and AUTO wait; the host lets go of the rack.
    m_rackController.setPlaying(false);
    m_wasRecording = m_tracks && m_tracks->recording();
    if (m_tracks)
        m_tracks->setRecording(false);
    m_wasAutoWrite = m_arranger.autoWrite();
    m_arranger.setAutoWrite(false);
    if (m_host.suspend)
        m_host.suspend();
    // The arrangement plays (SONG), its automation too; the metronome only if asked.
    m_sequencer.setSongMode(true);
    if (m_automation)
        m_automation->setActive(true);
    if (m_klick >= 0 && !metronome)
        m_rack.setMute(m_klick, true);

    const double rate = m_host.sampleRate && m_host.sampleRate() > 0 ? m_host.sampleRate() : 48000.0;
    m_busy = true;
    m_cancel.store(false);
    m_done.store(false);
    m_progress.store(0.0);
    m_shownProgress = 0.0;
    m_written.clear();
    m_failed.clear();
    m_message = QStringLiteral("Exporting…");
    emit changed();
    emit progressChanged();
    std::vector<bool> solos;
    for (int d = 0; d < m_rack.deviceCount(); ++d)
        solos.push_back(m_rackController.solo(d));
    m_worker = std::thread(&ExportController::run, this, std::move(jobs), std::move(solos), from, to, rate);
    m_pollTimer.start();
    return true;
}

void ExportController::run(std::vector<Job> jobs, std::vector<bool> solos, double fromBeat, double toBeat, double rate)
{
    for (std::size_t j = 0; j < jobs.size() && !m_cancel.load(); ++j) {
        const Job &job = jobs[j];
        for (int d = 0; d < m_rack.deviceCount(); ++d)
            m_rack.setSolo(d, job.solo >= 0 ? d == job.solo : bool(solos[std::size_t(d)]));
        m_rack.setMasterStage(job.solo < 0); // stems without it: they add up to the mix before it
        m_rack.prepare(rate); // every pass from silence
        QDir().mkpath(QFileInfo(job.path).absolutePath());
        krate::WavWriter writer;
        if (!writer.open(job.path.toStdString(), int(rate), 2)) {
            std::lock_guard lock(m_mutex);
            m_failed = job.path;
            break;
        }
        float peak = 0.0f;
        const std::int64_t frames = krate::bounce(
            m_processor, m_transport, rate, krate::BounceRange{fromBeat, toBeat},
            [&](const float *l, const float *r, std::uint32_t n) {
                for (std::uint32_t i = 0; i < n; ++i)
                    peak = std::max({peak, std::fabs(l[i]), std::fabs(r[i])});
                return writer.write(l, r, n);
            },
            [&](double p) {
                m_progress.store((double(j) + p) / double(jobs.size()));
                return !m_cancel.load();
            });
        const bool ok = writer.close() && frames >= 0;
        if (!ok || (job.solo >= 0 && peak < kSilentStem)) { // stopped, or a stem with nothing in it
            QFile::remove(job.path);
            continue;
        }
        std::lock_guard lock(m_mutex);
        m_written << job.path;
    }
    for (int d = 0; d < m_rack.deviceCount(); ++d)
        m_rack.setSolo(d, solos[std::size_t(d)]);
    m_rack.setMasterStage(true);
    m_rack.prepare(rate); // no tails left over for live play
    m_done.store(true);
}

void ExportController::poll()
{
    if (const qreal p = m_progress.load(); p != m_shownProgress) {
        m_shownProgress = p;
        emit progressChanged();
    }
    if (m_done.load()) {
        m_worker.join();
        finish();
    }
}

void ExportController::finish()
{
    m_pollTimer.stop();
    // Everything as it was.
    for (int d = 0; d < m_rack.deviceCount(); ++d)
        m_rack.setMute(d, m_rackController.mute(d));
    const bool song = m_sequencerController.state().songMode;
    m_sequencer.setSongMode(song);
    if (m_automation)
        m_automation->setActive(song);
    m_arranger.reapply();
    m_arranger.setAutoWrite(m_wasAutoWrite);
    if (m_tracks)
        m_tracks->setRecording(m_wasRecording);
    if (m_host.resume)
        m_host.resume();

    std::lock_guard lock(m_mutex);
    m_files = m_written;
    if (!m_failed.isEmpty())
        m_message = QStringLiteral("Couldn't write %1").arg(m_failed);
    else if (m_cancel.load())
        m_message = QStringLiteral("Export stopped.");
    else
        m_message = m_files.size() > 1 ? QStringLiteral("Exported %1 and %2 stems.").arg(QFileInfo(m_files.first()).fileName()).arg(m_files.size() - 1)
                                       : QStringLiteral("Exported %1.").arg(QFileInfo(m_files.value(0)).fileName());
    m_busy = false;
    m_shownProgress = 1.0;
    emit progressChanged();
    emit changed();
}
