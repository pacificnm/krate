#include "app/StandaloneSession.h"

#include <algorithm>

#include <QJsonArray>
#include <QLoggingCategory>

#include "app/HostController.h"
#include "app/JsonFile.h"
#include "app/UpdateController.h"

Q_LOGGING_CATEGORY(lcSession, "krate.standalone")

namespace krate {

namespace {
constexpr int kVersion = 1;
constexpr int kRackVersion = 99; // rack.json: only its audio keys are read, whatever its version
} // namespace

StandaloneSession::StandaloneSession(QString path, HostController &host, UpdateController *updates,
                                     const QString &seedPath, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
    , m_host(host)
    , m_updates(updates)
{
    QJsonObject json;
    const files::LoadResult loaded = files::readVersionedJson(m_path, kVersion, &json);
    if (loaded == files::LoadResult::Loaded) {
        read(json);
        m_autoCheck = json.value(u"autoCheckUpdates").toBool(true);
    } else if (loaded == files::LoadResult::Missing && !seedPath.isEmpty()
               && files::readVersionedJson(seedPath, kRackVersion, &json) == files::LoadResult::Loaded) {
        read(json);
        m_seeded = true;
        qCInfo(lcSession) << "audio set up as in" << seedPath;
    }

    // Listening first: the host may wire the inputs at once (it's open already).
    connect(&m_host, &HostController::inputPortsChanged, this, [this] {
        if (m_host.inputPorts() != m_audioInputs) {
            m_audioInputs = m_host.inputPorts();
            save();
        }
    });
    connect(&m_host, &HostController::outputDeviceChanged, this, [this] {
        if (m_host.outputDevice() != m_outputDevice) {
            m_outputDevice = m_host.outputDevice();
            save();
        }
    });
    connect(&m_host, &HostController::requestedBufferSizeChanged, this, [this] {
        if (m_host.requestedBufferSize() != m_bufferSize) {
            m_bufferSize = m_host.requestedBufferSize();
            save();
        }
    });
    if (m_updates)
        connect(m_updates, &UpdateController::autoCheckChanged, this, [this] {
            if (m_updates->autoCheck() != m_autoCheck) {
                m_autoCheck = m_updates->autoCheck();
                save();
            }
        });
    m_host.setRequestedBufferSize(m_bufferSize);
    m_host.setOutputDevice(m_outputDevice);
    m_host.setInputPorts(m_audioInputs);
    // Never chosen (or all "None"): the interface's jacks in order, once it's seen (and kept).
    m_host.setAutoInputs(std::none_of(m_audioInputs.begin(), m_audioInputs.end(),
                                      [](const QString &p) { return !p.isEmpty(); }));
    if (m_updates)
        m_updates->setAutoCheck(m_autoCheck);
    if (m_seeded)
        save(); // from now on, this app's own
}

void StandaloneSession::read(const QJsonObject &json)
{
    m_audioInputs.clear();
    for (const auto &port : json.value(u"audioInputs").toArray())
        m_audioInputs << port.toString();
    m_bufferSize = json.value(u"bufferSize").toInt(0);
    m_outputDevice = json.value(u"outputDevice").toString();
}

void StandaloneSession::save()
{
    if (!files::writeJsonFile(m_path, QJsonObject{{QStringLiteral("version"), kVersion},
                                                  {QStringLiteral("audioInputs"), QJsonArray::fromStringList(m_audioInputs)},
                                                  {QStringLiteral("outputDevice"), m_outputDevice},
                                                  {QStringLiteral("bufferSize"), m_bufferSize},
                                                  {QStringLiteral("autoCheckUpdates"), m_autoCheck}}))
        qCWarning(lcSession) << "could not save" << m_path;
}

} // namespace krate
