#include "app/KlickController.h"

#include <QJSEngine>

KlickController::KlickController(krate::IInstrument &instrument, const QString &panelFilePath, QObject *parent)
    : krate::ParamController(instrument, panelFilePath, parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
}

KlickController::~KlickController()
{
    s_instance = nullptr;
}

KlickController *KlickController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}
