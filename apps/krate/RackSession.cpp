#include "RackSession.h"

#include <QJsonObject>
#include <QLoggingCategory>

#include "RackController.h"
#include "RackMidiLearn.h"
#include "app/JsonFile.h"
#include "rack/Rack.h"

Q_LOGGING_CATEGORY(lcSession, "krate.session")

RackSession::RackSession(const QString &path, RackController &controller, krate::Rack &rack, RackMidiLearn &learn,
                         const QStringList &deviceIds, QObject *parent)
    : QObject(parent)
    , m_path(path)
    , m_controller(controller)
    , m_rack(rack)
    , m_learn(learn)
    , m_ids(deviceIds)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &RackSession::save);
}

RackSession::~RackSession()
{
    if (m_saveTimer.isActive())
        save();
}

void RackSession::load()
{
    QJsonObject json;
    switch (krate::files::readVersionedJson(m_path, kVersion, &json)) {
    case krate::files::LoadResult::Loaded:
        qCInfo(lcSession) << "loaded session" << m_path;
        break;
    case krate::files::LoadResult::Missing:
        break;
    case krate::files::LoadResult::Invalid:
        qCWarning(lcSession) << "could not read" << m_path << "- starting with defaults";
        break;
    case krate::files::LoadResult::NewerVersion:
        qCWarning(lcSession) << m_path << "was written by a newer krate; saving to .v1 instead";
        m_path += QStringLiteral(".v1");
        break;
    }

    if (!json.isEmpty()) {
        m_midiInput = json.value(u"midiInput").toString(m_midiInput);
        if (const double bpm = json.value(u"tempo").toDouble(0); bpm > 0)
            m_controller.setBpm(bpm);
        m_controller.setFader(-1, json.value(u"master").toDouble(RackController::kUnityFader));

        const QJsonObject devices = json.value(u"devices").toObject();
        for (int i = 0; i < m_ids.size(); ++i) {
            if (!devices.contains(m_ids[i]))
                continue; // a new instrument keeps its defaults
            const QJsonObject d = devices.value(m_ids[i]).toObject();
            m_controller.setFader(i, d.value(u"fader").toDouble(RackController::kUnityFader));
            m_controller.setPan(i, d.value(u"pan").toDouble(0.0));
            m_controller.setMute(i, d.value(u"mute").toBool(false));
            m_controller.setSolo(i, d.value(u"solo").toBool(false));
            // Channel routing: notes on this channel (1..16) go to the instrument; 0 = focus only.
            for (int ch = 0; ch < krate::Rack::kChannels; ++ch)
                if (m_rack.channelDevice(ch) == i)
                    m_rack.setChannelDevice(ch, -1);
            if (const int ch = d.value(u"channel").toInt(0); ch >= 1 && ch <= 16)
                m_rack.setChannelDevice(ch - 1, i);
        }
        if (const int focus = int(m_ids.indexOf(json.value(u"focus").toString())); focus >= 0)
            m_controller.setFocus(focus);

        m_rack.clearMappings();
        m_learn.loadMappings(json.value(u"mappings").toArray());
    }

    // From here on, any change is saved.
    connect(&m_controller, &RackController::focusChanged, this, &RackSession::changed);
    connect(&m_controller, &RackController::bpmChanged, this, &RackSession::changed);
    connect(&m_controller, &RackController::mixerChanged, this, &RackSession::changed);
    connect(&m_learn, &RackMidiLearn::mappingsChanged, this, &RackSession::changed);
}

void RackSession::save()
{
    m_saveTimer.stop();
    QJsonObject devices;
    for (int i = 0; i < m_ids.size(); ++i) {
        int channel = 0;
        for (int ch = 0; ch < krate::Rack::kChannels && channel == 0; ++ch)
            if (m_rack.channelDevice(ch) == i)
                channel = ch + 1;
        devices.insert(m_ids[i], QJsonObject{
                                     {QStringLiteral("fader"), m_controller.fader(i)},
                                     {QStringLiteral("pan"), m_controller.pan(i)},
                                     {QStringLiteral("mute"), m_controller.mute(i)},
                                     {QStringLiteral("solo"), m_controller.solo(i)},
                                     {QStringLiteral("channel"), channel},
                                 });
    }
    const QJsonObject json{
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("tempo"), m_controller.bpm()},
        {QStringLiteral("focus"), m_ids.value(m_controller.focus())},
        {QStringLiteral("midiInput"), m_midiInput},
        {QStringLiteral("master"), m_controller.fader(-1)},
        {QStringLiteral("devices"), devices},
        {QStringLiteral("mappings"), m_learn.saveMappings()},
    };
    if (!krate::files::writeJsonFile(m_path, json))
        qCWarning(lcSession) << "could not save session to" << m_path;
}
