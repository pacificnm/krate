#include "UpdateController.h"

#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QVersionNumber>

Q_LOGGING_CATEGORY(lcUpdates, "krate.updates")

namespace {
constexpr auto kLatest = "https://api.github.com/repos/pacificnm/krate/releases/latest";
constexpr int kFirstCheckMs = 15 * 1000;            // soon after starting: the network may still be coming up
constexpr int kCheckEveryMs = 12 * 60 * 60 * 1000; // twice a day
} // namespace

UpdateController::UpdateController(QString currentVersion, QString updaterPath, QString statusPath, QObject *parent)
    : QObject(parent)
    , m_current(std::move(currentVersion))
    , m_updater(std::move(updaterPath))
    , m_statusPath(std::move(statusPath))
    , m_network(new QNetworkAccessManager(this))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_checkTimer.setSingleShot(true);
    connect(&m_checkTimer, &QTimer::timeout, this, [this] {
        check();
        m_checkTimer.start(kCheckEveryMs);
    });
    m_checkTimer.start(kFirstCheckMs);
    m_statusTimer.setInterval(500);
    connect(&m_statusTimer, &QTimer::timeout, this, &UpdateController::pollStatus);
}

UpdateController::~UpdateController()
{
    s_instance = nullptr;
}

UpdateController *UpdateController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool UpdateController::available() const { return !m_latest.isEmpty() && newer(m_latest, m_current); }

bool UpdateController::canInstall() const { return QFileInfo(m_updater).isExecutable(); }

bool UpdateController::newer(const QString &latest, const QString &current)
{
    const QVersionNumber a = QVersionNumber::fromString(latest), b = QVersionNumber::fromString(current);
    return !a.isNull() && QVersionNumber::compare(a, b) > 0;
}

bool UpdateController::parseRelease(const QByteArray &json, Release *release)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QString tag = o.value(u"tag_name").toString();
    if (tag.startsWith(u'v'))
        tag.remove(0, 1);
    if (QVersionNumber::fromString(tag).isNull() || o.value(u"draft").toBool() || o.value(u"prerelease").toBool())
        return false;
    release->version = tag;
    release->notes = o.value(u"body").toString().trimmed();
    static const QRegularExpression deb(QStringLiteral(R"(^krate_.*_amd64\.deb$)"));
    release->hasPackage = false;
    for (const QJsonValue &asset : o.value(u"assets").toArray())
        release->hasPackage = release->hasPackage || deb.match(asset.toObject().value(u"name").toString()).hasMatch();
    return true;
}

void UpdateController::parseStatus(const QString &line, QString *state, QString *text)
{
    const QString l = line.trimmed();
    *state = l.section(u' ', 0, 0);
    *text = l.section(u' ', 1);
}

void UpdateController::set(const QString &state, const QString &message)
{
    m_state = state;
    m_message = message;
    emit changed();
}

void UpdateController::check()
{
    if (m_state == u"checking" || m_statusTimer.isActive())
        return; // asking already, or installing
    set(QStringLiteral("checking"), QStringLiteral("Asking GitHub for the latest release"));
    QNetworkRequest request{QUrl(QString::fromLatin1(kLatest))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("krate/") + m_current);
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCInfo(lcUpdates) << "couldn't check for updates:" << reply->errorString();
            set(QString(), QStringLiteral("Couldn't reach GitHub to check (no network?)"));
            return;
        }
        Release r;
        if (!parseRelease(reply->readAll(), &r)) {
            set(QString(), QStringLiteral("GitHub's answer had no release in it"));
            return;
        }
        m_latest = r.hasPackage ? r.version : QString(); // a release still being built has no package yet
        m_notes = r.notes;
        if (available())
            qCInfo(lcUpdates) << "Krate" << m_latest << "is out (this is" << m_current << ")";
        set(QString(), available() ? QStringLiteral("Krate %1 is out").arg(m_latest) : QStringLiteral("Up to date"));
    });
}

void UpdateController::install()
{
    if (!canInstall() || m_statusTimer.isActive())
        return;
    m_statusBefore = QFileInfo(m_statusPath).lastModified();
    set(QStringLiteral("installing"), QStringLiteral("Starting the updater"));
    qCInfo(lcUpdates) << "starting the updater for" << m_latest;
    auto *systemctl = new QProcess(this);
    connect(systemctl, &QProcess::finished, this, [this, systemctl](int code) {
        systemctl->deleteLater();
        if (code != 0) { // not allowed (no polkit rule, not at the computer), or no such service
            const QString why = QString::fromUtf8(systemctl->readAllStandardError()).trimmed();
            qCWarning(lcUpdates) << "couldn't start the updater:" << why;
            m_statusTimer.stop();
            set(QStringLiteral("failed"), QStringLiteral("Couldn't start the updater: ") + why);
        }
    });
    // --no-block: it runs on its own; its progress comes through the status file.
    systemctl->start(QStringLiteral("systemctl"), {QStringLiteral("start"), QStringLiteral("--no-block"),
                                                   QStringLiteral("krate-update.service")});
    m_statusTimer.start();
}

void UpdateController::pollStatus()
{
    const QFileInfo info(m_statusPath);
    if (!info.exists() || info.lastModified() <= m_statusBefore)
        return; // nothing from this run yet
    QFile f(m_statusPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QString state, text;
    parseStatus(QString::fromUtf8(f.readAll()), &state, &text);
    if (state == u"done") { // the kiosk restarts Krate itself; a desktop's is up to you
        m_statusTimer.stop();
        set(state, text + QStringLiteral(": restart Krate to use it"));
        qCInfo(lcUpdates) << text;
    } else if (state == u"failed" || state == u"current") {
        m_statusTimer.stop();
        set(state, text);
        qCInfo(lcUpdates) << "updater:" << state << text;
    } else if (state != m_state || text != m_message) {
        set(state, text);
    }
}
