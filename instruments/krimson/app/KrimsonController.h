#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class Transport;
}
namespace krimson {
class Engine;
}

// QML singleton "Krimson": the edited slot's controls (krate::ParamController
// by key), which slot is edited, plus Tempo and run/stop. Standalone, those
// drive the transport it is given; in the Krate rack it is given none.
class KrimsonController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Krimson)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    // The slot the panel edits: 0..3 = A..D.
    Q_PROPERTY(int editedSlot READ editedSlot WRITE setEditedSlot NOTIFY editedSlotChanged)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    KrimsonController(krimson::Engine &engine, const QString &panelFilePath, krate::Transport *transport,
                      QObject *parent);
    ~KrimsonController() override;

    static KrimsonController *create(QQmlEngine *, QJSEngine *);

    bool ownsTransport() const { return m_transport != nullptr; }
    bool playing() const;
    void setPlaying(bool playing);
    int editedSlot() const;
    void setEditedSlot(int slot);

signals:
    void playingChanged();
    void editedSlotChanged();

private:
    void syncTempo();
    void poll();

    static inline KrimsonController *s_instance = nullptr;

    krimson::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
    QTimer m_pollTimer;
    bool m_playing = false;
};
