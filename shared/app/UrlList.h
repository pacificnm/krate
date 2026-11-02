#pragma once

#include <QStringList>
#include <QVariantList>

namespace krate {

// Local paths from what QML hands a C++ function as files: a file dialog's
// selectedFiles (each url wrapped in a QVariant of its own), [folder], a
// drop's urls, plain paths. Anything that isn't a file:// url or a path is skipped.
QStringList localPaths(const QVariantList &urls);

} // namespace krate
