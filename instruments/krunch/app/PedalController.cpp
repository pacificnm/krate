#include "PedalController.h"

#include <QJSEngine>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>

#include "app/JsonFile.h"
#include "dsp/Krunch.h"

Q_LOGGING_CATEGORY(lcPedals, "krunch.pedals")

namespace {
constexpr int kVersion = 1;
constexpr int kFadeOutMs = 30; // a removed pedal switches off (10 ms fade) before it goes
} // namespace

PedalController::PedalController(krunch::Krunch &engine, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_board(engine.frontPedals(), engine.loopPedals())
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_collectTimer.setInterval(250);
    connect(&m_collectTimer, &QTimer::timeout, this, [this] { m_board.collect(m_engine.blocksProcessed()); });
    m_collectTimer.start();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &PedalController::saveFile);

    QJsonObject json;
    if (krate::files::readVersionedJson(m_filePath, kVersion, &json) == krate::files::LoadResult::Loaded) {
        load(json.value(u"pedals").toArray());
        m_saveTimer.stop(); // just loaded
    }
}

PedalController::~PedalController()
{
    if (m_saveTimer.isActive())
        saveFile();
    s_instance = nullptr;
}

PedalController *PedalController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList PedalController::types() const
{
    QVariantList result;
    for (const krunch::PedalInfo *info : krunch::pedalTypes())
        result << QVariantMap{{QStringLiteral("type"), QString::fromUtf8(info->type)},
                              {QStringLiteral("name"), QString::fromUtf8(info->name)},
                              {QStringLiteral("colour"), QString::fromUtf8(info->colour)}};
    return result;
}

QVariantList PedalController::pedals() const
{
    QVariantList result;
    for (int p = 0; p < m_board.count(); ++p) {
        krunch::Pedal &pedal = const_cast<krunch::Pedalboard &>(m_board).pedal(p);
        const krunch::PedalInfo &info = pedal.info();
        QVariantList knobs;
        for (std::size_t k = 0; k < info.knobs.size(); ++k)
            knobs << QVariantMap{{QStringLiteral("key"), QString::fromUtf8(info.knobs[k].key)},
                                 {QStringLiteral("label"), QString::fromUtf8(info.knobs[k].label)},
                                 {QStringLiteral("value"), pedal.knob(int(k))},
                                 {QStringLiteral("defaultValue"), info.knobs[k].defaultValue}};
        result << QVariantMap{{QStringLiteral("type"), QString::fromUtf8(info.type)},
                              {QStringLiteral("name"), QString::fromUtf8(info.name)},
                              {QStringLiteral("colour"), QString::fromUtf8(info.colour)},
                              {QStringLiteral("on"), pedal.on()},
                              {QStringLiteral("loop"), m_board.inLoop(p)},
                              {QStringLiteral("knobs"), knobs}};
    }
    return result;
}

bool PedalController::isOn(int position) const
{
    return valid(position) && const_cast<krunch::Pedalboard &>(m_board).pedal(position).on();
}

bool PedalController::inLoop(int position) const
{
    return valid(position) && m_board.inLoop(position);
}

void PedalController::setInLoop(int position, bool loop)
{
    if (!valid(position) || m_board.inLoop(position) == loop)
        return;
    m_board.setInLoop(position, loop, m_engine.blocksProcessed());
    bump();
}

qreal PedalController::knob(int position, int knob) const
{
    return valid(position) ? const_cast<krunch::Pedalboard &>(m_board).pedal(position).knob(knob) : 0.0;
}

QString PedalController::knobText(int position, int knob, qreal value) const
{
    if (!valid(position))
        return {};
    const auto &knobs = const_cast<krunch::Pedalboard &>(m_board).pedal(position).info().knobs;
    if (knob < 0 || std::size_t(knob) >= knobs.size())
        return {};
    return QString::fromStdString(knobs[std::size_t(knob)].format(float(value)));
}

