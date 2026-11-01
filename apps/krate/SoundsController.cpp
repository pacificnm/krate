#include "SoundsController.h"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QFileInfo>
#include <QJSEngine>

#include "RackController.h"
#include "app/InstrumentRegistry.h"
#include "dsp/Wav.h"
#include "rack/Preview.h"
#include "song/BackingTrack.h"

struct SoundsController::Decoded
{
    int id = -1;
    krate::PreviewSound sound;
};

SoundsController::SoundsController(const QString &databaseFile, krate::Previewer &previewer, RackController &rack,
                                   std::vector<Device> devices, QObject *parent)
    : QObject(parent)
    , m_library(databaseFile)
    , m_previewer(previewer)
    , m_rack(rack)
    , m_devices(std::move(devices))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &SoundsController::freeRetired);
    m_timer.start();
    if (!m_library.isOpen())
        m_message = QStringLiteral("The sound library couldn't open its database.");
}

SoundsController::~SoundsController()
{
    finish();
    s_instance = nullptr;
}

SoundsController *SoundsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void SoundsController::finish()
{
    m_quit.store(true);
    if (m_worker.joinable())
        m_worker.join();
    m_previewer.stop();
    m_previewing = -1;
}

QVariantList SoundsController::categoryCounts() const
{
    QVariantList list;
    for (int c : m_library.categoryCounts())
        list.append(c);
    return list;
}

QVariantList SoundsController::list(int category, bool favourites, const QString &search) const
{
    SoundLibrary::Query q;
    q.category = category;
    q.favourites = favourites;
    q.search = search;
    return m_library.sounds(q);
}

void SoundsController::addFolder(const QUrl &folder)
{
    addFolderPath(folder.isLocalFile() ? folder.toLocalFile() : folder.toString());
}

void SoundsController::addFolderPath(const QString &folder)
{
    if (!QFileInfo(folder).isDir()) {
        m_message = QStringLiteral("Not a folder: %1").arg(folder);
        bump();
        return;
    }
    if (!m_library.addFolder(folder)) {
        m_message = QStringLiteral("%1 is in the library already.").arg(QDir(folder).absolutePath());
        bump();
        return;
    }
    rescan();
}

void SoundsController::removeFolder(const QString &folder)
{
    m_library.removeFolder(folder);
    m_message = QStringLiteral("Removed %1 (the files are where they were).").arg(folder);
    bump();
}

void SoundsController::rescan()
{
    if (m_scanning.exchange(true))
        return; // one at a time
    if (m_worker.joinable())
        m_worker.join();
    m_message = QStringLiteral("Looking through your folders…");
    bump();
    m_worker = std::thread(&SoundsController::scan, this, m_library.folders());
}

void SoundsController::scan(QStringList folders)
{
    for (const QString &folder : folders) {
        if (m_quit.load())
            break;
        std::vector<sounds::FileInfo> files;
        for (const QString &path : sounds::findSoundFiles(folder)) {
            if (m_quit.load())
                break;
            files.push_back(sounds::soundInfo(path));
        }
        // Into the database on the GUI thread (its connection lives there).
        QMetaObject::invokeMethod(this, [this, folder, files = std::move(files)] {
            const int n = m_library.update(folder, files);
            m_message = QStringLiteral("%1: %2 sounds").arg(QFileInfo(folder).fileName()).arg(n);
            bump();
        }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, [this] {
        m_scanning.store(false);
        m_message = QStringLiteral("%1 sounds in the library.").arg(m_library.total());
        bump();
    }, Qt::QueuedConnection);
}

void SoundsController::setCategory(int id, int category)
{
    m_library.setCategory(id, category);
    bump();
}

void SoundsController::setFavourite(int id, bool favourite)
{
    m_library.setFavourite(id, favourite);
    bump();
}

std::shared_ptr<const SoundsController::Decoded> SoundsController::decode(int id)
{
    if (m_decoded && m_decoded->id == id)
        return m_decoded;
    const QVariantMap s = m_library.sound(id);
    if (s.isEmpty())
        return nullptr;
    const QString path = s.value(QStringLiteral("path")).toString();
    auto decoded = std::make_shared<Decoded>();
    decoded->id = id;
    std::string why;
    if (path.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)) {
        krate::song::AudioClip clip;
        if (!krate::song::decodeMp3(path.toStdString(), &clip, &why)) {
            m_message = QStringLiteral("Can't play %1: %2").arg(QFileInfo(path).fileName(), QString::fromStdString(why));
            return nullptr;
        }
        decoded->sound.rate = clip.rate;
        decoded->sound.left.resize(clip.frames());
        decoded->sound.right.resize(clip.frames());
        for (std::size_t i = 0; i < clip.frames(); ++i) {
            decoded->sound.left[i] = float(clip.samples[2 * i]) / 32768.0f;
            decoded->sound.right[i] = float(clip.samples[2 * i + 1]) / 32768.0f;
        }
        m_library.setSeconds(id, double(clip.frames()) / std::max(clip.rate, 1));
    } else {
        krate::WavData wav;
        if (!krate::readWavFile(path.toStdString(), &wav, &why)) {
            m_message = QStringLiteral("Can't play %1: %2").arg(QFileInfo(path).fileName(), QString::fromStdString(why));
            return nullptr;
        }
        decoded->sound.rate = wav.rate;
        decoded->sound.left = std::move(wav.left);
        decoded->sound.right = std::move(wav.right);
    }
    m_decoded = decoded;
    return decoded;
}

