# Krunch: presets

A preset is the whole amp: every knob of the four channels and the power amp (and which channel is playing), each channel's cabinet IRs, and the pedalboard (its pedals in order, each one's place, footswitch and knobs). It's the same snapshot a song keeps when you press SAVE on the Song screen (`Instance::saveSound()`).

On the amp's grille:

- **The name** (green; `*` once anything has changed since it was loaded or saved). A click opens the list: click a preset to load it; **RENAME**; **DELETE** (press twice).
- **◂ ▸**: the previous and next preset, round the list. Both are learnable (`presetPrevious`, `presetNext`), for a controller's buttons or a foot controller.
- **SAVE**: over the current preset (orange while it's modified); with none, it asks for a name.
- **SAVE AS…**: a new preset (a name already taken is replaced, and it says so).

Presets are kept in `~/.local/share/krunch/presets.json` (`version`, `current`, `presets`: `{name, sound}` in the list's order); the current one is remembered. Loading a preset whose pedal type or IR this Krunch doesn't have loads the rest and counts what it got as unmodified.

`app/PresetController.cpp` (QML `Presets`); the amp's controllers (knobs, cabinets, pedals) tell it when anything changes, and it compares the amp with the preset a moment after the last change. Tests: `tests/tst_presets.cpp` (save, load, step; modified and back; what it can't load; rename, remove; kept across runs).
