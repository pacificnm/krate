#include "RackMidiLearn.h"

#include <QJsonObject>
#include <QLoggingCategory>

#include "app/InstrumentRegistry.h"

Q_LOGGING_CATEGORY(lcRackLearn, "krate.learn")

RackMidiLearn::RackMidiLearn(krate::Rack &rack, std::vector<Device> devices, ActionHandler rackAction,
                             QObject *parent)
    : MidiLearnController(parent)
    , m_rack(rack)
    , m_devices(std::move(devices))
    , m_rackAction(std::move(rackAction))
{
}

RackMidiLearn::~RackMidiLearn()
{
    m_rack.arm({});
}

QString RackMidiLearn::paramKey(int device, int index) const
{
    if (device < 0 || device >= int(m_devices.size()))
        return {};
    const auto params = m_devices[std::size_t(device)].params;
    if (index < 0 || index >= int(params.size()))
        return {};
    return QString::fromLatin1(params[std::size_t(index)].key.data(), qsizetype(params[std::size_t(index)].key.size()));
}

int RackMidiLearn::deviceIndex(const QString &id) const
{
    for (std::size_t i = 0; i < m_devices.size(); ++i)
        if (m_devices[i].id == id)
            return int(i);
    return -1;
}

std::optional<RackMidiLearn::Mapping> RackMidiLearn::resolve(const QString &key) const
{
    using Kind = Mapping::Kind;
    if (const qsizetype a = kRackActions.indexOf(key.mid(5)); key.startsWith(u"rack:") && a >= 0)
        return Mapping{Kind::Action, krate::Rack::kRackDevice, int(a)};
    if (key == u"rack:next")
        return Mapping{Kind::FocusAction, -1, krate::Rack::kFocusNext};
    if (key == u"rack:previous")
        return Mapping{Kind::FocusAction, -1, krate::Rack::kFocusPrevious};
    if (key.startsWith(u"rack:mix:")) { // rack:mix:<n|master>:<control>
        const QStringList parts = key.split(u':');
        const qsizetype control = kMixerControls.indexOf(parts.value(3));
        if (parts.size() != 4 || control < 0)
            return std::nullopt;
        if (parts[2] == u"master")
            return masterControl(int(control)) ? std::optional(Mapping{Kind::Mixer, krate::Rack::kRackDevice, int(control)})
                                               : std::nullopt;
        bool ok = false;
        const int device = parts[2].toInt(&ok);
        if (ok && device >= 0 && device < int(m_devices.size()) && stripControl(int(control)))
            return Mapping{Kind::Mixer, device, int(control)};
        return std::nullopt;
    }
    if (key.startsWith(u"rack:focus:")) {
        bool ok = false;
        const int device = key.mid(11).toInt(&ok);
        if (ok && device >= 0 && device < int(m_devices.size()))
            return Mapping{Kind::FocusAction, -1, device};
        return std::nullopt;
    }

    // A panel key: the focused instrument's parameter or button.
    const int focused = m_rack.focus();
    if (focused < 0 || focused >= int(m_devices.size()))
        return std::nullopt;
    const Device &d = m_devices[std::size_t(focused)];
    for (std::size_t i = 0; i < d.params.size(); ++i)
        if (paramKey(focused, int(i)) == key)
            return Mapping{followFocus() ? Kind::FollowFocus : Kind::Param, focused, int(i)};
    if (const qsizetype action = d.actions.indexOf(key); action >= 0)
        return Mapping{Kind::Action, focused, int(action)};
    return std::nullopt;
}

bool RackMidiLearn::matches(const Mapping &m, const QString &key) const
{
    using Kind = Mapping::Kind;
    if (key.startsWith(u"rack:")) {
        const auto r = resolve(key);
        if (!r || r->kind != m.kind || r->target != m.target)
            return false;
        // Focus actions carry no device (it doesn't survive packing as -1); actions do.
        return (m.kind != Kind::Action && m.kind != Kind::Mixer) || r->device == m.device;
    }
    const int focused = m_rack.focus();
    switch (m.kind) {
    case Kind::Param:
        return m.device == focused && paramKey(m.device, m.target) == key;
    case Kind::FollowFocus:
        return paramKey(m.device, m.target) == key; // follows whatever has focus
    case Kind::Action:
        return m.device == focused && m.device < int(m_devices.size())
            && m_devices[std::size_t(m.device)].actions.value(m.target) == key;
    default:
        return false;
    }
}

