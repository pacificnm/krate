# Krunch: pedals

The pedalboard sits under the amp: the guitar goes through its pedals left to right, then into the amp (`frontPedals()`, before the gate). Up to eight pedals.

- **Add:** the **+** tile at the end of the board, then the pedal from the menu.
- **Reorder:** drag a pedal by its body; it drops into the nearest place.
- **Remove:** the **×** at a pedal's top right (it switches off and fades out first).
- **On / off:** the footswitch; the light shows it's on. Switching crossfades over 10 ms, so it never clicks; an added pedal fades in.
- **MIDI:** the footswitches are learnable by their place on the board, `pedal1` … `pedal8` (MIDI Learn, then press a button or a pad on the controller).

The board is kept in `~/.config/krunch/pedals.json` (each pedal's type, footswitch and knobs, in order) and, like the rest of the amp, in a song's sounds when you press SAVE on the Song screen.

## How it's built

`dsp/Pedal.h`: a `Pedal` has its kind (`PedalInfo`: type, name, colour, knobs), its knobs and footswitch as atomics, and the bypass crossfade; each kind of pedal only writes its effect. `PedalChain` is what the audio thread plays: a list of pedals handed over through an atomic pointer. `Pedalboard` owns the pedals on the GUI side: every add, remove or move builds a new list and hands it over, and a replaced list or a removed pedal is freed once the audio thread has moved two blocks on (`Krunch::blocksProcessed()`), so changing the board never stops or glitches the sound. `app/PedalController.cpp` (QML `Pedals`) is the panel's view of it.

## Green Drive

The classic green mid-boost overdrive, modelled on its circuit in broad strokes (`dsp/GreenDrive.cpp`):

- **The clipping stage:** an op-amp whose gain is 1 + (51 k + Drive) / 4.7 k, where the 4.7 k leg has a capacitor that leaves frequencies below about 720 Hz at unity: only the mids and highs get the gain. That extra part is clipped softly by the diodes in the feedback loop and added to the clean signal, so the bass stays tight and the mids push. The feedback capacitor rolls the treble off, lower as Drive goes up (from about 60 kHz down to about 5.6 kHz). Drive is an audio-taper 500 k pot. The stage runs at 4×.
- **Tone:** a fixed 723 Hz low-pass, with the Tone control bringing back what it took: dark at 0, about −6 dB of treble at noon, a little bright at 10.
- **Level:** −24 … +12 dB.
- Small-signal response at noon: about +3 dB at 80 Hz, +17 dB at 1 kHz, +16 dB at 5 kHz (the hump); at full drive the mids get about 30 dB.

| Knob | Range | Default |
| --- | --- | --- |
| `drive` | 0 … 10 | 5 |
| `tone` | 0 … 10 | 5 |
| `level` | −24 … +12 dB | −6 dB |

Tests: `tests/tst_pedals.cpp` (bypass untouched, no clicks, the mid hump, drive, tone, level, bounded clipping, aliasing, the board's order).
