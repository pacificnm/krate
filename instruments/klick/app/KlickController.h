#pragma once

#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

// QML singleton "Klick": Klick's knobs (krate::ParamController by key).
class KlickController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Klick)
    QML_SINGLETON

public:
    // Not default-constructible on purpose (see k303's SynthController).
    KlickController(krate::IInstrument &instrument, const QString &panelFilePath, QObject *parent);
    ~KlickController() override;

    static KlickController *create(QQmlEngine *, QJSEngine *);

private:
    static inline KlickController *s_instance = nullptr;
};
