#include "KeyboardSettings.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace krate {

KeyboardSettings::KeyboardSettings(const QString &filePath, QObject *parent) : QObject(parent), m_filePath(filePath)
{
    load();
}

KeyboardSettings *KeyboardSettings::instance()
{
    static KeyboardSettings *settings = new KeyboardSettings(
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/krate/keyboard.json"));
    return settings;
}

KeyboardSettings *KeyboardSettings::create(QQmlEngine *, QJSEngine *)
{
    KeyboardSettings *settings = instance();
    QJSEngine::setObjectOwnership(settings, QJSEngine::CppOwnership);
    return settings;
}

void KeyboardSettings::setKeys(int keys)
{
    keys = std::clamp(keys, 13, 88);
    if (keys == m_keys)
        return;
    m_keys = keys;
    save();
    emit changed();
}

void KeyboardSettings::setFirstNote(int note)
{
    note = std::clamp(note, 0, 127);
    if (note == m_firstNote)
        return;
    m_firstNote = note;
    save();
    emit changed();
}

void KeyboardSettings::setOctaveDown(int octaves)
{
    octaves = std::clamp(octaves, 0, 8);
    if (octaves == m_octaveDown)
        return;
    m_octaveDown = octaves;
    save();
    emit changed();
}

void KeyboardSettings::setOctaveUp(int octaves)
{
    octaves = std::clamp(octaves, 0, 8);
    if (octaves == m_octaveUp)
        return;
    m_octaveUp = octaves;
    save();
    emit changed();
}

void KeyboardSettings::setCapturing(bool on)
{
    if (on == m_capturing)
        return;
    m_capturing = on;
    emit capturingChanged();
}

void KeyboardSettings::captured(int note)
{
    if (!m_capturing)
        return;
    setCapturing(false);
    setFirstNote(note);
}

void KeyboardSettings::restoreDefaults()
{
    m_keys = kDefaultKeys;
    m_firstNote = kDefaultFirstNote;
    m_octaveDown = kDefaultOctaveDown;
    m_octaveUp = kDefaultOctaveUp;
    save();
    emit changed();
}

void KeyboardSettings::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return; // the defaults
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    m_keys = std::clamp(o.value(u"keys").toInt(kDefaultKeys), 13, 88);
    m_firstNote = std::clamp(o.value(u"firstNote").toInt(kDefaultFirstNote), 0, 127);
    m_octaveDown = std::clamp(o.value(u"octaveDown").toInt(kDefaultOctaveDown), 0, 8);
    m_octaveUp = std::clamp(o.value(u"octaveUp").toInt(kDefaultOctaveUp), 0, 8);
}

void KeyboardSettings::save() const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile out(m_filePath);
    if (!out.open(QIODevice::WriteOnly))
        return;
    out.write(QJsonDocument(QJsonObject{{QStringLiteral("keys"), m_keys},
                                        {QStringLiteral("firstNote"), m_firstNote},
                                        {QStringLiteral("octaveDown"), m_octaveDown},
                                        {QStringLiteral("octaveUp"), m_octaveUp}})
                  .toJson());
    out.commit();
}

} // namespace krate
