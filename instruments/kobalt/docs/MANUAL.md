# Kobalt manual

Kobalt is a monophonic synthesizer: one note at a time, made by one oscillator with a saw, a pulse, a sub-oscillator and noise mixed together, a resonant four-pole low-pass filter that can sing on its own, one envelope, an LFO, portamento, an arpeggiator, a 64-step sequencer, delay and reverb, and 64 patches. It is built for basses, leads and sequenced lines, the way the small one-oscillator synths of the early 80s were played.

This manual goes through the panel from left to right and top to bottom. Every slider, knob, switch and button is described here: what it does, its range, and how to use it. Drag a slider or knob (or use the mouse wheel over it) to change it; its value shows while you drag; double-click to put it back to its default. Click a switch to step through its positions.

## Quick start

1. **Pick a sound.** The first time, Kobalt starts in Manual (the display shows **--**). Click **BANK 1** then **PATCH 1**: the display shows **11**, Sub Bass. Step through all 64 with **▲ ▼** next to the display. Banks 1 and 2 hold the factory sounds.
2. **Play.** Use your MIDI keyboard or click the on-screen keys. Kobalt plays one note at a time: hold a key and press another, and the new one sounds; let it go and the first comes back.
3. **Change it.** Move **FREQ** and **RES** in the VCF section first: they change the sound the most. Then the source mixer's **SAW**, **PULSE** and **SUB** sliders, and the envelope's **A**, **D**, **S**, **R**.
4. **Glide.** Set PORTAMENTO **MODE** to Auto and play legato (press the next key before letting go of the last): the pitch slides between them.
5. **Keep it.** Press **WRITE**, then the **PATCH** button (and first a **BANK**, if you want another bank) to store into.
6. **Make a line.** Press **SEQ**, click step 1 on the grid and play notes one at a time; press PLAY (or **START**) and transpose it from the keys.

The panel as you leave it is kept when you quit; patches only change when you WRITE.

## How Kobalt makes sound

```
           ┌ PULSE ┐
oscillator ┼ SAW   ┼─ source mixer ─► VCF ─► VCA ─► delay ─► reverb ─► VOLUME
           ├ SUB   ┤
   noise ──┴ NOISE ┘
```

- The **oscillator** makes a saw and a pulse at the note's pitch, and a sub-oscillator one or two octaves below, all from the same cycle so they stay locked together. **Noise** is a hiss with no pitch.
- The **source mixer** sets how much of each goes on.
- The **VCF** (low-pass filter) shapes it from dark to bright; resonance makes it whistle, squelch or ring.
- The **VCA** sets the level over time, from the envelope or just from the key.
- The **envelope**, the **LFO**, the bender and the lever move things over time.
- After the voice: **delay** (it makes Kobalt stereo), **reverb**, then **VOLUME**.

## Modulator

The LFO (low-frequency oscillator): a slow wave that wobbles the pitch, the pulse width or the filter, as the MOD controls in those sections ask, and that can fire the envelope.

| Control | What it does |
| --- | --- |
| **RATE** | The LFO's speed, 0.1–30 Hz: from a slow sweep to a fast flutter. |
| **WAVE** | **Triangle** (smooth up and down: vibrato, sweeps), **Square** (jumps between two values: trills), **Random** (a new random value every cycle: the burbling "sample and hold" sound), **Noise** (a new random value every moment: grit and growl on the filter). |

## VCO

The oscillator.

