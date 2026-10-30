# Klang manual

Klang is a sampler: 16 pads, each playing a sound file of your own (WAV or MP3) or a slice of one, as a hit, a held note or a loop locked to the tempo, through its own filter, drive, bitcrusher and ring modulator, chopped by a trance gate, sent to a delay and a reverb, and played by patterns of up to 64 steps with accents, rolls, reverse and pitch per step. It is made for cut-up, mechanical, gated music: drum breaks chopped and re-sequenced, voices reversed and crushed, drones chopped in time, metal hits ringing through delay.

This manual goes through the panel: the pads, the edited pad's controls, the patterns, Klang's own knobs. Drag a knob (or use the mouse wheel over it) to change it; its value shows under it; double-click to put it back to its default.

## Quick start

1. **Hear the starter kit.** The first time, Klang puts 16 sounds it makes itself on the pads (drums, metal, noise, a break, a drone, two voices, a stab...) and two patterns. Press **START** (Space): pattern **A** plays. Click pattern **B**: it starts at the end of the bar.
2. **Play the pads.** Click them, or play notes 36–51 (C2–D#3) from your keyboard or pad controller. Clicking a pad also picks it for editing: its waveform and controls show on the right.
3. **Load your own sounds.** Drag WAV or MP3 files from your file manager onto a pad (several files fill the pads after it), or click a pad and press **LOAD**. They are copied into Klang's library, so they keep working if you move the originals.
4. **Mangle.** On the edited pad, try **REVERSE**, **BITS**, **RATE**, **DRIVE**, **RING**, a **FILTER** with **RES** up, **GATE** on.
5. **Chop a loop.** Load a drum loop, set **MODE** to Loop and **SYNC** to its length in beats (4 for a bar), then press **SLICE 16** or **HITS**: its pieces land on the pads, ready for a new pattern.
6. **Write a pattern.** Pick a pattern (A–P), click steps in the grid (a row per pad), then use the tools (**ACCENT**, **ROLL**, **REVERSE**, **PITCH**) on them.

Everything is kept when you quit: the pads in `pads.json`, the patterns in `patterns.json`, the samples in `~/.local/share/klang/samples`.

## How a pad makes sound

```
sample region ─► pitch / speed ─► envelope ─► FILTER ─► DRIVE ─► BITS · RATE ─► RING ─► GATE ─► LEVEL · PAN ─┬─► out
 (START…END,      (TUNE, FINE,     (ATTACK,                                                                   ├─► DELAY send
  REVERSE)         SYNC)            DECAY)                                                                     └─► REVERB send
```

Each pad plays one hit at a time: hitting it again fades the last hit out in 3 ms while the new one starts.

## Pads

| Control | What it does |
| --- | --- |
| **The 16 pads** | Click to play and edit; let go to stop a Gate or Loop pad. Pad 1 is at the bottom left, as on a pad controller. A pad lights while it sounds; a dot shows it has steps in the pattern shown. Drop files on a pad to load them there. |
| **STARTER KIT** | Puts the starter kit back on the pads (Klang's own sounds; also in the library). |
| **LOAD** | Opens a file dialog: WAV or MP3 files onto the edited pad and the pads after it. |
| **◂ ▸** | The previous or next sound in the library onto the edited pad. |
| **CLEAR** | No sound on the edited pad. |
| **SLICE 4 / 8 / 16** | Cuts the edited pad's region into that many equal slices, onto it and the pads after it (as far as pad 16). Each slice keeps the pad's settings, as a one shot. Best with a loop of a whole number of bars. |
| **HITS** | Cuts the region where its hits are (drum hits, notes starting), up to 16 slices. |

## The edited pad: sample

| Control | What it does |
| --- | --- |
| **LEVEL** | The pad's level. |
| **PAN** | Left to right. |
| **TUNE** | Pitch in semitones, −24 … +24. Faster is higher, as on the old samplers. |
| **FINE** | ±50 cents. |
| **START** | Where in the sample the pad starts (the waveform's lit part). |
| **END** | Where it ends. |
| **ATTACK** | A fade-in, 0–2 s: from a hard hit to a swell. |
| **DECAY** | How long the hit takes to die away, 10 ms – 10 s; at the top, **Full**: the whole region at full level. Short decays turn a loop into a gated stab. |
| **MODE** | **One shot**: plays to its end or decay, whatever the key does. **Gate**: plays while the key (or the step) is held. **Loop**: loops the region until you let go, hit it again, a choke cuts it, or the transport stops. |
| **SYNC** | Fits the region to **1, 2, 4, 8 or 16 beats** of the tempo, playing it faster or slower (and so higher or lower). A one-bar loop on 4 stays in time at any tempo. **Off**: the sample's own speed. |
| **REVERSE** | Plays the region backwards. |
| **CHOKE** | **Group 1–4**: a pad cuts the other pads of its group (a closed hi-hat cutting the open one; the starter kit's hats are in group 1). **None**: it doesn't. |

## The edited pad: tone

| Control | What it does |
| --- | --- |
| **FILTER** | **Off**, **Low-pass** (darker), **High-pass** (thinner), **Band-pass** (a narrow band: telephone, radio). |
| **CUTOFF** | The filter's frequency, 20 Hz – 20 kHz. |
| **RES** | Resonance: a peak at the cutoff; high, it rings and whistles. |
| **ENV** | Opens the filter at each hit, up to 6 octaves above the cutoff, closing as the hit decays: a pluck or a zap. |
| **DRIVE** | Saturation: warm, then hard clipping, and above half way the wave folds over itself (metallic, harsh). |
| **BITS** | Bit depth: **Off**, then 16 down to 2 bits. Lower is grittier; 4–6 bits is the classic crunch, 2 is a buzzing square. |
| **RATE** | Sample-rate reduction: **Full**, then 1/2 … 1/32. Lowers the fidelity, adds aliasing whine: old samplers, broken machines. |
| **RING** | Ring modulator mix, 0–100 %: multiplies the sound by a sine, for bells, robots and clanging metal. |
| **FREQ** | The ring modulator's frequency, 20 Hz – 4 kHz: low makes a tremolo-like warble, high a metallic clang. |
| **GATE** | **On**: the trance gate (below the step grid) chops this pad in time. |
| **DELAY** | How much of the pad goes to the delay. |
| **REVERB** | How much of the pad goes to the reverb. |

## Patterns

16 patterns, **A**–**P**, each up to 64 steps of 1/16 note, a row of steps for every pad.

| Control | What it does |
| --- | --- |
| **A … P** | Pick a pattern to see and edit; it also plays next: at once when stopped, from the end of the bar when playing. Lit: the one shown; dark orange: the one playing; grey: has steps. |
| **START** | Starts and stops the transport (Space does the same) standalone; in the Krate rack, Klang's own RUN on the rack's transport. |
| **LENGTH − +** | The pattern's length, 1–64 steps. |
| **1–16 … 49–64** | The page of 16 steps the grid shows. |
| **STEP** | Tool: a click switches a step on or off (off also clears its accent, roll, reverse and pitch). |
| **ACCENT** | Tool: a click accents a step (louder; lit brighter). |
| **ROLL** | Tool: a click cycles the step's hits: 1, 2, 3, 4 (×2 … ×4 on the step): rolls and stutters. |
| **REVERSE** | Tool: a click makes that hit play backwards (◂ on the step). |
| **PITCH + / PITCH −** | Tool: a click pitches the hit up or down a semitone (−12 … +12, shown on the step). |
| **COPY / PASTE** | Copy the pattern, pick another, paste it there. |
| **CLEAR** | Empties the pattern. |
| **GATE** (the row under the grid) | The trance gate's 16 steps for this pattern: lit steps let the GATE pads through, dark ones silence them. Each pattern has its own. |

The grid's rows are the pads, 16 at the top and 1 at the bottom; click a row's number to edit that pad. While playing, a frame shows the step playing.

## Klang's own knobs

| Control | What it does |
| --- | --- |
| **SWING** | Moves every second sixteenth later, 50 % (straight) to 75 % (a hard shuffle). |
| **KEYS** | **Pads**: notes 36–51 play pads 1–16, and other keys play the edited pad at their pitch. **Chromatic**: every key plays the edited pad at its pitch (C4 is the sample's own pitch): melodies from one sound. |
| **VOLUME** | Klang's output level. |
| **GATE RATE** | The trance gate's steps: **1/8**, **1/16**, **1/32**. |
| **DEPTH** | How far the gate closes: 100 % silences, less lets some through. |
| **SMOOTH** | The gate's edges, 1–30 ms: sharp chops to soft pulses. |
| **DELAY** | The delay's level (what the pads send comes back at this level). The echoes bounce left and right. |
| **TIME** | The delay time, tempo-synced: **1/16**, **1/8T**, **1/8**, **1/8.**, **1/4**, **1/4.**, **1/2**. |
| **FEEDBACK** | How many repeats, 0–90 %. |
| **REVERB** | The reverb's level. |
| **SIZE** | The reverb's decay, 0.4 s to 8 s. |
| **TEMPO** | Standalone only: the tempo (40–240 BPM). In the Krate rack, the rack's transport sets it. |

## MIDI and MIDI Learn

| Control | What it does |
| --- | --- |
| **LEARN** | MIDI Learn (also the L key; Esc leaves it): click a knob, a pad or a button, then move a knob or press a pad or button on your controller. A learned knob moves the pad being edited. |
| **UNMAP** | Removes every MIDI link. |
| **AUDIO** | Lit when Klang is connected to the audio system; click to reconnect. The line next to it shows the sample rate, buffer and dropouts, and the meter the output level. |
| **HELP** | Opens this manual, with a search. |

- **Notes**: 36–51 (C2–D#3) play the pads, with velocity; see KEYS for the rest. In the Krate rack, Klang's channel is 11, and your keyboard plays it whenever it is the instrument in focus.
- **Program Change** n (0–15) plays pattern n next (A = 0).
- **Learnable buttons**: the 16 pads (play and edit), the 16 patterns, START, SLICE (8 slices of the edited pad), and next / previous pad.
- Keys on the computer: Space start / stop, ← → the edited pad, L learn.

## Sound design recipes

- **Chopped break**: load a one-bar drum loop, SLICE 16, then write a new pattern from the slices; ROLL a few steps ×3, REVERSE one, PITCH a snare slice down 5.
- **Gated drone**: a long pad or drone, MODE Loop, SYNC to its bars, GATE on, GATE RATE 1/16, edit the GATE row; FILTER Low-pass with RES up.
- **Industrial metal**: a clank or a pipe, RING 50 %, FREQ high, DRIVE past half, DELAY on 1/8.; pitch the steps up and down.
- **Broken voice**: a vocal phrase, REVERSE, BITS 6, RATE 1/8, REVERB up; or Chromatic KEYS and play it as a choir of robots.
- **Trance stab**: a short chord, GATE on with a busy gate pattern, DELAY on 1/8., REVERB up.
- **Kick and sub**: the kick with DECAY short, the boom with DECAY long on the same steps, CHOKE them apart if they fight.

## Troubleshooting

- **A pad is silent**: does it have a sound (its name on the pad)? Is END before START? Is LEVEL down, or FILTER closed (low CUTOFF on Low-pass)? With GATE on and the gate row dark, it's chopped silent.
- **A loop drifts out of time**: set SYNC to its length in beats; without SYNC it plays at its own speed.
- **The pattern doesn't play**: press START; in the rack, Klang's RUN must be on and the rack's transport playing.
- **The pattern changes late**: a pattern picked while playing starts at the end of the bar (by design).
- **An MP3 won't load**: Klang reads MP3 and WAV (16/24/32-bit, float); the message under the pads says why a file was refused. Files longer than 2 minutes are cut at 2 minutes.
- **Too loud with many pads**: lower the pads' LEVELs or VOLUME; the reverb and delay add up too.
