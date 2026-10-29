#include "krimson/app/SlotController.h"

#include <algorithm>

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "krimson/app/KrimsonController.h"
#include "krimson/dsp/Engine.h"
#include "krimson/dsp/Params.h"

Q_LOGGING_CATEGORY(lcSlots, "krimson.slots")

namespace {
constexpr int kFileVersion = 1;
constexpr int kSaveDelayMs = 2000;
const char *const kZones[] = {"whole", "lower", "upper"};

QString keyOf(int index)
{
    const auto k = krimson::kParams[std::size_t(index)].key;
    return QString::fromUtf8(k.data(), qsizetype(k.size()));
}
} // namespace

using Zone = krimson::Engine::Zone;
using krate::files::LoadResult;

SlotController::SlotController(KrimsonController &panel, krimson::Engine &engine, const QString &filePath,
                               QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    load();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &SlotController::save);
    // Any knob move (panel or MIDI) changes a slot's sound: save it.
    connect(&m_panel, &krate::ParamController::paramsChanged, this, [this] { m_saveTimer.start(); });
}

SlotController::~SlotController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

SlotController *SlotController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void SlotController::load()
{
    QJsonObject json;
    switch (krate::files::readVersionedJson(m_filePath, kFileVersion, &json)) {
    case LoadResult::Loaded:
        break;
    case LoadResult::Missing:
        return; // first run: the engine's defaults
    case LoadResult::Invalid:
        qCWarning(lcSlots) << "could not read" << m_filePath << "- starting with default slots";
        return;
    case LoadResult::NewerVersion:
        qCWarning(lcSlots) << m_filePath << "was written by a newer version; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        return;
    }
    const QJsonArray slotList = json.value(u"slots").toArray();
    unsigned lit = 0;
    for (int slot = 0; slot < krimson::SlotCount && slot < slotList.size(); ++slot) {
        const QJsonObject so = slotList.at(slot).toObject();
        const QJsonObject params = so.value(u"params").toObject();
        for (int i = 0; i < krimson::SlotParamCount; ++i)
            if (const QJsonValue v = params.value(keyOf(i)); v.isDouble())
                m_engine.setSlotParam(slot, krimson::ParamId(i),
                                      krate::snapToDetent(krimson::kParams[std::size_t(i)], float(std::clamp(v.toDouble(), 0.0, 1.0))));
        m_engine.setSlotChannel(slot, std::clamp(so.value(u"channel").toInt(3 + slot), 0, 16) - 1);
        m_engine.setSlotOn(slot, so.value(u"on").toBool(true));
        const QString zone = so.value(u"zone").toString();
        m_engine.setSlotZone(slot, zone == u"lower" ? Zone::Lower : zone == u"upper" ? Zone::Upper : Zone::Whole);
        if (so.value(u"lit").toBool(slot == 0))
            lit |= 1u << slot;
    }
    m_engine.setLitSlots(lit ? lit : 1u);
    m_engine.setSplitNote(std::clamp(json.value(u"split").toInt(-1), -1, 127));
    m_panel.setEditedSlot(std::clamp(json.value(u"edited").toInt(0), 0, krimson::SlotCount - 1));
    m_engine.markAllChanged(); // the panel shows the loaded values
    qCInfo(lcSlots) << "loaded slots from" << m_filePath;
}

void SlotController::save()
{
    m_saveTimer.stop();
    QJsonArray slotList;
    for (int slot = 0; slot < krimson::SlotCount; ++slot) {
        QJsonObject params;
        for (int i = 0; i < krimson::SlotParamCount; ++i)
            params.insert(keyOf(i), double(m_engine.slotParam(slot, krimson::ParamId(i))));
        slotList.append(QJsonObject{
            {QStringLiteral("params"), params},
            {QStringLiteral("channel"), channel(slot)},
            {QStringLiteral("on"), isOn(slot)},
            {QStringLiteral("zone"), QString::fromLatin1(kZones[zone(slot)])},
            {QStringLiteral("lit"), isLit(slot)},
        });
    }
    const QJsonObject json{
        {QStringLiteral("version"), kFileVersion},
        {QStringLiteral("edited"), m_engine.editedSlot()},
        {QStringLiteral("split"), splitNote()},
        {QStringLiteral("slots"), slotList},
    };
    if (!krate::files::writeJsonFile(m_filePath, json))
        qCWarning(lcSlots) << "could not save slots to" << m_filePath;
}

void SlotController::edited()
{
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void SlotController::pressSlot(int slot, bool shift)
{
    if (slot < 0 || slot >= krimson::SlotCount)
        return;
    unsigned lit = m_engine.litSlots();
    if (!shift) {
        lit = 1u << slot;
    } else {
        lit ^= 1u << slot;
        if (lit == 0) // at least one slot stays lit
            lit = 1u << slot;
    }
    m_engine.setLitSlots(lit);
    if (lit & (1u << slot))
        m_panel.setEditedSlot(slot);
    else if (!(lit & (1u << m_engine.editedSlot()))) // unlit the edited slot: edit the first lit one
        for (int s = 0; s < krimson::SlotCount; ++s)
            if (lit & (1u << s)) {
                m_panel.setEditedSlot(s);
                break;
            }
    edited();
}

bool SlotController::isLit(int slot) const
{
    return slot >= 0 && slot < krimson::SlotCount && (m_engine.litSlots() & (1u << slot));
}

int SlotController::channel(int slot) const
{
    return slot >= 0 && slot < krimson::SlotCount ? m_engine.slotChannel(slot) + 1 : 0;
}

void SlotController::setChannel(int slot, int channel)
{
    if (slot < 0 || slot >= krimson::SlotCount || channel < 0 || channel > 16 || channel == this->channel(slot))
        return;
    m_engine.setSlotChannel(slot, channel - 1);
    edited();
}

bool SlotController::isOn(int slot) const
{
    return slot >= 0 && slot < krimson::SlotCount && m_engine.slotOn(slot);
}

void SlotController::setOn(int slot, bool on)
{
    if (slot < 0 || slot >= krimson::SlotCount || on == isOn(slot))
        return;
    m_engine.setSlotOn(slot, on);
    edited();
}

int SlotController::zone(int slot) const
{
    return slot >= 0 && slot < krimson::SlotCount ? int(m_engine.slotZone(slot)) : 0;
}

void SlotController::setZone(int slot, int zone)
{
    if (slot < 0 || slot >= krimson::SlotCount || zone < 0 || zone > 2 || zone == this->zone(slot))
        return;
    m_engine.setSlotZone(slot, Zone(zone));
    edited();
}

int SlotController::splitNote() const
{
    return m_engine.splitNote();
}

void SlotController::setSplitNote(int note)
{
    note = std::clamp(note, -1, 127);
    if (note == splitNote())
        return;
    m_engine.setSplitNote(note);
    edited();
}
