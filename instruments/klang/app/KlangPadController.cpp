#include "klang/app/KlangPadController.h"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "dsp/Wav.h"
#include "klang/app/KlangController.h"
#include "klang/dsp/Engine.h"
#include "klang/dsp/Params.h"
#include "song/BackingTrack.h"

Q_LOGGING_CATEGORY(lcKlangPads, "klang.pads")

namespace {
constexpr int kFileVersion = 1;
constexpr int kSaveDelayMs = 2000;

QString keyOf(int index)
{
    const auto k = klang::kParams[std::size_t(index)].key;
    return QString::fromUtf8(k.data(), qsizetype(k.size()));
}

bool sameFile(const QString &a, const QString &b)
{
    QFile fa(a), fb(b);
    return fa.size() == fb.size() && fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly)
           && fa.readAll() == fb.readAll();
}
} // namespace

using klang::ParamId;

KlangPadController::KlangPadController(KlangController &panel, klang::Engine &engine, const QString &dataDir,
                                       QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_engine(engine)
    , m_libraryDir(dataDir + QStringLiteral("/samples"))
    , m_path(dataDir + QStringLiteral("/pads.json"))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    rescan();
    load();

    // A pad control moved: pads.json follows (only the edited pad's are in panel.json).
    connect(&m_panel, &krate::ParamController::paramsChanged, this, [this] { m_saveTimer.start(); });
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &KlangPadController::save);
    m_freeTimer.setInterval(100);
    connect(&m_freeTimer, &QTimer::timeout, this, &KlangPadController::freeRetired);
    m_freeTimer.start();
}

KlangPadController::~KlangPadController()
{
    if (m_saveTimer.isActive())
        save();
    for (int pad = 0; pad < kPads; ++pad)
        m_engine.setSample(pad, nullptr); // the host has stopped processing by now
    s_instance = nullptr;
}

KlangPadController *KlangPadController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

// --- Files ----------------------------------------------------------------------------

KlangPadController::SamplePtr KlangPadController::decode(const QString &path, QString *error)
{
    const QString key = QFileInfo(path).canonicalFilePath().isEmpty() ? path : QFileInfo(path).canonicalFilePath();
    if (const auto it = m_decoded.find(key); it != m_decoded.end())
        if (SamplePtr shared = it->second.lock())
            return shared;
    const QString name = QFileInfo(path).fileName();
    std::string why;
    SamplePtr sample;
    if (QFileInfo(path).suffix().compare(QStringLiteral("mp3"), Qt::CaseInsensitive) == 0) {
        krate::song::AudioClip clip;
        if (!krate::song::decodeMp3(path.toStdString(), &clip, &why)) {
            *error = QString::fromStdString(why);
            return nullptr;
        }
        std::vector<float> left(clip.frames()), right(clip.frames());
        bool mono = true;
        for (std::size_t i = 0; i < clip.frames(); ++i) {
            left[i] = float(clip.samples[2 * i]) / 32768.0f;
            right[i] = float(clip.samples[2 * i + 1]) / 32768.0f;
            mono = mono && clip.samples[2 * i] == clip.samples[2 * i + 1];
        }
        if (mono)
            right.clear();
        sample = std::make_shared<const klang::Sample>(
            klang::Sample::fromChannels(std::move(left), std::move(right), double(clip.rate), name.toStdString()));
    } else {
        krate::WavData wav;
        if (!krate::readWavFile(path.toStdString(), &wav, &why)) {
            *error = QString::fromStdString(why);
            return nullptr;
        }
        sample = std::make_shared<const klang::Sample>(klang::Sample::fromChannels(
            std::move(wav.left), std::move(wav.right), double(wav.rate), name.toStdString()));
    }
    if (sample->frames() < 2) {
        *error = QStringLiteral("it is empty");
        return nullptr;
    }
    m_decoded[key] = sample;
    return sample;
}

QString KlangPadController::import(const QString &path, QString *error)
{
    const QFileInfo source(path);
    if (QFileInfo(source.absolutePath()).canonicalFilePath() == QFileInfo(m_libraryDir).canonicalFilePath())
        return source.fileName(); // already in the library
    QDir().mkpath(m_libraryDir);
    QString name = source.fileName();
    for (int n = 2; QFile::exists(m_libraryDir + u'/' + name); ++n) {
        if (sameFile(path, m_libraryDir + u'/' + name))
            return name; // the same file, imported before
        name = QStringLiteral("%1 (%2).%3").arg(source.completeBaseName()).arg(n).arg(source.suffix());
    }
    if (!QFile::copy(path, m_libraryDir + u'/' + name)) {
        *error = QStringLiteral("could not copy it into the library");
        return {};
    }
    return name;
}

void KlangPadController::rescan()
{
    m_library = QDir(m_libraryDir).entryList({QStringLiteral("*.wav"), QStringLiteral("*.WAV"), QStringLiteral("*.mp3"),
                                              QStringLiteral("*.MP3")},
                                             QDir::Files, QDir::Name | QDir::IgnoreCase);
}

