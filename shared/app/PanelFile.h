#pragma once

#include <span>
#include <vector>

#include <QString>

#include "app/JsonFile.h"
#include "core/ParamDescriptor.h"

namespace krate::files {

// Every knob and switch position of an instrument (panel.json), normalized
// 0..1, one value per descriptor, stored by parameter key.
inline constexpr int kPanelFileVersion = 1;

using PanelValues = std::vector<float>;

PanelValues defaultPanel(std::span<const ParamDescriptor> params);
// On anything but Loaded, *values is set to defaultPanel(). Unknown keys are
// ignored; missing ones keep their default; values are clamped to 0..1.
LoadResult loadPanel(const QString &path, std::span<const ParamDescriptor> params, PanelValues *values);
bool savePanel(const QString &path, std::span<const ParamDescriptor> params, const PanelValues &values);

} // namespace krate::files