void PedalController::add(const QString &type)
{
    if (!m_board.add(type.toStdString(), m_engine.sampleRate(), m_engine.blocksProcessed()))
        return;
    bumpLayout();
}

void PedalController::remove(int position)
{
    if (!valid(position))
        return;
    // Switch it off and let it fade, then take it out (by identity: the board may change meanwhile).
    krunch::Pedal *pedal = &m_board.pedal(position);
    pedal->setOn(false);
    QTimer::singleShot(kFadeOutMs, this, [this, pedal] {
        for (int p = 0; p < m_board.count(); ++p)
            if (&m_board.pedal(p) == pedal) {
                m_board.remove(p, m_engine.blocksProcessed());
                bumpLayout();
                return;
            }
    });
    bump();
}

void PedalController::move(int from, int to)
{
    if (!valid(from) || !valid(to) || from == to)
        return;
    m_board.move(from, to, m_engine.blocksProcessed());
    bumpLayout();
}

void PedalController::setOn(int position, bool on)
{
    if (!valid(position) || m_board.pedal(position).on() == on)
        return;
    m_board.pedal(position).setOn(on);
    bump();
}

void PedalController::toggle(int position)
{
    if (valid(position))
        setOn(position, !m_board.pedal(position).on());
}

void PedalController::setKnob(int position, int knob, qreal value)
{
    if (!valid(position))
        return;
    m_board.pedal(position).setKnob(knob, float(value));
    bump();
}

QJsonArray PedalController::save() const
{
    QJsonArray result;
    for (int p = 0; p < m_board.count(); ++p) {
        krunch::Pedal &pedal = const_cast<krunch::Pedalboard &>(m_board).pedal(p);
        QJsonObject knobs;
        for (std::size_t k = 0; k < pedal.info().knobs.size(); ++k)
            knobs.insert(QString::fromUtf8(pedal.info().knobs[k].key), pedal.knob(int(k)));
        result.append(QJsonObject{{QStringLiteral("type"), QString::fromUtf8(pedal.info().type)},
                                  {QStringLiteral("on"), pedal.on()},
                                  {QStringLiteral("loop"), m_board.inLoop(p)},
                                  {QStringLiteral("knobs"), knobs}});
    }
    return result;
}

void PedalController::load(const QJsonArray &pedals)
{
    m_board.clear(m_engine.blocksProcessed());
    for (const QJsonValue &v : pedals) {
        const QJsonObject o = v.toObject();
        const int loop = o.contains(u"loop") ? (o.value(u"loop").toBool() ? 1 : 0) : -1; // -1: where its kind goes
        if (!m_board.add(o.value(u"type").toString().toStdString(), m_engine.sampleRate(), m_engine.blocksProcessed(), -1, loop)) {
            qCWarning(lcPedals) << "skipping a pedal we don't have:" << o.value(u"type").toString();
            continue;
        }
        krunch::Pedal &pedal = m_board.pedal(m_board.count() - 1);
        const QJsonObject knobs = o.value(u"knobs").toObject();
        for (std::size_t k = 0; k < pedal.info().knobs.size(); ++k) {
            const QString key = QString::fromUtf8(pedal.info().knobs[k].key);
            if (knobs.contains(key))
                pedal.setKnob(int(k), float(knobs.value(key).toDouble()));
        }
        pedal.setOn(o.value(u"on").toBool(true));
    }
    bumpLayout();
}

void PedalController::bump()
{
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void PedalController::bumpLayout()
{
    ++m_layoutRevision;
    emit layoutChanged();
    bump();
}

void PedalController::saveFile()
{
    m_saveTimer.stop();
    if (!krate::files::writeJsonFile(m_filePath, QJsonObject{{QStringLiteral("version"), kVersion},
                                                             {QStringLiteral("pedals"), save()}}))
        qCWarning(lcPedals) << "could not save" << m_filePath;
}