std::vector<RackMidiLearn::Entry> RackMidiLearn::entries() const
{
    std::vector<Entry> out;
    for (int ch = 0; ch <= krate::Rack::kAnyChannel; ++ch)
        for (int cc = 0; cc < 128; ++cc)
            if (const Mapping m = m_rack.ccMapping(ch, cc); m.kind != Mapping::Kind::None)
                out.push_back({false, ch, cc, m});
    for (int ch = 0; ch < krate::Rack::kChannels; ++ch)
        for (int note = 0; note < 128; ++note)
            if (const Mapping m = m_rack.noteMapping(ch, note); m.kind != Mapping::Kind::None)
                out.push_back({true, ch, note, m});
    return out;
}

void RackMidiLearn::setEntry(const Entry &e, Mapping m)
{
    if (e.isNote)
        m_rack.setNoteMapping(e.channel, e.number, m);
    else
        m_rack.setCcMapping(e.channel, e.number, m);
}

bool RackMidiLearn::armTarget(const QString &key)
{
    if (key.isEmpty()) {
        m_rack.arm({});
        return true;
    }
    const auto target = resolve(key);
    m_rack.arm(target.value_or(Mapping{}));
    return target.has_value();
}

QString RackMidiLearn::mappingFor(const QString &key) const
{
    QStringList sources;
    for (const Entry &e : entries()) {
        if (!matches(e.mapping, key))
            continue;
        const QString channel = e.channel == krate::Rack::kAnyChannel ? QString() : QStringLiteral(" ch%1").arg(e.channel + 1);
        sources << (e.isNote ? QStringLiteral("note %1").arg(e.number) : QStringLiteral("CC %1").arg(e.number)) + channel;
    }
    return sources.join(QStringLiteral(", "));
}

QString RackMidiLearn::badgeFor(const QString &key) const
{
    return mappingFor(key); // already says "CC 47 ch16" / "note 36 ch10"
}

void RackMidiLearn::clear(const QString &key)
{
    bool changed = false;
    for (const Entry &e : entries()) {
        if (matches(e.mapping, key)) {
            setEntry(e, {});
            changed = true;
        }
    }
    if (changed)
        mappingsEdited();
}

void RackMidiLearn::clearAll()
{
    m_rack.clearMappings();
    arm(armedParam()); // disarm
    mappingsEdited();
}

void RackMidiLearn::drainEvents()
{
    while (const auto bound = m_rack.popLearned()) {
        qCInfo(lcRackLearn) << (bound->isNote ? "note" : "CC") << bound->number << "ch" << bound->channel + 1
                            << "learned";
        learned(armedParam()); // whatever was armed is now mapped
    }
    while (const auto fired = m_rack.popFired()) {
        if (fired->device == krate::Rack::kRackDevice) {
            if (fired->action >= 0 && fired->action < kRackActions.size() && m_rackAction)
                m_rackAction(kRackActions[fired->action]);
        } else if (fired->device >= 0 && fired->device < int(m_devices.size())) {
            const Device &d = m_devices[std::size_t(fired->device)];
            const QString action = d.actions.value(fired->action);
            if (d.instance && !action.isEmpty())
                d.instance->triggerAction(action);
            emit actionTriggered(action);
        }
    }
}

// --- rack.json -----------------------------------------------------------------

