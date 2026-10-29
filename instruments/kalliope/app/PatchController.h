#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "kalliope/app/PatchFile.h"

class QQmlEngine;
class QJSEngine;
class KalliopeController;

namespace kalliope {
class Engine;
}

// QML singleton "Patches": Kalliope's 64 patches (docs/PLAY.md#patches).
// Selecting a patch loads its values into the panel; any edit after that shows
// the edited dot; WRITE then a PATCH button stores the panel into that slot;
// MANUAL frees the panel from the patch. Program Change selects patches.
// patches.json is saved on every write, and 2 s after the selection changes.
class PatchController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Patches)
    QML_SINGLETON

    Q_PROPERTY(int bank READ bank NOTIFY changed)               // 0..7, of the current patch
    Q_PROPERTY(int number READ number NOTIFY changed)           // 0..7
    Q_PROPERTY(int pendingBank READ pendingBank NOTIFY changed) // a BANK pressed, waiting for a PATCH; -1 none
    Q_PROPERTY(bool manual READ manual NOTIFY changed)
    Q_PROPERTY(bool edited READ edited NOTIFY changed)          // the panel differs from the patch
    Q_PROPERTY(bool writing READ writing NOTIFY changed)        // WRITE armed: the next PATCH press stores
    Q_PROPERTY(QString name READ name NOTIFY changed)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    PatchController(KalliopeController &panel, kalliope::Engine &engine, const QString &filePath, QObject *parent);
    ~PatchController() override;

    static PatchController *create(QQmlEngine *, QJSEngine *);

    int bank() const { return m_bank.current / kalliope::persistence::kPatchesPerBank; }
    int number() const { return m_bank.current % kalliope::persistence::kPatchesPerBank; }
    int pendingBank() const { return m_pendingBank; }
    bool manual() const { return m_bank.manual; }
    bool edited() const { return m_edited; }
    bool writing() const { return m_writing; }
    QString name() const;
    const kalliope::persistence::PatchBank &patchBank() const { return m_bank; }

    Q_INVOKABLE void pressBank(int bank);     // 0..7
    Q_INVOKABLE void pressPatch(int number);  // 0..7: select, or store when writing
    Q_INVOKABLE void toggleWrite();
    Q_INVOKABLE void pressManual();
    Q_INVOKABLE void step(int delta);         // the display's arrows: next / previous patch
    Q_INVOKABLE void select(int index);       // 0..63

    void save();

signals:
    void changed();

private:
    void updateEdited();
    void poll();
    void selectionChanged(); // notify, and save in 2 s

    static inline PatchController *s_instance = nullptr;

    KalliopeController &m_panel;
    kalliope::Engine &m_engine;
    QString m_filePath;
    kalliope::persistence::PatchBank m_bank;
    int m_pendingBank = -1;
    bool m_writing = false;
    bool m_edited = false;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
