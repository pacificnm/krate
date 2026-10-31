#include "app/PresetInbox.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include "app/JsonFile.h"

Q_LOGGING_CATEGORY(lcInbox, "krate.inbox")

namespace krate {

namespace {
constexpr int kVersion = 1;
}

QString PresetInbox::dirFor(const QString &bankFile) { return QFileInfo(bankFile).absolutePath() + QStringLiteral("/inbox"); }

PresetInbox::PresetInbox(const QString &bankFile, QObject *parent)
    : QObject(parent)
    , m_dir(dirFor(bankFile))
{
    QDir().mkpath(m_dir);
    m_watcher.addPath(m_dir);
    m_settle.setSingleShot(true);
    m_settle.setInterval(50);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_settle, qOverload<>(&QTimer::start));
    connect(&m_settle, &QTimer::timeout, this, [this] {
        if (!QDir(m_dir).entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty())
            emit arrived();
    });
}

std::vector<PresetInbox::Preset> PresetInbox::take()
{
    std::vector<Preset> out;
    // Oldest first: the names start with the time they were sent.
    for (const QString &name : QDir(m_dir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        const QString path = m_dir + u'/' + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        QFile::remove(path);
        const int slot = json.value(u"slot").toInt(-1);
        if (json.value(u"version").toInt() != kVersion || slot < 0 || !json.value(u"params").isObject()) {
            qCWarning(lcInbox) << "not a preset, removed:" << path;
            continue;
        }
        Preset p{slot, json.value(u"name").toString(), {}, json.value(u"from").toString()};
        const QJsonObject params = json.value(u"params").toObject();
        for (auto it = params.begin(); it != params.end(); ++it)
            if (it->isDouble())
                p.values.insert(it.key(), std::clamp(it->toDouble(), 0.0, 1.0));
        qCInfo(lcInbox) << "received" << p.name << "for slot" << slot << "from" << p.from;
        out.push_back(std::move(p));
    }
    return out;
}

bool PresetInbox::send(const QString &bankFile, const Preset &preset)
{
    QJsonObject params;
    for (auto it = preset.values.begin(); it != preset.values.end(); ++it)
        params.insert(it.key(), it.value());
    const QString name = QStringLiteral("%1-%2.json").arg(QDateTime::currentMSecsSinceEpoch()).arg(preset.slot);
    return files::writeJsonFile(dirFor(bankFile) + u'/' + name,
                                QJsonObject{{QStringLiteral("version"), kVersion},
                                            {QStringLiteral("slot"), preset.slot},
                                            {QStringLiteral("name"), preset.name},
                                            {QStringLiteral("params"), params},
                                            {QStringLiteral("from"), preset.from}});
}

} // namespace krate
