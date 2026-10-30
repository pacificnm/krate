#include "PresetController.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QLoggingCategory>

#include "app/JsonFile.h"

Q_LOGGING_CATEGORY(lcPresets, "krunch.presets")

namespace {
constexpr int kVersion = 1;
}

PresetController::PresetController(const QString &filePath, Save save, Load load, QObject *parent)
    : QObject(parent)
    , m_filePath(filePath)
    , m_save(std::move(save))
    , m_load(std::move(load))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_check.setSingleShot(true);
    m_check.setInterval(100);
    connect(&m_check, &QTimer::timeout, this, [this] {
        const bool modified = !m_current.isEmpty() && m_save() != m_currentSound;
        if (modified != m_modified) {
            m_modified = modified;
            emit changed();
        }
    });
    readFile();
}

PresetController::~PresetController()
{
    s_instance = nullptr;
}

PresetController *PresetController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QStringList PresetController::names() const
{
    QStringList result;
    for (const Preset &p : m_presets)
        result << p.name;
    return result;
}

int PresetController::indexOf(const QString &name) const
{
    for (qsizetype i = 0; i < m_presets.size(); ++i)
        if (m_presets[i].name == name)
            return int(i);
    return -1;
}

void PresetController::load(const QString &name)
{
    const int i = indexOf(name);
    if (i < 0)
        return;
    m_loading = true;
    m_load(m_presets[i].sound);
    m_loading = false;
    // What the amp holds now (a pedal or IR it doesn't have is left out) counts as unmodified.
    setCurrent(name, m_save());
}

void PresetController::step(int by)
{
    if (m_presets.isEmpty())
        return;
    const int count = int(m_presets.size());
    const int at = indexOf(m_current);
    const int next = at < 0 ? (by > 0 ? 0 : count - 1) : ((at + by) % count + count) % count;
    load(m_presets[next].name);
}

bool PresetController::saveAs(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;
    const QJsonObject sound = m_save();
    if (const int i = indexOf(trimmed); i >= 0)
        m_presets[i].sound = sound;
    else
        m_presets.append({trimmed, sound});
    setCurrent(trimmed, sound);
    writeFile();
    return true;
}

bool PresetController::rename(const QString &from, const QString &to)
{
    const QString trimmed = to.trimmed();
    const int i = indexOf(from);
    if (i < 0 || trimmed.isEmpty() || (trimmed != from && indexOf(trimmed) >= 0))
        return false;
    m_presets[i].name = trimmed;
    if (m_current == from)
        m_current = trimmed;
    writeFile();
    emit changed();
    return true;
}

void PresetController::remove(const QString &name)
{
    const int i = indexOf(name);
    if (i < 0)
        return;
    m_presets.removeAt(i);
    if (m_current == name) {
        m_current.clear();
        m_currentSound = {};
        m_modified = false;
    }
    writeFile();
    emit changed();
}

void PresetController::ampChanged()
{
    if (!m_loading && !m_current.isEmpty())
        m_check.start();
}

void PresetController::setCurrent(const QString &name, const QJsonObject &sound)
{
    m_current = name;
    m_currentSound = sound;
    m_modified = false;
    m_check.stop();
    writeFile(); // remembers the current one
    emit changed();
}

void PresetController::readFile()
{
    QJsonObject json;
    if (krate::files::readVersionedJson(m_filePath, kVersion, &json) != krate::files::LoadResult::Loaded)
        return;
    for (const QJsonValue &v : json.value(u"presets").toArray()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(u"name").toString().trimmed();
        if (!name.isEmpty() && indexOf(name) < 0)
            m_presets.append({name, o.value(u"sound").toObject()});
    }
    // The current one, as it was saved (whether the amp still matches is checked as it changes).
    if (const int i = indexOf(json.value(u"current").toString()); i >= 0) {
        m_current = m_presets[i].name;
        m_currentSound = m_presets[i].sound;
        m_modified = m_save() != m_currentSound;
    }
}

void PresetController::writeFile() const
{
    QJsonArray presets;
    for (const Preset &p : m_presets)
        presets.append(QJsonObject{{QStringLiteral("name"), p.name}, {QStringLiteral("sound"), p.sound}});
    if (!krate::files::writeJsonFile(m_filePath, QJsonObject{{QStringLiteral("version"), kVersion},
                                                             {QStringLiteral("current"), m_current},
                                                             {QStringLiteral("presets"), presets}}))
        qCWarning(lcPresets) << "could not save" << m_filePath;
}
