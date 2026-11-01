#include "app/KlickController.h"

#include <QJSEngine>

KlickController::KlickController(krate::IInstrument &instrument, const QString &panelFilePath, QObject *parent)
    : krate::ParamController(instrument, panelFilePath, parent)
{
}

KlickController::~KlickController()
{
}

