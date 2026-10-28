#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace krate {

class IInstrument;

// A panel's view of any instrument's parameters, by key: values, defaults,
// display text, following changes made by MIDI (the instrument's "changed by
// audio" bits), and saving every position to panel.json 2 s after a change and
// on destruction. Instruments subclass it to register a QML singleton
// (e.g. "Klick"); QML binds like
//     value: Klick.revision >= 0 ? Klick.value("level") : 0
class ParamController : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS // known to QML tooling as the base of instrument singletons
    // Bumped whenever any value changes (from the panel or from MIDI).
    Q_PROPERTY(int revision READ revision NOTIFY paramsChanged)

public:
    ParamController(IInstrument &instrument, const QString &panelFilePath, QObject *parent);
    ~ParamController() override;

    int revision() const { return m_revision; }

    Q_INVOKABLE qreal value(const QString &key) const;
    Q_INVOKABLE void setValue(const QString &key, qreal value);
    Q_INVOKABLE qreal defaultValue(const QString &key) const;
    // The parameter's label from the descriptor table, e.g. "Bass Drum Tune"
    // (accessible names).
    Q_INVOKABLE QString label(const QString &key) const;
    // Positions of a selector or switch (0 = continuous).
    Q_INVOKABLE int detents(const QString &key) const;
    // Display text, e.g. "1340 Hz"; pass the value so bindings update with it.
    Q_INVOKABLE QString displayText(const QString &key, qreal value) const;

signals:
    void paramsChanged();

private:
    int indexOf(const QString &key) const;
    void poll();
    void changed();
    void save();

    IInstrument &m_instrument;
    QString m_panelFilePath;
    int m_revision = 0;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};

} // namespace krate
