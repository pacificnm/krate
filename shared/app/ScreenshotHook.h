#pragma once

class QQmlApplicationEngine;

namespace krate {

// Dev hook (instruments/korrode/docs/BUILD.md): with KRATE_SCREENSHOT=<file.png>
// set, saves the first window after KRATE_SCREENSHOT_DELAY_MS (default 500) and
// quits. Does nothing when the variable is unset.
void installScreenshotHook(QQmlApplicationEngine &engine);

} // namespace krate
