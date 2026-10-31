#pragma once

#include <functional>

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// QML singleton "Presets": Krunch's presets (docs/PRESETS.md). A preset is
// the whole amp: every knob of all four channels and the power amp, each
// channel's IRs, the pedalboard (pedals, order, places, footswitches, knobs):
// the instance's saveSound(), the same snapshot songs keep. Kept in
// presets.json; the preset last loaded or saved is remembered.
class PresetController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Presets)
    QML_SINGLETON

    Q_PROPERTY(QStringList names READ names NOTIFY changed)
    // The preset last loaded or saved ("" = none), and whether the amp has changed since.
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(bool modified READ modified NOTIFY changed)

public:
    using Save = std::function<QJsonObject()>;
    using Load = std::function<void(const QJsonObject &)>;

    // Not default-constructible on purpose (see Korrode's SynthController).
    // save / load: the instance's whole sound.
    PresetController(const QString &filePath, Save save, Load load, QObject *parent);
    ~PresetController() override;

    static PresetController *create(QQmlEngine *, QJSEngine *);

    QStringList names() const;
    QString current() const { return m_current; }
    bool modified() const { return m_modified; }

    Q_INVOKABLE void load(const QString &name);
    Q_INVOKABLE void step(int by); // the next (1) or previous (-1) preset, round the list
    // The amp as it is, under `name` (replacing a preset of that name). False for an empty name.
    Q_INVOKABLE bool saveAs(const QString &name);
    Q_INVOKABLE bool save() { return saveAs(m_current); } // over the current one
    Q_INVOKABLE bool rename(const QString &from, const QString &to);
    Q_INVOKABLE void remove(const QString &name);

    // The amp changed (its controllers' signals): recheck `modified`.
    void ampChanged();

signals:
    void changed();

private:
    struct Preset
    {
        QString name;
        QJsonObject sound;
    };
    int indexOf(const QString &name) const;
    void setCurrent(const QString &name, const QJsonObject &sound);
    void readFile();
    void writeFile() const;

    static inline PresetController *s_instance = nullptr;

    QString m_filePath;
    Save m_save;
    Load m_load;
    QList<Preset> m_presets;
    QString m_current;
    QJsonObject m_currentSound; // as loaded or saved, to tell `modified`
    bool m_modified = false;
    bool m_loading = false;
    QTimer m_check; // changes come in bursts (a preset load sets everything): check once after
};
