#include "kessel/app/SampleController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonObject>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "dsp/Wav.h"
#include "kessel/dsp/Engine.h"

Q_LOGGING_CATEGORY(lcSamples, "kessel.samples")

namespace {
constexpr int kStateVersion = 1;
constexpr const char *kDrumKeys[] = {"bd", "sd", "lt", "mt", "ht", "rs", "cp", "ch",
                                     "oh", "cr", "rd", "me", "ck", "nz", "bm", "sm"};
static_assert(std::size(kDrumKeys) == std::size_t(kessel::DrumCount));

std::unique_ptr<kessel::Sample> decode(const QString &path, QString *error)
{
    krate::WavData wav;
    std::string why;
    if (!krate::readWavFile(path.toStdString(), &wav, &why)) {
        *error = QString::fromStdString(why);
        return nullptr;
    }
    auto sample = std::make_unique<kessel::Sample>(
        kessel::Sample::fromChannels(wav.left, wav.right, double(wav.rate), QFileInfo(path).fileName().toStdString()));
    if (sample->data.empty()) {
        *error = QStringLiteral("it is silent");
        return nullptr;
    }
    return sample;
}

bool sameFile(const QString &a, const QString &b)
{
    QFile fa(a), fb(b);
    return fa.size() == fb.size() && fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly) && fa.readAll() == fb.readAll();
}
} // namespace

SampleController::SampleController(kessel::Engine &engine, const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_libraryDir(dataDir + QStringLiteral("/samples"))
    , m_statePath(dataDir + QStringLiteral("/samples.json"))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    rescan();
    QJsonObject json;
    if (krate::files::readVersionedJson(m_statePath, kStateVersion, &json) == krate::files::LoadResult::Loaded) {
        const QString name = json.value(u"current").toString();
        if (!name.isEmpty() && !select(name))
            qCWarning(lcSamples) << "could not load the sample" << name;
        const QJsonObject drums = json.value(u"drums").toObject();
        for (int d = 0; d < kessel::DrumCount; ++d)
            if (const QString n = drums.value(drumKey(d)).toString(); !n.isEmpty() && d != int(kessel::DrumId::Sm)
                                                                       && !setDrumSample(d, n))
                qCWarning(lcSamples) << "could not load the sample" << n << "for" << drumKey(d);
    }
    m_freeTimer.setInterval(100);
    connect(&m_freeTimer, &QTimer::timeout, this, &SampleController::freeRetired);
    m_freeTimer.start();
}

SampleController::~SampleController()
{
    m_engine.setSample(nullptr); // the host has stopped processing by now
    for (int d = 0; d < kessel::DrumCount; ++d)
        m_engine.setDrumSample(d, nullptr);
    s_instance = nullptr;
}

SampleController *SampleController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

double SampleController::seconds() const
{
    return m_current ? double(m_current->data.size()) / m_current->rate : 0.0;
}

void SampleController::rescan()
{
    m_names = QDir(m_libraryDir).entryList({QStringLiteral("*.wav"), QStringLiteral("*.WAV")}, QDir::Files,
                                            QDir::Name | QDir::IgnoreCase);
}

void SampleController::use(std::unique_ptr<kessel::Sample> sample, const QString &name)
{
    m_engine.setSample(sample.get());
    if (m_current)
        m_retired.push_back({std::move(m_current), m_engine.blocksProcessed()});
    m_current = std::move(sample);
    m_name = name;
    saveState();
    emit changed();
}

bool SampleController::select(const QString &name)
{
    if (name == m_name && m_current)
        return true;
    QString error;
    auto sample = decode(m_libraryDir + u'/' + name, &error);
    if (!sample) {
        m_message = QStringLiteral("%1: %2").arg(name, error);
        emit changed();
        return false;
    }
    use(std::move(sample), name);
    return true;
}

void SampleController::step(int delta)
{
    rescan();
    if (m_names.isEmpty())
        return;
    const int at = m_names.indexOf(m_name);
    const int n = int(m_names.size());
    const int next = at < 0 ? (delta > 0 ? 0 : n - 1) : ((at + delta) % n + n) % n;
    select(m_names.at(next));
}

void SampleController::clear()
{
    use(nullptr, QString());
}

