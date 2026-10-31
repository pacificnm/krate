#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "kessel/app/KitFile.h"

class QQmlEngine;
class QJSEngine;
class DrumsController;
class SampleController;

// QML singleton "Kits": Kessel's 16 kits (docs/KITS.md). Choosing a kit loads
// its sound and mix knobs into the panel; any edit after that shows the edited
// dot. WRITE arms a store: the arrows then choose the slot, WRITE again
// stores the panel there (cancel() drops it). kits.json is saved on every
// store, and 2 s after the selection changes.
class KitController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Kits)
    QML_SINGLETON

    Q_PROPERTY(int index READ index NOTIFY changed)          // 0..15, the kit loaded
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(bool edited READ edited NOTIFY changed)       // the panel differs from the kit
    Q_PROPERTY(bool writing READ writing NOTIFY changed)     // WRITE armed
    Q_PROPERTY(int target READ target NOTIFY changed)        // while writing: the slot to store into
    Q_PROPERTY(QString targetName READ targetName NOTIFY changed)

public:
    // Not default-constructible on purpose (see Korrode's SynthController).
    // samples: the kit's SAMPLE voice file is loaded with it and stored by WRITE.
    KitController(DrumsController &panel, SampleController &samples, const QString &filePath, QObject *parent);
    ~KitController() override;

    static KitController *create(QQmlEngine *, QJSEngine *);

    int index() const { return m_bank.current; }
    QString name() const { return m_bank.kits[std::size_t(m_bank.current)].name; }
    bool edited() const { return m_edited; }
    bool writing() const { return m_writing; }
    int target() const { return m_target; }
    QString targetName() const { return m_bank.kits[std::size_t(m_target)].name; }
    const kessel::persistence::KitBank &kitBank() const { return m_bank; }

    Q_INVOKABLE void select(int index); // load kit 0..15
    Q_INVOKABLE void step(int delta);   // the arrows: the next kit, or (writing) the next slot
    Q_INVOKABLE void pressWrite();      // arm; armed: store into the target
    Q_INVOKABLE void cancel();          // drop an armed store

    void save();

signals:
    void changed();

private:
    void updateEdited();

    static inline KitController *s_instance = nullptr;

    DrumsController &m_panel;
    SampleController &m_samples;
    QString m_filePath;
    kessel::persistence::KitBank m_bank;
    bool m_writing = false;
    bool m_edited = false;
    int m_target = 0;
    QTimer m_saveTimer;
};
