#pragma once

#include <QJsonArray>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "dsp/Pedal.h"

class QQmlEngine;
class QJSEngine;

namespace krunch {
class Krunch;
}

// QML singleton "Pedals": the pedalboard (docs/PEDALS.md): add, remove and
// reorder pedals, put each in front of the amp or in its effects loop, their
// knobs and footswitches. Kept in pedals.json, and in songs' sounds.
class PedalController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Pedals)
    QML_SINGLETON

    // Bumped when anything on the board changes (a knob, a footswitch)...
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // ...and when its layout does (pedals added, removed or moved): what the board's views rebuild on.
    Q_PROPERTY(int layoutRevision READ layoutRevision NOTIFY layoutChanged)
    Q_PROPERTY(int count READ count NOTIFY layoutChanged)
    Q_PROPERTY(int maxPedals READ maxPedals CONSTANT)
    // Every kind of pedal, for the add menu: [{type, name, colour}].
    Q_PROPERTY(QVariantList types READ types CONSTANT)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    PedalController(krunch::Krunch &engine, const QString &filePath, QObject *parent);
    ~PedalController() override;

    static PedalController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    int layoutRevision() const { return m_layoutRevision; }
    int count() const { return m_board.count(); }
    int maxPedals() const { return krunch::PedalList::kMaxPedals; }
    QVariantList types() const;

    // The board, in order: [{type, name, colour, on, loop, knobs: [{key, label, value, defaultValue}]}].
    Q_INVOKABLE QVariantList pedals() const;
    Q_INVOKABLE bool isOn(int position) const;
    // In the amp's effects loop, or in front of the amp.
    Q_INVOKABLE bool inLoop(int position) const;
    Q_INVOKABLE void setInLoop(int position, bool loop);
    Q_INVOKABLE qreal knob(int position, int knob) const;
    Q_INVOKABLE QString knobText(int position, int knob, qreal value) const;

    Q_INVOKABLE void add(const QString &type);
    Q_INVOKABLE void remove(int position); // fades it out first
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void setOn(int position, bool on);
    Q_INVOKABLE void toggle(int position); // a footswitch (learnable as "pedal1".."pedal8")
    Q_INVOKABLE void setKnob(int position, int knob, qreal value);

    // For songs' sounds and pedals.json: [{type, on, loop, knobs: {key: value}}].
    QJsonArray save() const;
    void load(const QJsonArray &pedals);

signals:
    void changed();
    void layoutChanged();

private:
    bool valid(int position) const { return position >= 0 && position < m_board.count(); }
    void bump();
    void bumpLayout();
    void saveFile();

    static inline PedalController *s_instance = nullptr;

    krunch::Krunch &m_engine;
    krunch::Pedalboard m_board;
    QString m_filePath;
    int m_revision = 0;
    int m_layoutRevision = 0;
    QTimer m_collectTimer;
    QTimer m_saveTimer;
};
