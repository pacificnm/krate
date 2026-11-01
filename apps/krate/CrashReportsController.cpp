#include "CrashReportsController.h"

#include <QFile>
#include <QJSEngine>

#include "app/CrashReports.h"

CrashReportsController::CrashReportsController(QString dir, QObject *parent)
    : QObject(parent)
    , m_dir(std::move(dir))
    , m_new(krate::crash::newReport())
{
    Q_ASSERT(!s_instance);
    s_instance = this;
}

CrashReportsController::~CrashReportsController()
{
    s_instance = nullptr;
}

CrashReportsController *CrashReportsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList CrashReportsController::reports() const
{
    QVariantList list;
    for (const auto &r : krate::crash::reports(m_dir))
        list << QVariantMap{{QStringLiteral("path"), r.path},
                            {QStringLiteral("when"), r.when.toString(QStringLiteral("ddd d MMM yyyy, HH:mm:ss"))},
                            {QStringLiteral("summary"), r.summary}};
    return list;
}

QString CrashReportsController::newSummary() const
{
    for (const auto &r : krate::crash::reports(m_dir))
        if (r.path == m_new)
            return r.summary;
    return {};
}

void CrashReportsController::setViewing(bool on)
{
    if (on == m_viewing)
        return;
    m_viewing = on;
    emit viewingChanged();
}

QString CrashReportsController::folder() const { return m_dir + QStringLiteral("/crashes"); }

QString CrashReportsController::text(const QString &path) const
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

void CrashReportsController::dismiss()
{
    if (m_new.isEmpty())
        return;
    m_new.clear();
    emit changed();
}

void CrashReportsController::remove(const QString &path)
{
    if (!path.startsWith(folder() + u'/')) // only its own reports
        return;
    QFile::remove(path);
    if (path == m_new)
        m_new.clear();
    emit changed();
}

void CrashReportsController::removeAll()
{
    for (const auto &r : krate::crash::reports(m_dir))
        QFile::remove(r.path);
    m_new.clear();
    emit changed();
}