void KlangPadController::use(int pad, const SamplePtr &sample, const QString &name)
{
    m_engine.setSample(pad, sample.get());
    if (m_samples[std::size_t(pad)])
        m_retired.push_back({m_samples[std::size_t(pad)], m_engine.blocksProcessed()});
    m_samples[std::size_t(pad)] = sample;
    m_names[std::size_t(pad)] = name;
}

void KlangPadController::freeRetired()
{
    const std::uint64_t now = m_engine.blocksProcessed();
    std::erase_if(m_retired, [now](const Retired &r) { return now >= r.block + 2; });
}

void KlangPadController::loadUrls(int pad, const QList<QUrl> &urls)
{
    QStringList loaded, failed;
    for (const QUrl &url : urls) {
        if (pad >= kPads)
            break;
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        QString error;
        if (!decode(path, &error)) { // check it plays before copying
            failed << QFileInfo(path).fileName() + QStringLiteral(" (") + error + u')';
            continue;
        }
        const QString name = import(path, &error);
        if (name.isEmpty() || !loadFromLibrary(pad, name)) {
            failed << QFileInfo(path).fileName() + QStringLiteral(" (") + error + u')';
            continue;
        }
        loaded << QStringLiteral("%1 on pad %2").arg(name).arg(pad + 1);
        ++pad;
    }
    rescan();
    m_message = failed.isEmpty() ? QStringLiteral("Loaded ") + loaded.join(QStringLiteral(", "))
                                 : QStringLiteral("Could not load ") + failed.join(QStringLiteral(", "));
    edited();
}

bool KlangPadController::loadFromLibrary(int pad, const QString &name)
{
    if (pad < 0 || pad >= kPads)
        return false;
    QString error;
    const SamplePtr sample = decode(m_libraryDir + u'/' + name, &error);
    if (!sample) {
        m_message = QStringLiteral("%1: %2").arg(name, error);
        emit changed();
        return false;
    }
    use(pad, sample, name);
    // A new sample plays whole from the start.
    m_engine.setPadParam(pad, ParamId::Start, 0.0f);
    m_engine.setPadParam(pad, ParamId::End, 1.0f);
    m_engine.reportAllChanged();
    edited();
    return true;
}

void KlangPadController::step(int pad, int delta)
{
    rescan();
    if (m_library.isEmpty() || pad < 0 || pad >= kPads)
        return;
    const int at = int(m_library.indexOf(m_names[std::size_t(pad)]));
    const int n = int(m_library.size());
    const int next = at < 0 ? (delta > 0 ? 0 : n - 1) : ((at + delta) % n + n) % n;
    loadFromLibrary(pad, m_library.at(next));
}

void KlangPadController::clear(int pad)
{
    if (pad < 0 || pad >= kPads)
        return;
    use(pad, nullptr, QString());
    edited();
}

QString KlangPadController::name(int pad) const
{
    return pad >= 0 && pad < kPads ? m_names[std::size_t(pad)] : QString();
}

double KlangPadController::seconds(int pad) const
{
    if (pad < 0 || pad >= kPads || !m_samples[std::size_t(pad)])
        return 0.0;
    const klang::Sample &s = *m_samples[std::size_t(pad)];
    return double(s.frames()) / s.rate;
}

QVariantList KlangPadController::waveform(int pad, int points) const
{
    QVariantList peaks;
    if (pad < 0 || pad >= kPads || !m_samples[std::size_t(pad)] || points < 1)
        return peaks;
    const klang::Sample &s = *m_samples[std::size_t(pad)];
    const std::size_t frames = s.frames();
    for (int k = 0; k < points; ++k) {
        const std::size_t a = frames * std::size_t(k) / std::size_t(points);
        const std::size_t b = std::max(a + 1, frames * std::size_t(k + 1) / std::size_t(points));
        float peak = 0.0f;
        for (std::size_t i = a; i < b && i < frames; i += std::max<std::size_t>(1, (b - a) / 64)) {
            peak = std::max(peak, std::fabs(s.left[i]));
            if (s.stereo())
                peak = std::max(peak, std::fabs(s.right[i]));
        }
        peaks << double(std::min(peak, 1.0f));
    }
    return peaks;
}

// --- Slicing -------------------------------------------------------------------------

void KlangPadController::sliceInto(int pad, const std::vector<double> &cuts)
{
    const double end = m_engine.padParam(pad, ParamId::End);
    const int count = std::min(int(cuts.size()), kPads - pad);
    for (int k = 0; k < count; ++k) {
        const int target = pad + k;
        if (target != pad) { // the pad's settings, its sample
            for (int i = 0; i < klang::PadParamCount; ++i)
                m_engine.setPadParam(target, ParamId(i), m_engine.padParam(pad, ParamId(i)));
            use(target, m_samples[std::size_t(pad)], m_names[std::size_t(pad)]);
        }
        m_engine.setPadParam(target, ParamId::Start, float(cuts[std::size_t(k)]));
        m_engine.setPadParam(target, ParamId::End,
                             float(k + 1 < int(cuts.size()) ? cuts[std::size_t(k + 1)] : end));
        // A slice is a hit: one shot, at its own speed.
        m_engine.setPadParam(target, ParamId::Mode, klang::position(0, klang::positions::kMode));
        m_engine.setPadParam(target, ParamId::Sync, 0.0f);
    }
    m_engine.reportAllChanged();
    m_message = QStringLiteral("Sliced into %1 pads (%2–%3)").arg(count).arg(pad + 1).arg(pad + count);
    edited();
}

