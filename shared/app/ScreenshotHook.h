#pragma once

class QQmlApplicationEngine;

namespace krate {

// Dev hook (instruments/k303/docs/BUILD.md): with K303_SCREENSHOT=<file.png>
// set, saves the first window after K303_SCREENSHOT_DELAY_MS (default 500) and
// quits. Does nothing when the variable is unset.
void installScreenshotHook(QQmlApplicationEngine &engine);

} // namespace krate