void SoundsController::preview(int id)
{
    const auto decoded = decode(id);
    if (!decoded) {
        bump();
        return;
    }
    retirePlaying();
    m_playing = decoded;
    m_previewer.play(&m_playing->sound);
    m_playStart = m_previewer.blocks();
    m_previewing = id;
    emit previewChanged();
}

void SoundsController::retirePlaying()
{
    if (!m_playing)
        return;
    // Kept until the audio thread can't be reading it.
    Retired r{std::move(m_playing), m_previewer.blocks(), {}};
    r.since.start();
    m_retired.push_back(std::move(r));
}

void SoundsController::stopPreview()
{
    m_previewer.stop();
    retirePlaying();
    if (m_previewing >= 0) {
        m_previewing = -1;
        emit previewChanged();
    }
}

void SoundsController::freeRetired()
{
    const std::uint64_t now = m_previewer.blocks();
    std::erase_if(m_retired, [now](const Retired &r) { return now >= r.block + 2 || r.since.elapsed() > 2000; });
    // Played to its end: nothing playing now.
    if (m_previewing >= 0 && now >= m_playStart + 2 && !m_previewer.playing()) {
        m_previewing = -1;
        emit previewChanged();
    }
}

QVariantList SoundsController::waveform(int id, int points)
{
    QVariantList list;
    const auto decoded = decode(id);
    if (!decoded || points < 1)
        return list;
    const krate::PreviewSound &s = decoded->sound;
    const std::size_t n = s.frames();
    std::vector<float> peaks(std::size_t(points), 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        float v = std::fabs(s.left[i]);
        if (!s.right.empty())
            v = std::max(v, std::fabs(s.right[i]));
        float &bin = peaks[std::min<std::size_t>(i * std::size_t(points) / std::max<std::size_t>(n, 1), std::size_t(points) - 1)];
        bin = std::max(bin, std::min(v, 1.0f));
    }
    for (float p : peaks)
        list.append(p);
    return list;
}

krate::InstrumentInstance *SoundsController::instanceOf(int device) const
{
    for (const Device &d : m_devices)
        if (d.device == device)
            return d.instance;
    return nullptr;
}

QVariantList SoundsController::instruments() const
{
    QVariantList list;
    for (const Device &d : m_devices) {
        if (!m_rack.isPresent(d.device) || d.instance->sampleTargets().isEmpty())
            continue;
        list.append(QVariantMap{{QStringLiteral("device"), d.device},
                                {QStringLiteral("name"), d.name},
                                {QStringLiteral("targets"), d.instance->sampleTargets()},
                                {QStringLiteral("contents"), d.instance->sampleTargetContents()}});
    }
    return list;
}

bool SoundsController::assign(int id, int device, int target)
{
    krate::InstrumentInstance *instance = instanceOf(device);
    const QVariantMap s = m_library.sound(id);
    if (!instance || s.isEmpty())
        return false;
    QString error;
    const QString name = s.value(QStringLiteral("name")).toString();
    const QStringList targets = instance->sampleTargets();
    const bool ok = instance->assignSample(target, s.value(QStringLiteral("path")).toString(), &error);
    m_message = ok ? QStringLiteral("%1 is on %2's %3.").arg(name, m_devices.empty() ? QString() : [&] {
                         for (const Device &d : m_devices)
                             if (d.device == device)
                                 return d.name;
                         return QString();
                     }(), targets.value(target))
                   : QStringLiteral("Couldn't put %1 there: %2").arg(name, error);
    bump();
    return ok;
}

void SoundsController::clearTarget(int device, int target)
{
    if (krate::InstrumentInstance *instance = instanceOf(device)) {
        instance->clearSample(target);
        bump();
    }
}

void SoundsController::bump()
{
    ++m_revision;
    emit changed();
}
