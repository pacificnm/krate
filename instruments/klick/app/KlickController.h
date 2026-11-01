#pragma once

#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

// QML type "Klick" (one per instrument, given to its panel): Klick's knobs (krate::ParamController by key).
class KlickController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Klick)
    QML_UNCREATABLE("one per instrument, given to its panel")

public:
    // Not default-constructible on purpose (see Korrode's SynthController).
    KlickController(krate::IInstrument &instrument, const QString &panelFilePath, QObject *parent);
    ~KlickController() override;


private:
};
