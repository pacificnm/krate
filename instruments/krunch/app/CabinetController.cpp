#include "CabinetController.h"

#include <QFileInfo>
#include <QJSEngine>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrl>

#include "app/JsonFile.h"
#include "dsp/Cabinet.h"
#include "dsp/Krunch.h"
#include "dsp/Wav.h"

Q_LOGGING_CATEGORY(lcCabinets, "krunch.cabinets")

namespace {
constexpr int kVersion = 1;
constexpr int kPollIntervalMs = 250;
} // namespace

CabinetController::CabinetController(krunch::Krunch &engine, const QString &libraryDirectory, const QString &filePath,
                                     QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_library(libraryDirectory)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &CabinetController::saveFile);
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &CabinetController::poll);
    m_pollTimer.start();
    loadFile();
}

CabinetController::~CabinetController()
{
    if (m_saveTimer.isActive())
        saveFile();
    // The engine outlives this (it's the instance's); empty its slots first.
    for (int s = 0; s < 2 * kChannels; ++s)
        m_engine.cabinet().setKernel(s, nullptr);
    s_instance = nullptr;
}

CabinetController *CabinetController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int CabinetController::libraryCount() const { return int(m_library.list().size()); }

QVariantList CabinetController::irs(const QString &filter) const
{
    QVariantList result;
    for (const IrLibrary::Ir &ir : m_library.list(filter))
        result << QVariantMap{{QStringLiteral("id"), ir.id},
                              {QStringLiteral("pack"), ir.pack},
                              {QStringLiteral("folder"), ir.folder},
                              {QStringLiteral("name"), ir.name},
                              {QStringLiteral("channels"), ir.channels},
                              {QStringLiteral("rate"), ir.rate},
                              {QStringLiteral("seconds"), ir.rate > 0 ? double(ir.frames) / ir.rate : 0.0},
                              {QStringLiteral("favourite"), ir.favourite}};
    return result;
}

void CabinetController::importUrls(const QVariantList &urls)
{
    QStringList paths;
    for (const QVariant &u : urls) {
        const QUrl url = u.toUrl();
        paths << (url.isLocalFile() ? url.toLocalFile() : u.toString());
    }
    const IrLibrary::ImportResult r = m_library.import(paths);
    QStringList parts;
    parts << QStringLiteral("%1 IR%2 added").arg(r.added).arg(r.added == 1 ? QString() : QStringLiteral("s"));
    if (r.duplicates)
        parts << QStringLiteral("%1 already in the library").arg(r.duplicates);
    if (r.failed)
        parts << QStringLiteral("%1 not readable (%2)").arg(r.failed).arg(r.errors.join(QStringLiteral("; ")));
    m_importMessage = parts.join(QStringLiteral(", "));
    qCInfo(lcCabinets) << "import:" << m_importMessage;
    bump();
}

void CabinetController::setFavourite(const QString &id, bool favourite)
{
    m_library.setFavourite(id, favourite);
    bump();
}

void CabinetController::removeIr(const QString &id)
{
    if (!m_library.remove(id))
        return;
    for (int s = 0; s < 2 * kChannels; ++s)
        if (m_ids[std::size_t(s)] == id) {
            m_ids[std::size_t(s)].clear();
            build(s);
        }
    m_saveTimer.start();
    bump();
}

QString CabinetController::irId(int channel, bool b) const
{
    return channel >= 0 && channel < kChannels ? m_ids[std::size_t(slot(channel, b))] : QString();
}

QString CabinetController::irName(int channel, bool b) const
{
    const QString id = irId(channel, b);
    if (id.isEmpty())
        return {};
    const auto ir = m_library.find(id);
    return ir ? ir->name : QStringLiteral("(not in the library)");
}

void CabinetController::choose(int channel, bool b, const QString &id)
{
    if (channel < 0 || channel >= kChannels || m_ids[std::size_t(slot(channel, b))] == id)
        return;
    m_ids[std::size_t(slot(channel, b))] = id;
    build(slot(channel, b));
    m_saveTimer.start();
    bump();
}

void CabinetController::build(int s)
{
    std::unique_ptr<krunch::IrKernel> kernel;
    const QString &id = m_ids[std::size_t(s)];
    if (!id.isEmpty()) {
        const QString path = m_library.filePath(id);
        krate::WavData wav;
        std::string error;
        if (path.isEmpty())
            qCWarning(lcCabinets) << "IR" << id << "is not in the library";
        else if (!krate::readWavFile(path.toStdString(), &wav, &error)
                 || !(kernel = krunch::IrKernel::build(wav, m_engine.sampleRate(), &error)))
            qCWarning(lcCabinets) << "can't load" << path << ":" << error.c_str();
    }
    m_engine.cabinet().setKernel(s, kernel.get());
    if (m_live[std::size_t(s)]) // the audio thread may be in the middle of it: free it later
        m_retired.push_back({std::move(m_live[std::size_t(s)]), m_engine.cabinet().blocksProcessed()});
    m_live[std::size_t(s)] = std::move(kernel);
    m_builtRate = m_engine.sampleRate();
}

void CabinetController::poll()
{
    if (m_engine.sampleRate() != m_builtRate)
        for (int s = 0; s < 2 * kChannels; ++s)
            build(s);
    // Two blocks on, the audio thread has finished with anything replaced before.
    const quint64 now = m_engine.cabinet().blocksProcessed();
    std::erase_if(m_retired, [now](const Retired &r) { return now >= r.after + 2; });
}

void CabinetController::bump()
{
    ++m_revision;
    emit changed();
}

QJsonArray CabinetController::save() const
{
    QJsonArray channels;
    for (int c = 0; c < kChannels; ++c)
        channels.append(QJsonArray{irId(c, false), irId(c, true)});
    return channels;
}

void CabinetController::load(const QJsonArray &channels)
{
    for (int c = 0; c < kChannels; ++c) {
        const QJsonArray pair = channels.at(c).toArray();
        choose(c, false, pair.at(0).toString());
        choose(c, true, pair.at(1).toString());
    }
}

void CabinetController::saveFile()
{
    m_saveTimer.stop();
    if (!krate::files::writeJsonFile(m_filePath, QJsonObject{{QStringLiteral("version"), kVersion},
                                                             {QStringLiteral("channels"), save()}}))
        qCWarning(lcCabinets) << "could not save" << m_filePath;
}

void CabinetController::loadFile()
{
    QJsonObject json;
    if (krate::files::readVersionedJson(m_filePath, kVersion, &json) == krate::files::LoadResult::Loaded) {
        load(json.value(u"channels").toArray());
        m_saveTimer.stop(); // just loaded
    }
}
