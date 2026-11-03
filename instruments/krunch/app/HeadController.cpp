#include "HeadController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>

#include "app/HeadFile.h"
#include "app/UrlList.h"
#include "dsp/Krunch.h"

Q_LOGGING_CATEGORY(lcHeads, "krunch.heads")

namespace {
constexpr int kVersion = 1;
const QString kSuffix = QStringLiteral(".json");
} // namespace

HeadController::HeadController(krunch::Krunch &engine, const QString &folder, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_folder(QDir::cleanPath(folder))
    , m_filePath(filePath)
{
    QDir().mkpath(m_folder);
    m_watcher.addPath(m_folder); // heads put there by hand, by Krate Remote or krunch-fit
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { rescan(); });
    rescan();
    loadFile();
    for (int c = 0; c < kChannels; ++c)
        apply(c);
    m_poll.setInterval(100);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        const quint64 now = m_engine.blocksProcessed();
        std::erase_if(m_retired, [now](const Retired &r) { return now >= r.after + 2; });
    });
    m_poll.start();
}

HeadController::~HeadController()
{
    for (int c = 0; c < kChannels; ++c)
        m_engine.setHead(c, nullptr); // (the engine is stopped by now, or about to be)
}

void HeadController::bump()
{
    ++m_revision;
    emit changed();
}

void HeadController::rescan()
{
    QStringList names;
    std::vector<Entry> entries;
    for (const QFileInfo &f : QDir(m_folder).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        const auto head = krunch::HeadFile::read(f.filePath());
        if (!head)
            continue; // not a head
        names << f.completeBaseName();
        QString about;
        if (const QString capture = head->fit.value(u"capture").toString(); !capture.isEmpty())
            about = QStringLiteral("fitted to %1 (score %2; the closest built-in was %3)")
                        .arg(capture)
                        .arg(head->fit.value(u"score").toDouble(), 0, 'f', 2)
                        .arg(head->fit.value(u"built_in_score").toDouble(), 0, 'f', 2);
        entries.push_back({head->name.isEmpty() ? f.completeBaseName() : head->name, about});
    }
    const bool same = names == m_names;
    m_names = names;
    m_entries = std::move(entries);
    for (int c = 0; c < kChannels; ++c) // a chosen head's file may have been fitted again
        if (!m_chosen[std::size_t(c)].isEmpty())
            apply(c);
    if (!same)
        bump();
}

QString HeadController::head(int channel) const
{
    return channel >= 0 && channel < kChannels ? m_chosen[std::size_t(channel)] : QString();
}

QString HeadController::title(const QString &name) const
{
    const qsizetype i = m_names.indexOf(name);
    return i >= 0 ? m_entries[std::size_t(i)].title : name;
}

QString HeadController::about(const QString &name) const
{
    const qsizetype i = m_names.indexOf(name);
    return i >= 0 ? m_entries[std::size_t(i)].about : QString();
}

bool HeadController::ownPowerTubes(int channel) const
{
    return channel >= 0 && channel < kChannels && m_live[std::size_t(channel)] && !m_live[std::size_t(channel)]->channelPower;
}

void HeadController::choose(int channel, const QString &name)
{
    if (channel < 0 || channel >= kChannels || name == m_chosen[std::size_t(channel)])
        return;
    m_chosen[std::size_t(channel)] = name;
    apply(channel);
    saveFile();
    bump();
}

void HeadController::apply(int channel)
{
    const QString name = m_chosen[std::size_t(channel)];
    std::unique_ptr<krunch::HeadDesign> head;
    if (!name.isEmpty()) {
        QString error;
        if (const auto file = krunch::HeadFile::read(QDir(m_folder).filePath(name + kSuffix), &error)) {
            head = std::make_unique<krunch::HeadDesign>(file->design);
        } else {
            m_message = QStringLiteral("%1: %2").arg(name, error);
            qCWarning(lcHeads).noquote() << "could not load" << name << ":" << error;
        }
    }
    m_engine.setHead(channel, head.get());
    if (m_live[std::size_t(channel)])
        m_retired.push_back({std::move(m_live[std::size_t(channel)]), m_engine.blocksProcessed()});
    m_live[std::size_t(channel)] = std::move(head);
    bump();
}

void HeadController::importUrls(const QVariantList &urls)
{
    int added = 0;
    QStringList failed;
    for (const QString &path : krate::localPaths(urls)) { // (a dialog's files come wrapped)
        const QFileInfo info(path);
        if (!info.isFile() || !krunch::HeadFile::read(path)) {
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
    m_message = failed.isEmpty() ? QStringLiteral("%1 head(s) added").arg(added)
                                 : QStringLiteral("%1 added; not heads: %2").arg(added).arg(failed.join(QStringLiteral(", ")));
    rescan();
    bump();
}

QJsonArray HeadController::save() const
{
    QJsonArray a;
    for (const QString &name : m_chosen)
        a.append(name);
    return a;
}

void HeadController::load(const QJsonArray &channels)
{
    for (int c = 0; c < kChannels; ++c) {
        const QString name = channels.at(c).toString();
        if (name == m_chosen[std::size_t(c)])
            continue;
        m_chosen[std::size_t(c)] = name;
        apply(c);
    }
    saveFile();
    bump();
}

void HeadController::saveFile()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), kVersion}, {QStringLiteral("channels"), save()}}).toJson());
        f.commit();
    }
}

void HeadController::loadFile()
{
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray channels = QJsonDocument::fromJson(f.readAll()).object().value(u"channels").toArray();
    for (int c = 0; c < kChannels; ++c)
        m_chosen[std::size_t(c)] = channels.at(c).toString();
}
