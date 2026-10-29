#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

namespace krunch {
class Krunch;
}

// QML singleton "Krunch": the amp's knobs (krate::ParamController by key) and
// its meters, polled at 60 Hz.
class KrunchController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Krunch)
    QML_SINGLETON

    // 0..1 over -60..0 dB, with a steady fall: the input as it arrives, the output.
    Q_PROPERTY(qreal inputLevel READ inputLevel NOTIFY metersChanged)
    Q_PROPERTY(qreal outputLevel READ outputLevel NOTIFY metersChanged)
    // The input reached full scale in the last second: turn the interface's gain down.
    Q_PROPERTY(bool clipping READ clipping NOTIFY metersChanged)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    KrunchController(krunch::Krunch &engine, const QString &panelFilePath, QObject *parent);
    ~KrunchController() override;

    static KrunchController *create(QQmlEngine *, QJSEngine *);

    qreal inputLevel() const { return m_inputLevel; }
    qreal outputLevel() const { return m_outputLevel; }
    bool clipping() const { return m_clipFrames > 0; }

signals:
    void metersChanged();

private:
    void pollMeters();

    static inline KrunchController *s_instance = nullptr;

    krunch::Krunch &m_engine;
    QTimer m_meterTimer;
    qreal m_inputLevel = 0.0;
    qreal m_outputLevel = 0.0;
    int m_clipFrames = 0; // polls left showing the clip light
};
