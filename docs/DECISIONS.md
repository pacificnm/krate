# Decisions

Decisions taken after the [project brief](PROJECT_BRIEF.md). Where a decision narrows or changes the brief, the "Brief said" column records it. Newest first.

| # | Date | Decision | Brief said | Why |
| --- | --- | --- | --- | --- |
| D11 | 2026-10-27 | Panel redesigned as a silver hardware-style layout, design size 1600 × 692, with a 16-step LED row above the note keys; full control list in [UI.md](UI.md#layout) | Cream chassis, orange strip, 1600 × 600 | User's choice of look. All names, logos and artwork are our own |
| D10 | 2026-10-27 | Tests use Qt Test plus an offline render tool (`k303-render`) that writes WAV files | — | No extra dependency; DSP can be checked by ear and by test without JACK running |
| D9 | 2026-10-27 | Patterns persist to `~/.local/share/k303/patterns.json`; 8 patterns in v1 | Not covered | Patterns are user data, not config; 8 is enough for v1, format is versioned |
| D8 | 2026-10-27 | Step gate is an enum `Note / Tie / Rest` | "gate/tie (bool)" | Same information as two bools, but rules out the invalid "tie + no gate" combination; matches the classic note/tie/rest step entry |
| D7 | 2026-10-27 | C++ types exposed to QML with `QML_ELEMENT` / `QML_SINGLETON` and `qt_add_qml_module` | `qmlRegisterType` or context properties | Modern Qt 6 approach; compile-time registration, qmllint/qmlsc can see the types |
| D6 | 2026-10-27 | MIDI mappings stored as hand-written JSON via `QJsonDocument` at `~/.config/k303/midimap.json` | `QSettings` or manual JSON | Human-readable, diffable, versioned schema |
| D5 | 2026-10-27 | DSP core (`k303_dsp`) is plain C++20 with no Qt or JACK dependency | Implied | Lets LV2 wrap the same core later and lets tests run offline |
| D4 | 2026-10-27 | Target Qt 6.8 (Debian 13 ships 6.8.2) | "Qt6" | Matches the dev machine; 6.8 is an LTS |
| D3 | 2026-10-27 | Binary / config name is `k303` | `<app>` | User choice |
| D2 | 2026-10-27 | Standalone app first; LV2 is a later phase | LV2 "packaging option" | User choice; core is kept host-agnostic so LV2 stays cheap |
| D1 | 2026-10-27 | Audio and MIDI both through the JACK API, run on PipeWire via `pipewire-jack` | Audio: PipeWire or JACK. MIDI: RtMidi or ALSA sequencer on its own thread with a lock-free queue | User choice. One API; MIDI events arrive timestamped inside the audio callback, so they are sample-accurate and need no MIDI thread or queue. Works on real JACK too |

## Open questions

- Exact LX49 mk3 mode name and default CC numbers: confirm on the hardware with `aseqdump` (see [MIDI.md](MIDI.md)).
- DSP constants marked "starting value" in [DSP.md](DSP.md) are to be tuned by ear against reference recordings.