| Control | What it does |
| --- | --- |
| **MOD** | The LFO on pitch, from nothing to ±1 octave. The bottom of the slider is fine vibrato (it's squared: the first half goes to a quarter of the way); the top is a siren. The mod wheel adds vibrato too (see LEVER). |
| **RANGE** | The octave: **16'** (one octave down), **8'** (as played), **4'** (one up), **2'** (two up). The footages are the organ-stop names for octaves. |
| **PULSE WIDTH** | How far the pulse narrows from a square (50 %) towards a thin pulse (5 %): hollow, then nasal, then reedy. With PWM SOURCE on Manual it sets the width; on LFO or Env it's how far they move it. |
| **PWM SOURCE** | What moves the pulse width: **LFO** (the slow, swirling "PWM" sound, as if several oscillators played), **Manual** (PULSE WIDTH sets a fixed width), **Env** (the width follows the envelope: the tone changes as each note dies away). |

## Source mixer

| Control | What it does |
| --- | --- |
| **PULSE** | The pulse's level (its width from PULSE WIDTH). |
| **SAW** | The saw's level: bright and buzzy, the sound of most basses and leads. |
| **SUB** | The sub-oscillator's level: weight under the note. |
| **SUB TYPE** | **1 Oct** (a square one octave down), **2 Oct** (a square two octaves down: deep, for basses), **2 Oct Pulse** (a narrow pulse two octaves down: a thinner, buzzier low end). |
| **NOISE** | White noise: hiss, wind, snares and hats (with the filter and a short envelope). |

All four up at once push the filter's input hard: the sound thickens and saturates, as on the original circuits. Bring them back if the filter starts to sound crushed.

## VCF

The low-pass filter: four stages, 24 dB per octave, each stage soft-limiting as it gets loud.

| Control | What it does |
| --- | --- |
| **FREQ** | The cutoff frequency, 20 Hz–20 kHz: what's below passes, what's above is cut. Low is dark and muffled; high is open. |
| **RES** | Resonance: a peak at the cutoff. A little makes it vocal; a lot makes it squelch and whistle; at the top the filter rings on its own as a sine at the cutoff (try it with KYBD at full: it plays in tune with the keys). Loud resonant notes round off and buzz instead of getting louder. |
| **ENV** | How far the envelope opens the cutoff, up to 8 octaves: the "wow" or "pluck" at the start of each note. Shape it with A, D, S, R. |
| **MOD** | The LFO on the cutoff, up to ±4 octaves: wah, wobble, or growl (with WAVE on Noise). |
| **KYBD** | How much the cutoff follows the keys, 0–100 %: at 100 % every note is equally bright; at 0 high notes get darker. |

## VCA

| Control | What it does |
| --- | --- |
| **MODE** | **Env**: the level follows the envelope. **Gate**: the level is fully on while a key is down and off when you let go, whatever the envelope does (the envelope still moves the filter): organ-like notes with a filter pluck. |

## ENV

One ADSR envelope, for the VCA (in Env mode), the filter (ENV), and the pulse width (PWM SOURCE Env).

| Control | What it does |
| --- | --- |
| **TRIGGER** | When the envelope starts again. **Gate + Trig**: every new key, even one pressed while another is held. **Gate**: only a key pressed after the others were let go; playing legato keeps the envelope going (smooth, connected lines). **LFO**: the LFO restarts it on every cycle while a key is down: rhythmic pulses from one held note, at the LFO's RATE. |
| **A** | Attack: how long the envelope takes to rise, 1 ms–10 s. |
| **D** | Decay: how long it takes to fall to the sustain level, 1 ms–10 s. |
| **S** | Sustain: the level it holds while the key is down, 0–100 %. |
| **R** | Release: how long it takes to die away after the key is let go, 1 ms–10 s. |

## Portamento

| Control | What it does |
| --- | --- |
| **TIME** | How long a glide takes, 0–2 s (the time to get 99 % of the way). |
| **MODE** | **Off**: no glide. **Auto**: glide only between notes played legato (the next key down before the last is let go), so you choose note by note. **On**: every note glides from the one before. |

## Arpeggiator and sequencer

The left of the function deck. **Play mode** chooses what your keys do:

| Control | What it does |
| --- | --- |
| **NOTE** | Keys play notes. |
| **ARP** | Keys feed the **arpeggiator**: hold a chord and it plays its notes one after another, in time. Standalone, ARP starts the clock; in the Krate rack it follows the rack's PLAY. |
| **SEQ** | The **step sequencer**: 8 patterns of up to 64 steps that play while the transport runs; your keys then transpose the pattern (by their distance from middle C) instead of sounding. Stopped, keys play as usual. |
| **HOLD** | Arpeggio notes stay after you let go; the first key of a new chord replaces them. |
| **START** | Starts and stops the transport (Space does the same) standalone; in the rack, Kobalt's own RUN: its sequencer on the rack's transport. |
| **MODE** | Arpeggio order: **Up**, **Up & Down** (without repeating the top and bottom notes), **Down**. |
| **RANGE** | The arpeggio spreads the held notes over 1–3 octaves. |
| **RATE** | Steps per beat for the arpeggiator and the sequencer: **1/8**, **1/16**, **1/8T**, **1/16T** (triplets). Each note lasts half a step. |
| **REST** | While loading: a rest in the current step, and on to the next. |
| **TIE** | While loading: the note before goes on through this step. |
| **LEGATO** | While loading: the step just loaded runs straight into the next note, with no gap (press again to undo). With TRIGGER on Gate the envelope goes on, and with portamento on Auto the pitch glides: the classic sliding bass part. |
| **CLEAR** | Empties the current pattern (16 rests). |
| **GEN** | In SEQ mode: writes a new random pattern into the current one, at its length. The notes come from the keys you hold (hold a few, then press GEN), else from the pattern's own notes, else a C minor pentatonic bass scale; RANGE spreads them over 1–3 octaves. Some notes tie on and some slide legato into the next. Each press makes a different one: keep pressing until one grabs you, then edit it step by step. |
| **DENSITY** | For GEN: how many steps play, 0–100 % (the first step always does); the rest are rests. Low makes sparse, funky lines; high, busy ones. |

**Loading a pattern**: choose **SEQ** with the transport stopped. The BANK and PATCH buttons become the sequencer's 64 steps, in four rows of 16. Click a step (it blinks), then play notes one at a time: each goes into the step and the next blinks. Press a key while still holding the one before, and the one before becomes legato (a small dot on its step). **Shift-click** a step to make it the last one. **▲ ▼** next to the display choose the pattern (**P1**–**P8**). Click the blinking step to stop loading. Patterns belong to the instrument, not to a patch: change the sound while the pattern plays.

On the grid: bright steps are notes, darker ones ties, dark ones rests; the step playing lights white; steps past the pattern's length are dimmed.

## Bender

How far the pitch bend wheel and the mod wheel move things.

| Control | What it does |
| --- | --- |
| **VCO** | The pitch bend wheel's range on the pitch, 0–12 semitones (2 by default). |
| **VCF** | The pitch bend wheel on the cutoff, up to ±4 octaves: bend up to open the filter as well as (or instead of, with VCO at 0) raising the pitch. |
| **LEVER** | How much LFO vibrato the mod wheel brings in, up to ±2 semitones at full (at the LFO's RATE and WAVE). |

## Patches

64 patches: 8 banks × 8, shown as **11**–**88** with the patch's name under the display. A patch keeps everything on the panel except VOLUME, TEMPO, OCTAVE, HOLD and DENSITY.

| Control | What it does |
| --- | --- |
| **The display** | The bank and patch number. A dot after it (**14.**) means you've changed the sound since it was loaded. **--** means Manual. In SEQ mode it shows the pattern (P1–P8). |
| **▲ ▼** | The next or previous patch (wrapping from 88 to 11); in SEQ mode, the pattern. |
| **BANK** 1–8 | Choose a bank: it blinks until you press a PATCH button. |
| **PATCH** 1–8 | Load that patch from the current (or blinking) bank. |
| **WRITE** | Store the panel: press WRITE (it blinks), optionally a BANK, then the PATCH to store into. WRITE again cancels. |
| **MANUAL** | Play the panel as it is, not tied to any patch. |

**Factory patches**: bank 1: 11 Sub Bass, 12 Glide Lead, 13 PWM Lead, 14 Squelch Bass, 15 Pluck, 16 LFO Gate, 17 Noise Hat, 18 Zap Tom; bank 2: 21 Wobble Bass, 22 Siren, 23 Hollow Square, 24 Arp Blip (arpeggio on), 25 Random Bleeps, 26 Gate Organ, 27 Brass Lead, 28 Bend Lead. Banks 3–8 start empty (Init) for your own sounds.

## Effects

After the voice: delay, then reverb.

| Control | What it does |
| --- | --- |
| **DELAY** | Echo level (0 = off). The echoes bounce left, right, left. |
| **TIME** | Tempo-synced: **1/16**, **1/8T**, **1/8**, **1/8.** (dotted), **1/4**, **1/4.**, **1/2**. |
| **FEEDBACK** | How many repeats, 0–90 %; each repeat is a little darker. |
| **REVERB** | Reverb level (0 = off). |
| **SIZE** | The reverb's decay, from 0.4 s (a small room) to 8 s (a huge hall). |

## Playing controls

Next to the keyboard, on the right:

| Control | What it does |
| --- | --- |
| **VELOCITY** | How much how hard you play matters, 0–100 %: on the level and on the filter's ENV. At 0 every note plays the same, as on the original instruments. |
| **ANALOG** | How much Kobalt behaves like an analog circuit (0–100 %, 30 % by default): the oscillator drifts slowly in pitch, and each note sits a little off in tuning and cutoff. At 0 it is perfectly steady. |

The **mod wheel** adds LFO vibrato (see LEVER). The **sustain pedal** holds the note after you let go, until it lifts.

## Volume, tempo, keyboard

| Control | What it does |
| --- | --- |
| **VOLUME** | Kobalt's output level. |
| **TEMPO** | Standalone only: the tempo (40–240 BPM) for the arpeggiator, the sequencer and the delay. In the Krate rack, the rack's transport sets the tempo. |
| **OCTAVE ◂ ▸** | Shifts your MIDI keyboard and the on-screen keyboard by −3 to +3 octaves (Z and X on the computer keyboard). The display shows the octave you're playing in, as on Krimson: 0 is where the keys play as marked; it adds your MIDI keyboard's own octave buttons (as the keys show it) to Kobalt's shift. A song's notes always play as written, whatever OCTAVE is set to. |
| **The on-screen keyboard** | Played with the mouse (press and glide). Keys held from MIDI light up. |

## MIDI and MIDI Learn

| Control | What it does |
| --- | --- |
| **LEARN** | MIDI Learn (also the L key; Esc leaves it): click a slider, knob, switch or button, then move a knob or press a button on your MIDI controller; they're linked. Right-click a control in Learn to remove its link. |
| **UNMAP** | Removes every MIDI link. |
| **AUDIO** | Lit when Kobalt is connected to the audio system; click to reconnect. The line under it shows the sample rate, buffer and dropouts, and the meter the output level. |
| **HELP** | Opens this manual, with a search. |

- **Channel**: standalone, Kobalt plays on every channel; in the Krate rack, on channel 7 by default.
- **Learnable buttons**: NOTE, ARP, SEQ, HOLD, START, GEN, REST, TIE, LEGATO, WRITE, the BANK and PATCH buttons and OCTAVE ◂ ▸, so a controller can switch patches, arpeggios and patterns while you play.
- **Program Change** loads a patch (0 = 11, 1 = 12, … 8 = 21, …). **Pitch bend**, **CC 1** (mod wheel) and **CC 64** (sustain) are understood; CC 120 and 123 stop the note.

## Sound design recipes

Start from an Init patch (an empty one in banks 3–8) or from the nearest factory sound.

- **Deep bass**: RANGE 16'; SAW half, SUB up with SUB TYPE 1 Oct or 2 Oct; FREQ low, RES a little, ENV up with D short and S low; A 0.
- **Sliding bass part**: SAW only, RANGE 16'; RES high, FREQ low, ENV up, D short, S 0; TRIGGER Gate; portamento MODE Auto, TIME short. In SEQ, mark a few steps LEGATO: those notes slide and don't pluck again.
- **Lead**: SAW and PULSE together; FREQ middle, RES a little; S high; portamento Auto; LEVER up and use the mod wheel for vibrato; DELAY on 1/8. with some FEEDBACK.
- **PWM strings**: PULSE only, PULSE WIDTH up, PWM SOURCE LFO, RATE slow; A and R long; REVERB up.
- **Hi-hat**: NOISE only; FREQ high, KYBD 0; A 0, D short, S 0, R short.
- **Zap / tom**: no sources at all (or a breath of NOISE); RES full, KYBD full, ENV up, D medium, S 0: the ringing filter is the sound, and the keys tune it.
- **Rhythmic pulses**: TRIGGER LFO, WAVE Square, RATE to taste; D short, S 0: one held key plays a pattern of plucks.
- **Burbling filter**: WAVE Random, MOD up in the VCF, RES high; TRIGGER LFO for a new pluck on every random step.

## Troubleshooting

- **No sound**: are all the source mixer's sliders down? Is VOLUME down? With VCA MODE Env, is S at 0 with a short D (notes too short to hear)? Is FREQ at the bottom with ENV at 0? Is AUDIO lit?
- **A chord plays only one note**: Kobalt is monophonic: the newest key sounds. Use Kalliope or Krimson for chords.
- **Legato notes don't pluck**: TRIGGER is on Gate. Set Gate + Trig for a pluck on every note.
- **Notes glide when you don't want them to**: portamento MODE is On (every note) or Auto (legato notes). Set Off, or lift each key before the next.
- **Keys don't sound in SEQ mode**: while the sequencer runs, keys transpose the pattern instead of playing. Choose NOTE to play.
- **The arpeggio doesn't start**: in the Krate rack it follows the rack's PLAY; standalone, ARP starts the transport.
- **High or low notes are missing**: check OCTAVE: it shifts your MIDI keyboard, and notes shifted past the MIDI range don't play.
