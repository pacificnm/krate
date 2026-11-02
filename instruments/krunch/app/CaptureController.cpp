#include "CaptureController.h"

#include <chrono>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QUrl>

#include "app/UrlList.h"
#include "dsp/Capture.h"
#include "dsp/Krunch.h"

Q_LOGGING_CATEGORY(lcCaptures, "krunch.captures")

namespace {
constexpr int kVersion = 1;
const QString kSuffix = QStringLiteral(".nam");
} // namespace

CaptureController::CaptureController(krunch::Krunch &engine, const QString &folder, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_folder(QDir::cleanPath(folder))
    , m_filePath(filePath)
{
    QDir().mkpath(m_folder);
    m_watcher.addPath(m_folder); // files put there by hand or by Krate Remote
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { rescan(); });
    rescan();
    loadFile();
    m_poll.setInterval(50);
    connect(&m_poll, &QTimer::timeout, this, &CaptureController::poll);
    m_poll.start();
}

CaptureController::~CaptureController()
{
    for (int c = 0; c < kChannels; ++c)
        m_engine.setCapture(c, nullptr); // (the engine is stopped by now, or about to be)
}

void CaptureController::bump()
{
    ++m_revision;
    emit changed();
}

void CaptureController::rescan()
{
    QStringList names;
    for (const QFileInfo &f : QDir(m_folder).entryInfoList({QStringLiteral("*.nam")}, QDir::Files, QDir::Name | QDir::IgnoreCase))
        names << f.completeBaseName();
    if (names == m_names)
        return;
    m_names = names;
    bump();
}

QString CaptureController::capture(int channel) const
{
    return channel >= 0 && channel < kChannels ? m_chosen[std::size_t(channel)] : QString();
}

bool CaptureController::loading(int channel) const
{
    return channel >= 0 && channel < kChannels && m_loading[std::size_t(channel)].model.valid();
}

void CaptureController::choose(int channel, const QString &name)
{
    if (channel < 0 || channel >= kChannels || name == m_chosen[std::size_t(channel)])
        return;
    m_chosen[std::size_t(channel)] = name;
    start(channel);
    saveFile();
    bump();
}

void CaptureController::start(int channel)
{
    const QString name = m_chosen[std::size_t(channel)];
    Loading &l = m_loading[std::size_t(channel)];
    l.name = name;
    l.model = {};
    if (name.isEmpty() || m_engine.sampleRate() <= 0.0) { // none (or no rate yet: poll() starts it)
        if (name.isEmpty() && m_live[std::size_t(channel)]) {
            m_engine.setCapture(channel, nullptr);
            m_retired.push_back({std::move(m_live[std::size_t(channel)]), m_engine.blocksProcessed()});
        }
        return;
    }
    const std::string path = QDir(m_folder).filePath(name + kSuffix).toStdString();
    const double rate = m_engine.sampleRate();
    l.rate = rate;
    l.error = std::make_shared<QString>();
    auto error = l.error;
    // Parsing a model and readying it allocates and takes a moment: not on the GUI thread.
    l.model = std::async(std::launch::async, [path, rate, error]() -> std::shared_ptr<krunch::CaptureModel> {
        std::string why;
        std::shared_ptr<krunch::CaptureModel> model = krunch::CaptureModel::load(path, rate, int(krunch::Krunch::kBlock), &why);
        if (!model)
            *error = QString::fromStdString(why);
        return model;
    });
}

void CaptureController::poll()
{
    const double rate = m_engine.sampleRate();
    if (rate > 0.0 && rate != m_rate) { // the first rate, or a new one: every capture readied again
        m_rate = rate;
        for (int c = 0; c < kChannels; ++c)
            if (!m_chosen[std::size_t(c)].isEmpty())
                start(c);
    }
    for (int c = 0; c < kChannels; ++c) {
        Loading &l = m_loading[std::size_t(c)];
        if (!l.model.valid() || l.model.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            continue;
        std::shared_ptr<krunch::CaptureModel> model = l.model.get();
        if (l.name != m_chosen[std::size_t(c)] || l.rate != m_engine.sampleRate())
            continue; // chosen again since, or the rate moved: a newer load is coming
        if (!model) {
            m_message = QStringLiteral("%1: %2").arg(l.name, *l.error);
            qCWarning(lcCaptures).noquote() << "could not load" << l.name << ":" << *l.error;
            bump();
            continue;
        }
        if (model->expectedRate() > 0.0 && std::abs(model->expectedRate() - rate) > 1.0)
            m_message = QStringLiteral("%1 was captured at %2 Hz; Krate runs at %3 Hz (it may sound a little off)")
                            .arg(l.name).arg(model->expectedRate()).arg(rate);
        else
            m_message.clear();
        m_engine.setCapture(c, model.get());
        if (m_live[std::size_t(c)])
            m_retired.push_back({std::move(m_live[std::size_t(c)]), m_engine.blocksProcessed()});
        m_live[std::size_t(c)] = std::move(model);
        qCInfo(lcCaptures).noquote() << "channel" << c + 1 << "plays" << l.name;
        bump();
    }
    const quint64 now = m_engine.blocksProcessed();
    std::erase_if(m_retired, [now](const Retired &r) { return now >= r.after + 2; });
}

void CaptureController::importUrls(const QVariantList &urls)
{
    int added = 0;
    QStringList failed;
    for (const QString &path : krate::localPaths(urls)) { // (a dialog's files come wrapped)
        const QFileInfo info(path);
        if (!info.isFile() || !path.endsWith(kSuffix, Qt::CaseInsensitive)) {
            failed << info.fileName();
            continue;
        }
        const QString target = QDir(m_folder).filePath(info.completeBaseName() + kSuffix);
        QFile::remove(target); // a newer one of the same name
        if (QFile::copy(path, target))
            ++added;
        else
            failed << info.fileName();
    }
    m_message = failed.isEmpty() ? QStringLiteral("%1 capture(s) added").arg(added)
                                 : QStringLiteral("%1 added; not .nam files: %2").arg(added).arg(failed.join(QStringLiteral(", ")));
    rescan();
    bump();
}

QJsonArray CaptureController::save() const
{
    QJsonArray a;
    for (const QString &name : m_chosen)
        a.append(name);
    return a;
}

void CaptureController::load(const QJsonArray &channels)
{
    for (int c = 0; c < kChannels; ++c) {
        const QString name = channels.at(c).toString();
        if (name == m_chosen[std::size_t(c)])
            continue;
        m_chosen[std::size_t(c)] = name;
        start(c);
    }
    saveFile();
    bump();
}

void CaptureController::saveFile()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), kVersion}, {QStringLiteral("channels"), save()}}).toJson());
        f.commit();
    }
}

void CaptureController::loadFile()
{
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray channels = QJsonDocument::fromJson(f.readAll()).object().value(u"channels").toArray();
    for (int c = 0; c < kChannels; ++c)
        m_chosen[std::size_t(c)] = channels.at(c).toString(); // loaded once the engine has its rate (poll)
}