QJsonArray RackMidiLearn::saveMappings() const
{
    using Kind = Mapping::Kind;
    QJsonArray out;
    for (const Entry &e : entries()) {
        QJsonObject o{
            {QStringLiteral("source"), e.isNote ? QStringLiteral("note") : QStringLiteral("cc")},
            {QStringLiteral("channel"), e.channel == krate::Rack::kAnyChannel ? 0 : e.channel + 1}, // 0 = any
            {QStringLiteral("number"), e.number},
        };
        const Mapping &m = e.mapping;
        const QString deviceId = m.device >= 0 && m.device < int(m_devices.size()) ? m_devices[std::size_t(m.device)].id : QString();
        switch (m.kind) {
        case Kind::Param:
        case Kind::FollowFocus:
            o[QStringLiteral("kind")] = m.kind == Kind::Param ? QStringLiteral("param") : QStringLiteral("follow");
            o[QStringLiteral("device")] = deviceId;
            o[QStringLiteral("key")] = paramKey(m.device, m.target);
            break;
        case Kind::FocusAction:
            o[QStringLiteral("kind")] = QStringLiteral("focus");
            o[QStringLiteral("key")] = m.target == krate::Rack::kFocusNext       ? QStringLiteral("next")
                                     : m.target == krate::Rack::kFocusPrevious ? QStringLiteral("previous")
                                     : m.target >= 0 && m.target < int(m_devices.size()) ? m_devices[std::size_t(m.target)].id
                                                                                         : QString();
            break;
        case Kind::Action:
            o[QStringLiteral("kind")] = QStringLiteral("action");
            if (m.device == krate::Rack::kRackDevice) {
                o[QStringLiteral("device")] = QStringLiteral("rack");
                o[QStringLiteral("key")] = kRackActions.value(m.target);
            } else {
                o[QStringLiteral("device")] = deviceId;
                o[QStringLiteral("key")] = deviceId.isEmpty() ? QString() : m_devices[std::size_t(m.device)].actions.value(m.target);
            }
            break;
        case Kind::Mixer:
            o[QStringLiteral("kind")] = QStringLiteral("mixer");
            o[QStringLiteral("device")] = m.device == krate::Rack::kRackDevice ? QStringLiteral("master") : deviceId;
            o[QStringLiteral("key")] = o.value(u"device").toString().isEmpty() ? QString() : kMixerControls.value(m.target);
            break;
        case Kind::None:
            continue;
        }
        if (!o.value(QStringLiteral("key")).toString().isEmpty())
            out.append(o);
    }
    return out;
}

void RackMidiLearn::loadMappings(const QJsonArray &mappings)
{
    using Kind = Mapping::Kind;
    for (const auto &value : mappings) {
        const QJsonObject o = value.toObject();
        Entry e;
        e.isNote = o.value(u"source").toString() == u"note";
        const int channel = o.value(u"channel").toInt(-1);
        e.number = o.value(u"number").toInt(-1);
        if (channel < 0 || channel > 16 || e.number < 0 || e.number > 127 || (e.isNote && channel == 0))
            continue;
        e.channel = channel == 0 ? krate::Rack::kAnyChannel : channel - 1;

        const QString kind = o.value(u"kind").toString();
        const QString key = o.value(u"key").toString();
        const QString deviceId = o.value(u"device").toString();
        const int device = deviceIndex(deviceId);
        std::optional<Mapping> m;
        if (kind == u"param" || kind == u"follow") {
            for (std::size_t i = 0; device >= 0 && i < m_devices[std::size_t(device)].params.size(); ++i)
                if (paramKey(device, int(i)) == key)
                    m = Mapping{kind == u"param" ? Kind::Param : Kind::FollowFocus, device, int(i)};
        } else if (kind == u"focus") {
            if (key == u"next")
                m = Mapping{Kind::FocusAction, -1, krate::Rack::kFocusNext};
            else if (key == u"previous")
                m = Mapping{Kind::FocusAction, -1, krate::Rack::kFocusPrevious};
            else if (const int target = deviceIndex(key); target >= 0)
                m = Mapping{Kind::FocusAction, -1, target};
        } else if (kind == u"action") {
            if (deviceId == u"rack") {
                if (const qsizetype a = kRackActions.indexOf(key); a >= 0)
                    m = Mapping{Kind::Action, krate::Rack::kRackDevice, int(a)};
            } else if (device >= 0) {
                if (const qsizetype a = m_devices[std::size_t(device)].actions.indexOf(key); a >= 0)
                    m = Mapping{Kind::Action, device, int(a)};
            }
        } else if (kind == u"mixer") {
            const qsizetype control = kMixerControls.indexOf(key);
            if (deviceId == u"master" && masterControl(int(control)))
                m = Mapping{Kind::Mixer, krate::Rack::kRackDevice, int(control)};
            else if (device >= 0 && stripControl(int(control)))
                m = Mapping{Kind::Mixer, device, int(control)};
        }
        const bool button = m && (m->kind == Kind::FocusAction || m->kind == Kind::Action
                                  || (m->kind == Kind::Mixer && krate::Rack::isMixerButton(m->target)));
        if (!m || (e.isNote && !button)) {
            qCWarning(lcRackLearn) << "skipping unknown mapping" << o;
            continue;
        }
        setEntry(e, *m);
    }
    mappingsEdited();
}