void KlangPadController::slice(int pad, int count)
{
    if (pad < 0 || pad >= kPads || !m_samples[std::size_t(pad)] || count < 2)
        return;
    const double a = m_engine.padParam(pad, ParamId::Start), b = m_engine.padParam(pad, ParamId::End);
    if (b <= a)
        return;
    std::vector<double> cuts;
    for (int k = 0; k < count; ++k)
        cuts.push_back(a + (b - a) * k / count);
    sliceInto(pad, cuts);
}

std::vector<double> KlangPadController::hits(int pad) const
{
    std::vector<double> cuts;
    if (pad < 0 || pad >= kPads || !m_samples[std::size_t(pad)])
        return cuts;
    const klang::Sample &s = *m_samples[std::size_t(pad)];
    const double frames = double(s.frames());
    const auto a = std::size_t(double(m_engine.padParam(pad, ParamId::Start)) * frames);
    const auto b = std::size_t(double(m_engine.padParam(pad, ParamId::End)) * frames);
    cuts.push_back(double(a) / frames);
    // The energy in 5 ms windows; a hit where it jumps well above the last 40 ms.
    const auto window = std::max<std::size_t>(16, std::size_t(0.005 * s.rate));
    const auto gap = std::size_t(0.06 * s.rate); // hits at least 60 ms apart
    std::vector<double> energy;
    for (std::size_t i = a; i + window <= b; i += window) {
        double e = 0.0;
        for (std::size_t j = i; j < i + window; ++j) {
            const double x = s.stereo() ? 0.5 * (double(s.left[j]) + double(s.right[j])) : double(s.left[j]);
            e += x * x;
        }
        energy.push_back(e / double(window));
    }
    std::size_t last = a;
    for (std::size_t w = 8; w < energy.size() && cuts.size() < std::size_t(kPads); ++w) {
        double before = 0.0;
        for (std::size_t k = w - 8; k < w; ++k)
            before += energy[k];
        before /= 8.0;
        const std::size_t at = a + w * window;
        if (energy[w] > 4.0 * before + 1e-5 && energy[w] > energy[w - 1] && at - last >= gap) {
            cuts.push_back(double(at) / frames);
            last = at;
        }
    }
    return cuts;
}

void KlangPadController::sliceAtHits(int pad)
{
    const std::vector<double> cuts = hits(pad);
    if (cuts.size() < 2) {
        m_message = QStringLiteral("No hits found to slice at");
        emit changed();
        return;
    }
    sliceInto(pad, cuts);
}

// --- pads.json -------------------------------------------------------------------------

void KlangPadController::edited()
{
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void KlangPadController::save()
{
    m_saveTimer.stop();
    QJsonArray pads;
    for (int pad = 0; pad < kPads; ++pad) {
        QJsonObject params;
        for (int i = 0; i < klang::PadParamCount; ++i)
            params.insert(keyOf(i), double(m_engine.padParam(pad, ParamId(i))));
        pads.append(QJsonObject{{QStringLiteral("sample"), m_names[std::size_t(pad)]},
                                {QStringLiteral("params"), params}});
    }
    const QJsonObject json{{QStringLiteral("version"), kFileVersion},
                           {QStringLiteral("edited"), m_engine.editedPad()},
                           {QStringLiteral("pads"), pads}};
    if (!krate::files::writeJsonFile(m_path, json))
        qCWarning(lcKlangPads) << "could not save" << m_path;
}

void KlangPadController::load()
{
    QJsonObject json;
    if (krate::files::readVersionedJson(m_path, kFileVersion, &json) != krate::files::LoadResult::Loaded)
        return;
    const QJsonArray pads = json.value(u"pads").toArray();
    for (int pad = 0; pad < kPads && pad < pads.size(); ++pad) {
        const QJsonObject po = pads.at(pad).toObject();
        const QJsonObject params = po.value(u"params").toObject();
        for (int i = 0; i < klang::PadParamCount; ++i)
            if (const QJsonValue v = params.value(keyOf(i)); v.isDouble())
                m_engine.setPadParam(pad, ParamId(i),
                                     krate::snapToDetent(klang::kParams[std::size_t(i)], float(std::clamp(v.toDouble(), 0.0, 1.0))));
        const QString name = po.value(u"sample").toString();
        if (name.isEmpty())
            continue;
        QString error;
        if (const SamplePtr sample = decode(m_libraryDir + u'/' + name, &error))
            use(pad, sample, name);
        else
            qCWarning(lcKlangPads) << "pad" << pad + 1 << ":" << name << error;
    }
    m_panel.setEditedPad(std::clamp(json.value(u"edited").toInt(0), 0, kPads - 1));
    m_engine.reportAllChanged();
}
