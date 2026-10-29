#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "krimson/app/ProgramFile.h"

class QQmlEngine;
class QJSEngine;
class KrimsonController;
class SlotController;

namespace krimson {
class Engine;
}

// QML singleton "Programs" (docs/PLAY.md#programs-and-performances): 64
// programs (each a slot's sound with its morph targets) and 8 performances
// (all four slots' sounds and setup). Selecting a program loads it into the
// edited slot (or, by Program Change, into the slots its channel reaches); a dot
// shows while the edited slot differs from its program; STORE, then ▲▼ to
// choose the destination, then STORE again writes it. programs.json is saved
// on every store and 2 s after a selection; performances.json on every save.
class ProgramController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Programs)
    QML_SINGLETON

    Q_PROPERTY(int number READ number NOTIFY changed)            // the edited slot's program, 0..63
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(bool edited READ edited NOTIFY changed)
    Q_PROPERTY(bool storing READ storing NOTIFY changed)         // STORE armed
    Q_PROPERTY(int storeTarget READ storeTarget NOTIFY changed)  // where STORE will write
    Q_PROPERTY(int performance READ performance NOTIFY changed)  // 0..7
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    ProgramController(KrimsonController &panel, SlotController &slotSetup, krimson::Engine &engine,
                      const QString &programsPath, const QString &performancesPath, QObject *parent);
    ~ProgramController() override;

    static ProgramController *create(QQmlEngine *, QJSEngine *);

    int number() const;
    QString name() const;
    bool edited() const { return m_edited; }
    bool storing() const { return m_storing; }
    int storeTarget() const { return m_storeTarget; }
    int performance() const { return m_performances.current; }
    int revision() const { return m_revision; }
    const krimson::persistence::ProgramBank &bank() const { return m_bank; }

    Q_INVOKABLE void select(int index);              // into the edited slot
    void selectForSlot(int slot, int index);
    Q_INVOKABLE void step(int delta);                // ▲▼: the next program, or the store destination
    Q_INVOKABLE void pressStore();                   // arm, or write
    Q_INVOKABLE void cancelStore();
    Q_INVOKABLE QString programName(int index) const;

    Q_INVOKABLE void loadPerformance(int index);
    Q_INVOKABLE void savePerformance(int index);
    Q_INVOKABLE void stepPerformance(int delta);     // choose the performance number
    Q_INVOKABLE bool performanceUsed(int index) const;

    void save();

signals:
    void changed();

private:
    void updateEdited();
    void poll();
    void notify();

    static inline ProgramController *s_instance = nullptr;

    KrimsonController &m_panel;
    SlotController &m_slotSetup;
    krimson::Engine &m_engine;
    QString m_programsPath, m_performancesPath;
    krimson::persistence::ProgramBank m_bank;
    krimson::persistence::PerformanceBank m_performances;
    bool m_edited = false;
    bool m_storing = false;
    int m_storeTarget = 0;
    int m_revision = 0;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
