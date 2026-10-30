#include "k909/app/SampleController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonObject>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "dsp/Wav.h"
#include "k909/dsp/Engine.h"

Q_LOGGING_CATEGORY(lcSamples, "k909.samples")

namespace {
constexpr int kStateVersion = 1;

std::unique_ptr<k909::Sample> decode(const QString &path, QString *error)
{
    krate::WavData wav;
    std::string why;
    if (!krate::readWavFile(path.toStdString(), &wav, &why)) {
        *error = QString::fromStdString(why);
        return nullptr;
    }
    auto sample = std::make_unique<k909::Sample>(
        k909::Sample::fromChannels(wav.left, wav.right, double(wav.rate), QFileInfo(path).fileName().toStdString()));
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

SampleController::SampleController(k909::Engine &engine, const QString &dataDir, QObject *parent)
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
    }
    m_freeTimer.setInterval(100);
    connect(&m_freeTimer, &QTimer::timeout, this, &SampleController::freeRetired);
    m_freeTimer.start();
}

SampleController::~SampleController()
{
    m_engine.setSample(nullptr); // the host has stopped processing by now
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

void SampleController::use(std::unique_ptr<k909::Sample> sample, const QString &name)
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

void SampleController::importUrls(const QList<QUrl> &urls)
{
    QDir().mkpath(m_libraryDir);
    QString last;
    int imported = 0;
    QStringList failed;
    for (const QUrl &url : urls) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        QString error;
        if (!decode(path, &error)) { // check it plays before copying
            failed << QFileInfo(path).fileName() + QStringLiteral(" (") + error + u')';
            continue;
        }
        // Keep the name; a different file of the same name gets a number.
        const QFileInfo info(path);
        QString name = info.fileName();
        for (int n = 2; QFile::exists(m_libraryDir + u'/' + name) && !sameFile(path, m_libraryDir + u'/' + name); ++n)
            name = QStringLiteral("%1 (%2).%3").arg(info.completeBaseName()).arg(n).arg(info.suffix());
        const QString target = m_libraryDir + u'/' + name;
        if (!QFile::exists(target) && !QFile::copy(path, target)) {
            failed << info.fileName() + QStringLiteral(" (could not copy)");
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

void SampleController::freeRetired()
{
    const std::uint64_t now = m_engine.blocksProcessed();
    std::erase_if(m_retired, [now](const Retired &r) { return now >= r.block + 2; });
}

void SampleController::saveState() const
{
    krate::files::writeJsonFile(m_statePath, QJsonObject{{QStringLiteral("version"), kStateVersion},
                                                         {QStringLiteral("current"), m_name}});
}
