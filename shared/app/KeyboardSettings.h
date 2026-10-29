#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace krate {

// The MIDI keyboard you play (QML singleton "ControllerKeyboard"): how many
// keys it has, the note its first (lowest) C sends at octave 0, and how far
// its octave buttons go down and up. The on-screen keyboards draw exactly
// that keyboard, key for key. Saved in ~/.config/krate/keyboard.json, shared
// by the rack and the standalone instruments. Defaults: a 49-key controller
// whose first C sends 35 at octave 0, octaves -3 to +4.
class KeyboardSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ControllerKeyboard)
    QML_SINGLETON

    Q_PROPERTY(int keys READ keys WRITE setKeys NOTIFY changed)
    Q_PROPERTY(int firstNote READ firstNote WRITE setFirstNote NOTIFY changed)
    Q_PROPERTY(int octaveDown READ octaveDown WRITE setOctaveDown NOTIFY changed)
    Q_PROPERTY(int octaveUp READ octaveUp WRITE setOctaveUp NOTIFY changed)
    // Waiting for the lowest key to be pressed (SET FROM KEY).
    Q_PROPERTY(bool capturing READ capturing WRITE setCapturing NOTIFY capturingChanged)

public:
    static constexpr int kDefaultKeys = 49;
    static constexpr int kDefaultFirstNote = 35;
    static constexpr int kDefaultOctaveDown = 3;
    static constexpr int kDefaultOctaveUp = 4;

    explicit KeyboardSettings(const QString &filePath, QObject *parent = nullptr);
    static KeyboardSettings *create(QQmlEngine *, QJSEngine *);
    static KeyboardSettings *instance(); // ~/.config/krate/keyboard.json

    int keys() const { return m_keys; }
    void setKeys(int keys);
    int firstNote() const { return m_firstNote; }
    void setFirstNote(int note);
    int octaveDown() const { return m_octaveDown; }
    void setOctaveDown(int octaves);
    int octaveUp() const { return m_octaveUp; }
    void setOctaveUp(int octaves);
    bool capturing() const { return m_capturing; }
    void setCapturing(bool on);

    // While capturing: the lowest key was pressed and sent this note (at octave 0).
    Q_INVOKABLE void captured(int note);
    Q_INVOKABLE void restoreDefaults();

signals:
    void changed();
    void capturingChanged();

private:
    void load();
    void save() const;

    QString m_filePath;
    int m_keys = kDefaultKeys;
    int m_firstNote = kDefaultFirstNote;
    int m_octaveDown = kDefaultOctaveDown;
    int m_octaveUp = kDefaultOctaveUp;
    bool m_capturing = false;
};

} // namespace krate
