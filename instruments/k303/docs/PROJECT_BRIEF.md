# k303 — Qt/QML Project Brief

2026-10-27

Source: https://claude.ai/artifact/LaAN7BW2HQQeyTFBxxYhxQ

## Overview

Build a standalone acid-bass synthesizer for Linux/KDE, written in Qt6/QML, inspired by classic 1980s analog bass synths, with a hardware-style panel and:

- A built-in 16-step sequencer (note, octave, slide, accent, gate/tie per step)
- MIDI Learn so any knob or control can be mapped to a hardware controller (target controller: Nektar Impact LX49 mk3)
- A software-modeled diode ladder filter aiming for the characteristic acid "squelch"

Note: use only original names, logos and artwork. Do not reproduce any manufacturer's trademarks, model names or panel graphics.

## UI / Panel Design (QML)

Hardware look: ivory/cream chassis, burnt-orange control strip (cutoff, resonance, env mod, decay, accent, tuning), black cylindrical knobs with a white indicator tick, a row of small red LED step buttons, plus up/down/write/pattern mode buttons.

**Components:**

| Component | Purpose |
| --- | --- |
| `Knob.qml` | Custom rotary control, drawn with `Shape`/`Canvas`, vertical-drag to rotate, emits a 0–1 value bound to a synth parameter |
| `StepLed.qml` | Circular LED button; lit/unlit/blinking states (blink = sequencer playhead — animate via opacity pulse, not geometry, for GPU-friendly rendering) |
| `PanelSwitch.qml` | Mode/function toggle buttons |
| `Main.qml` | Fixed-design-resolution `Item` (e.g. 1600×600, matching the hardware's aspect ratio) wrapped in a `Scale` transform so the panel scales cleanly to any window size without stretching or reflowing like a normal desktop app |

Style base: Qt Quick Controls **Basic** style — since every control is fully re-skinned anyway, there's no benefit to fighting Fusion/Material defaults.

QML stays a thin view layer: every knob/button binds to a C++ `QObject`-derived model exposed via `qmlRegisterType` or context properties. No audio logic lives in QML.

## Sequencer

Sample-accurate, not QML/`QTimer`-driven — UI-thread timers drift and jitter audibly.

- Step advance happens inside the audio engine, by **sample count**, in the PipeWire/JACK process callback (BPM → samples-per-step, recomputed once per tempo change)
- Each step holds: note, octave, slide (bool), accent (bool), gate/tie (bool) — the classic acid-bass step data
- QML only **displays** the current step (via a `Q_PROPERTY` the engine updates once per audio block) and **edits** step data through a thread-safe path (lock-free queue or atomic struct swap) — QML never owns timing
- Internal clock by default; optional external MIDI clock sync for locking to a DAW or drum machine later

## MIDI Input & Learn (Nektar Impact LX49 mk3)

Gotcha to check first: the LX49 mk3 has a DAW Integration mode (NIntegrate) that sends proprietary SysEx instead of plain CCs when paired with supported DAWs — keep it in generic/Standard mode so knobs send plain CC messages.

- `MidiInput` class (RtMidi or raw ALSA sequencer API) runs on its own thread, decodes incoming CC/note messages, pushes them into a lock-free queue the audio thread drains each block
- **MIDI Learn** is a UI-layer mode: click a knob → app enters "listening" → next CC received binds to that parameter → mapping stored as `{ccNumber: parameterId}`
- Mappings persist to `~/.config/<app>/midimap.json` (via `QSettings` or manual JSON), reloaded on startup
- Nice-to-have: step-record mode — tap a step pad on the LED row, then play a note on the LX49 to set that step's pitch

## Audio Engine & DSP

- **Oscillator:** single saw or square — straightforward
- **Filter (the hard part):** a diode ladder lowpass is the source of the squelchy, resonant acid character. Model it with a zero-delay-feedback (ZDF) nonlinear filter, plus 4x–8x oversampling to tame aliasing from the nonlinear saturation stages. Builds on established virtual-analog filter literature (Stilson/Smith-style analog filter analysis); several hobbyist and commercial implementations have nailed this convincingly
- **Envelope / accent circuit:** well documented in published analog-synth circuit analyses
- **Audio I/O:** PipeWire or JACK (bypass Qt Multimedia — too high-latency/abstracted for this)
- The DSP engine runs in its own realtime audio thread, fully separate from the Qt event loop
- Packaging option: ship as an LV2 plugin (Linux's native plugin format) so it slots into Ardour, Carla, etc. — LV2 UIs can themselves be Qt-based

## Project Structure / Tech Stack Summary

| Layer | Choice |
| --- | --- |
| UI | Qt6 QML (Basic style base), custom `Knob.qml` / `StepLed.qml` / `PanelSwitch.qml` |
| Audio I/O | PipeWire or JACK, realtime process callback |
| MIDI | ALSA sequencer API or RtMidi |
| DSP core | C++, ZDF diode-ladder filter model, own realtime thread |
| Config/mappings | JSON under `~/.config/<app>/`, via `QSettings` or manual parsing |
| Build | CMake + Qt6 |
| Optional packaging | LV2 plugin format for DAW/host integration |

**Suggested starting skeleton:** `CMakeLists.txt`, a Qt6 app shell, `Knob.qml`, a fixed-aspect `Main.qml` panel, a C++ `AudioEngine` stub wired to PipeWire, and a basic `MidiInput`/MIDI-learn class.