QString SampleController::copyIn(const QString &path, QString *error)
{
    QDir().mkpath(m_libraryDir);
    if (!decode(path, error)) // check it plays before copying
        return {};
    // Keep the name; a different file of the same name gets a number.
    const QFileInfo info(path);
    QString name = info.fileName();
    for (int n = 2; QFile::exists(m_libraryDir + u'/' + name) && !sameFile(path, m_libraryDir + u'/' + name); ++n)
        name = QStringLiteral("%1 (%2).%3").arg(info.completeBaseName()).arg(n).arg(info.suffix());
    const QString target = m_libraryDir + u'/' + name;
    if (!QFile::exists(target) && !QFile::copy(path, target)) {
        *error = QStringLiteral("could not copy");
        return {};
    }
    return name;
}

void SampleController::importUrls(const QList<QUrl> &urls)
{
    QString last;
    int imported = 0;
    QStringList failed;
    for (const QUrl &url : urls) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        QString error;
        const QString name = copyIn(path, &error);
        if (name.isEmpty()) {
            failed << QFileInfo(path).fileName() + QStringLiteral(" (") + error + u')';
            continue;
        }
        last = name;
        ++imported;
    }
    rescan();
    m_message = imported > 0 ? QStringLiteral("Imported %1 sample%2").arg(imported).arg(imported == 1 ? "" : "s") : QString();
    if (!failed.isEmpty())
        m_message += (m_message.isEmpty() ? QString() : QStringLiteral(". ")) + QStringLiteral("Not WAV or unreadable: ")
                     + failed.join(QStringLiteral(", "));
    if (!last.isEmpty())
        select(last);
    emit changed();
}

QString SampleController::drumKey(int drum)
{
    return drum >= 0 && drum < kessel::DrumCount ? QString::fromLatin1(kDrumKeys[drum]) : QString();
}

QString SampleController::drumSample(int drum) const
{
    if (drum == int(kessel::DrumId::Sm))
        return m_name;
    return drum >= 0 && drum < kessel::DrumCount ? m_drumNames[std::size_t(drum)] : QString();
}

QStringList SampleController::drumSamples() const
{
    QStringList list;
    for (int d = 0; d < kessel::DrumCount; ++d)
        list << drumSample(d);
    return list;
}

bool SampleController::setDrumSample(int drum, const QString &name)
{
    if (drum < 0 || drum >= kessel::DrumCount)
        return false;
    if (drum == int(kessel::DrumId::Sm)) {
        if (name.isEmpty()) {
            clear();
            return true;
        }
        return select(name);
    }
    const auto d = std::size_t(drum);
    if (name == m_drumNames[d] && (name.isEmpty() || m_drum[d]))
        return true;
    std::unique_ptr<kessel::Sample> sample;
    if (!name.isEmpty()) {
        QString error;
        sample = decode(m_libraryDir + u'/' + name, &error);
        if (!sample) {
            m_message = QStringLiteral("%1: %2").arg(name, error);
            emit changed();
            return false;
        }
    }
    m_engine.setDrumSample(drum, sample.get());
    if (m_drum[d])
        m_retired.push_back({std::move(m_drum[d]), m_engine.blocksProcessed()});
    m_drum[d] = std::move(sample);
    m_drumNames[d] = name;
    saveState();
    emit changed();
    return true;
}

bool SampleController::assignFile(int drum, const QString &path)
{
    QString error;
    const QString name = copyIn(path, &error);
    if (name.isEmpty()) {
        m_message = QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
        emit changed();
        return false;
    }
    rescan();
    return setDrumSample(drum, name);
}

void SampleController::freeRetired()
{
    const std::uint64_t now = m_engine.blocksProcessed();
    std::erase_if(m_retired, [now](const Retired &r) { return now >= r.block + 2; });
}

void SampleController::saveState() const
{
    QJsonObject drums;
    for (int d = 0; d < kessel::DrumCount; ++d)
        if (!m_drumNames[std::size_t(d)].isEmpty() && d != int(kessel::DrumId::Sm))
            drums.insert(drumKey(d), m_drumNames[std::size_t(d)]);
    krate::files::writeJsonFile(m_statePath, QJsonObject{{QStringLiteral("version"), kStateVersion},
                                                         {QStringLiteral("current"), m_name},
                                                         {QStringLiteral("drums"), drums}});
}
