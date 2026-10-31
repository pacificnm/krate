# Kalliope manual

Kalliope is a sixteen-voice polyphonic synthesizer: two oscillators per voice, a high-pass filter, a resonant ladder low-pass with drive, two envelopes, one LFO, an arpeggiator, a 16-step sequencer with a pattern generator, chorus, delay and reverb, and 64 patches. Its voices drift and differ a little from each other, as the voice boards of an analog polysynth do.

This manual goes through the panel from left to right and top to bottom. Every slider, knob, switch and button is described here: what it does, its range, and how to use it. Drag a slider or knob (or use the mouse wheel over it) to change it; its value shows while you drag; double-click to put it back to its default.

## Quick start

1. **Pick a sound.** The first time, Kalliope starts in Manual (the display shows **--**). Click **BANK 1** then **PATCH 1**: the display shows **11**, Brass. Step through all 64 with **▲ ▼** next to the display. Banks 1 and 2 hold the factory sounds.
2. **Play.** Use your MIDI keyboard or click the on-screen keys. **OCTAVE ◂ ▸** (or the Z and X keys) shifts the keyboard.
3. **Change it.** Move **CUTOFF FREQ** and **RES** in the VCF section first: they change the sound the most. Then the **A** (attack) and **R** (release) sliders of ENV-2.
4. **Keep it.** Press **WRITE**, then the **PATCH** button (and first a **BANK**, if you want another bank) to store into.
5. **Make it move.** Press **ARP** and hold a chord: Kalliope arpeggiates it in time with the tempo.

The panel as you leave it is kept when you quit; patches only change when you WRITE.

## How a voice makes sound

Each note gets a *voice*. In every voice:

```
VCO-1 ─┐
       ├─ SOURCE MIX ─► HPF ─► DRIVE ─► VCF ─► VCA ─► the effects ─► VOLUME
VCO-2 ─┘  (cross mod and sync between them)
```

- The two **VCOs** (oscillators) make the raw tone.
- **SOURCE MIX** balances them. The **HPF** (high-pass) thins out the bass. **DRIVE** pushes the sound harder into the filter.
- The **VCF** (low-pass filter) shapes it from dark to bright; resonance makes it sing or squelch.
- The **VCA** sets the level over time, following **ENV-2**.
- **ENV-1**, **ENV-2** and the **LFO** move things over time: pitch, pulse width, the filter, the level.
- After the voices: **chorus** (it makes Kalliope stereo), **delay**, **reverb**, then **VOLUME**.

## LFO

One low-frequency oscillator for the whole instrument: it wobbles pitch, pulse width, the filter or the level, as the LFO MOD controls in each section ask.

| Control | What it does |
| --- | --- |
| **RATE** | The LFO's speed, 0.05–30 Hz: from a slow sweep over many seconds to a fast flutter. |
| **DELAY TIME** | Fades the LFO in after each note starts, 0–5 s: notes begin steady and the vibrato (or wobble) grows in, as a string player's vibrato does. In Solo and Unison, it counts from the first note of a legato phrase. |
| **WAVE** | **Sine** (smooth), **Up saw** (rises then drops), **Down saw** (drops then rises), **Square** (jumps between two values: trills), **Random** (a new random value each cycle: the classic computer "sample and hold" burble). |

## VCO modulator

Moves the oscillators' pitch and pulse width.

| Control | What it does |
| --- | --- |
| **LFO MOD** | The LFO on pitch, up to ±1 octave. A little is vibrato; a lot is a siren. |
| **ENV MOD** | ENV-1 on pitch, up to +2 octaves: a pitch sweep at the start of each note (the sync lead's "tear", a zap, a falling tom). Shape it with ENV-1's sliders. |
| **DEST** | Which oscillators LFO MOD and ENV MOD move: **VCO-1**, **VCO-1 + 2**, or **VCO-2**. Moving only VCO-2 while it is synced to VCO-1 sweeps the sync sound without changing the pitch. |
| **PW MOD** | Pulse width (for the Pulse waves): how far the pulse narrows from a square (50 %) towards a thin pulse (5 %). With PWM SRC on Manual it simply sets the width; on LFO or ENV-1 it's how far they move it. |
| **PWM SRC** | What moves the pulse width: **LFO** (the slow, chorus-like "PWM" sound of string pads), **Manual** (PW MOD sets a fixed width), **ENV-1** (the width follows the envelope). |

## VCO-1

| Control | What it does |
| --- | --- |
| **CROSS MOD** | VCO-2 modulates VCO-1's frequency (cross modulation, a kind of FM): from a slight roughness to clangs, bells and metallic noise. Try it with VCO-2 at a different RANGE, or on Low for slow, wild sweeps. |
| **RANGE** | The octave: **16'** (one octave down), **8'** (as played), **4'** (one up), **2'** (two up). The footages are the organ-stop names for octaves. |
| **WAVE** | **Triangle** (soft, flute-like), **Saw** (bright and full: brass and strings), **Pulse** (its width from PW MOD: hollow to nasal), **Square** (hollow, clarinet-like). |

## VCO-2

| Control | What it does |
| --- | --- |
| **RANGE** | **Low** (a sub-audio rate, about 0.3 Hz at C2 and following the keyboard: for slow cross-mod sweeps and effects rather than notes), **16'**, **8'**, **4'**, **2'**. |
| **FINE TUNE** | Detunes VCO-2 by up to ±50 cents. A few cents against VCO-1 make the slow beating that fattens a sound (try 5–10). |
| **WAVE** | **Sine** (pure), **Saw**, **Pulse** (the same width as VCO-1's), **Noise** (white noise: wind, breath, snares and sweeps). |
| **SYNC** | **On**: VCO-2 restarts every time VCO-1 starts a cycle. VCO-2's pitch then shapes the tone instead of the note: set VCO-2 higher and sweep it (ENV MOD with DEST on VCO-2) for the tearing sync lead. |

## Mix and drive

| Control | What it does |
| --- | --- |
| **SOURCE MIX** | VCO-1 to the left, VCO-2 to the right, both at full level in the middle. |
| **DRIVE** | How hard the mixed sound goes into the filter: 0 is clean; turning it up adds warmth, then grit and growl (up to +18 dB into the filter's input, the output trimmed back by half that, so soft notes come up a little and loud ones grind). Drive and resonance fight each other as on a real ladder filter: lots of drive tames the resonance. |

## HPF

| Control | What it does |
| --- | --- |
| **FREQ** | A gentle high-pass filter before the VCF: at 0 it's off; turned up (20 Hz–1 kHz) it removes bass. Use it to thin pads and strings so they sit above a bass part. |

## VCF

The low-pass filter: a four-pole transistor ladder. It lets through what is under the cutoff and removes what is over it.

| Control | What it does |
| --- | --- |
| **CUTOFF FREQ** | The cutoff, 20 Hz–20 kHz: dark and muffled at the bottom, fully open at the top. Everything below adds to it: envelope, LFO, key follow, aftertouch. |
| **RES** | Resonance: a peak at the cutoff. A little makes the sound vocal; a lot squelches; at the top the filter whistles on its own (self-oscillation: a sine tone; with KEY FOLLOW at 100 % you can play it in tune). Loud resonance compresses and growls rather than getting ever louder, as on an analog ladder. |
| **SLOPE** | **-24 dB** (four poles: steep, round, the classic bass and brass filter) or **-12 dB** (two poles: brighter, buzzier). |
| **ENV MOD** | How far the envelope moves the cutoff, up to 8 octaves up: the "wow" at the start of each note. Velocity scales it (see VELOCITY). |
| **ENV SRC** | Which envelope moves the cutoff: **ENV-1** (its own shape) or **ENV-2** (the same shape as the level). |
| **LFO MOD** | The LFO on the cutoff, up to ±4 octaves: wah-wah and wobbles. |
| **KEY FOLLOW** | How much the cutoff follows the note: at 0 every note has the same cutoff (high notes sound duller); at 100 % the cutoff moves with the pitch (every note equally bright; a self-oscillating filter plays in tune). |

## VCA

| Control | What it does |
| --- | --- |
| **LEVEL** | Each voice's level. Turn it down if a big chord is too loud. |
| **LFO MOD** | The LFO on the level: tremolo. |

## Envelopes

**ENV-2** always shapes each note's level. **ENV-1** moves pitch (ENV MOD), pulse width (PWM SRC on ENV-1) and, with ENV SRC on ENV-1, the filter. Both have the same four sliders:

| Control | What it does |
| --- | --- |
| **A** | Attack: how long the note takes to rise when a key goes down, 1 ms–10 s. |
| **D** | Decay: how long it then takes to fall to the sustain level, 1 ms–10 s. |
| **S** | Sustain: the level it stays at while the key is held, 0–100 %. |
| **R** | Release: how long it takes to die away after the key comes up, 1 ms–10 s. |
| **KEY FOLLOW** (in ENV-2) | **On**: higher notes have shorter envelopes (up to half as long at the top of the keyboard), like a piano or a plucked string. It shortens both envelopes. |

A pluck: A 0, D short, S 0, R short. A pad: A long, S full, R long. An organ: A 0, D 0, S full, R short.

## Arpeggiator and sequencer

The left of the function deck. **Play Mode** chooses what your keys do:

| Control | What it does |
| --- | --- |
| **NOTE** | Keys play notes. |
| **ARP** | Keys feed the **arpeggiator**: hold a chord and it plays its notes one after another, in time. Standalone, ARP starts the clock; in the Krate rack it follows the rack's PLAY. |
| **SEQ** | The **step sequencer**: 8 patterns of up to 16 steps that play while the transport runs; your keys then transpose the pattern (by their distance from middle C) instead of sounding. |
| **HOLD** | Arpeggio notes stay after you let go; the first key of a new chord replaces them. |
| **START** | Standalone: starts and stops the transport (Space does the same). |
| **MODE** | Arpeggio order: **Up**, **Down**, **Up & Down** (without repeating the top and bottom notes), **Random**. |
| **RANGE** | The arpeggio (and the generator) spreads the held notes over 1–4 octaves. |
| **RATE** | Steps per beat for the arpeggiator and the sequencer: **1/8**, **1/16**, **1/8T**, **1/16T** (triplets). Each note lasts half a step. |
| **DENSITY** | For the pattern generator: how many steps play (the rest are rests). |
| **GEN** | Generates a new pattern into the current one, from the keys you hold (or the pattern's notes, or a C minor pentatonic scale). Each press makes a different one. |
| **REST** | While recording: writes a rest into the current step and moves on. |
| **TIE** | While recording: holds the previous step's notes through this step. |
| **CLEAR** | Empties the current pattern (16 rests). |

**Recording a pattern**: choose **SEQ** with the transport stopped. The BANK and PATCH rows become steps 1–8 and 9–16. Click a step (it blinks), then play a note or a chord: when you let go, the notes go into the step and the cursor moves on. **Shift-click** a step to make it the last one. **▲ ▼** next to the display choose the pattern (**P1**–**P8**). Patterns belong to the instrument, not to a patch: change the sound while the pattern plays.

## Key mode

| Control | What it does |
| --- | --- |
| **POLY** | Up to 16 notes at once. A 17th takes the voice that was released longest ago, else the oldest held one (it fades out quickly, without a click). |
| **UNISON** | 8 voices on one note, detuned across DETUNE: a huge, thick lead or bass. One note at a time, the newest key wins; holding a key and playing another moves to the new note without restarting the envelopes (legato). |
| **SOLO** | One voice, the newest key wins, legato: for basses and leads played like a monophonic synth. |
| **DETUNE** | How far apart the Unison voices are, ±0–48 cents (±12 by default). Low: a chorus-like shimmer; high: a sour, wide supersaw. You can turn it while a note plays. |
| **ANALOG** | How much the voices behave like analog circuits (0–100 %, 30 % by default). Each oscillator drifts slowly in pitch on its own, and each of the 16 voices has its own slightly different tuning, cutoff and envelope timing, as separate voice boards do: chords breathe and no two notes are quite alike. At 0 every voice is identical and perfectly in tune. Turn it down for sounds with two oscillators in unison that must not beat (the Sub Bass patch has it at 0); turn it up (60–100 %) for a vintage, slightly wobbly character. |

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

**Factory patches**: bank 1: 11 Brass, 12 Strings, 13 Soft Pad, 14 Bass, 15 Sync Lead, 16 Bells, 17 Unison Lead, 18 Sweep Bass; bank 2: 21 Organ, 22 Pluck, 23 Tremolo Keys, 24 Noise Sweep, 25 Arp Pluck (arpeggio on), 26 Detuned Saws, 27 Sub Bass, 28 Low Wobble. Banks 3–8 start empty (Init) for your own sounds.

## Effects

After the voices, in this order: chorus, delay, reverb.

| Control | What it does |
| --- | --- |
| **CHORUS** | Click to cycle **Off**, **I**, **II**. A bucket-brigade-style stereo chorus: the two sides sweep against each other, the copies are darker and softly saturated like old chorus chips. **I** is slow and gentle; **II** faster and deeper. With it off, Kalliope is mono. |
| **REVERB** | Reverb level (0 = off). |
| **SIZE** | The reverb's decay, from 0.4 s (a small room) to 8 s (a huge hall). |
| **DELAY** | Echo level (0 = off). The echoes bounce left, right, left. |
| **TIME** | Tempo-synced: **1/16**, **1/8T**, **1/8**, **1/8.** (dotted), **1/4**, **1/4.**, **1/2**. |
| **FEEDBACK** | How many repeats, 0–90 %; each repeat is a little darker. |

## Playing controls

Next to the keyboard, on the right:

| Control | What it does |
| --- | --- |
| **VELOCITY** | How much how hard you play matters, 0–100 %: on the level and on the filter's ENV MOD. At 0 every note plays the same. |
| **BEND** | The pitch bend wheel's range, 1–12 semitones. |
| **PRESSURE** | What aftertouch does, if your keyboard sends it (channel or polyphonic pressure; a key's pressure acts on every voice): **Off**, **Filter** (pressing harder opens the cutoff, up to 2.5 octaves), **LFO** (adds LFO vibrato, up to ±0.7 semitone), **Both**. |

The **mod wheel** adds LFO vibrato, up to ±1 semitone at full (at the LFO's RATE). The **sustain pedal** holds released notes until it lifts.

## Volume, tempo, keyboard

| Control | What it does |
| --- | --- |
| **VOLUME** | Kalliope's output level. |
| **TEMPO** | Standalone only: the tempo (40–240 BPM) for the arpeggiator, the sequencer and the delay. In the Krate rack, the rack's transport sets the tempo. |
| **OCTAVE ◂ ▸** | Shifts your MIDI keyboard and the on-screen keyboard by −3 to +3 octaves (Z and X on the computer keyboard). The display shows the octave you're playing in, as on Krimson: 0 is where the keys play as marked; it adds your MIDI keyboard's own octave buttons (as the keys show it) to Kalliope's shift. A song's notes always play as written, whatever OCTAVE is set to. |
| **The on-screen keyboard** | Two octaves, played with the mouse (press and glide). Keys held from MIDI light up. |

## MIDI and MIDI Learn

| Control | What it does |
| --- | --- |
| **LEARN** | MIDI Learn (also the L key; Esc leaves it): click a slider, knob or button, then move a knob or press a button on your MIDI controller; they're linked. Right-click a control in Learn to remove its link. |
| **UNMAP** | Removes every MIDI link. |
| **AUDIO** | Lit when Kalliope is connected to the audio system; click to reconnect. The line under it shows the sample rate, buffer and dropouts, and the meter the output level. |

- **Channel**: standalone, Kalliope plays on every channel; in the Krate rack, on channel 2 by default.
- **Learnable buttons**: NOTE, ARP, SEQ, HOLD, START, GEN, REST, TIE, WRITE, the BANK and PATCH buttons and OCTAVE ◂ ▸, so a controller can switch patches, arpeggios and patterns while you play.
- **Program Change** loads a patch (0 = 11, 1 = 12, … 8 = 21, …). **Pitch bend**, **CC 1** (mod wheel), **CC 64** (sustain) and **aftertouch** are understood; CC 120 and 123 stop every note.

## Sound design recipes

Start from an Init patch (an empty one in banks 3–8) or from the nearest factory sound.

- **Brass**: VCO-1 and VCO-2 Saw, FINE TUNE a few cents; SLOPE -24 dB, CUTOFF FREQ low-middle; ENV MOD up; ENV-1 A a little slow (0.1–0.3 s) so the brightness swells in; ENV-2 A a touch slower than instant, S high.
- **Strings**: two Saws slightly detuned; HPF a little; CUTOFF FREQ middle, ENV MOD low; ENV-2 A and R long; LFO MOD in the VCO MODULATOR a hint, DELAY TIME up; CHORUS I or II; REVERB up.
- **PWM pad**: both Pulse; PW MOD up, PWM SRC LFO, RATE slow; ENV-2 A and R long; CHORUS II; ANALOG 50 %.
- **Fat bass**: SOLO; VCO-1 Saw 16', VCO-2 Pulse 16'; CUTOFF FREQ low, RES a little, ENV MOD up with a short ENV-1 decay and S 0; DRIVE 30–50 %.
- **Squelchy bass**: SOLO; one Saw; RES high; ENV MOD up with a short decay; VELOCITY up so accents open the filter; DRIVE a little to growl.
- **Sync lead**: SYNC on; SOURCE MIX towards VCO-2; ENV MOD up, DEST VCO-2, ENV-1 decay medium: each note tears from bright to its pitch; DELAY on 1/8. dotted.
- **Supersaw lead**: UNISON, DETUNE 20–30 cents; Saws; CUTOFF FREQ fairly open; CHORUS off (Unison is wide already) or I; DELAY on.
- **Bells**: VCO-1 Triangle, VCO-2 Sine 2'; CROSS MOD 30–50 %; SOURCE MIX to VCO-1; ENV-2 A 0, D long, S 0; REVERB up.
- **Noise sweep**: VCO-2 Noise, SOURCE MIX to VCO-2; RES high; ENV MOD up with long ENV-1 attack and decay; REVERB large.
- **Expressive lead**: PRESSURE Both: press into the keys to brighten and add vibrato.

## Troubleshooting

- **No sound**: is VOLUME or the VCA LEVEL down? Is ENV-2's S at 0 with a short D (notes too short to hear)? Is CUTOFF FREQ at the bottom with ENV MOD at 0? Is AUDIO lit?
- **Keys don't sound in SEQ mode**: while the sequencer runs, keys transpose the pattern instead of playing. Choose NOTE to play.
- **The arpeggio doesn't start**: in the Krate rack it follows the rack's PLAY; standalone, ARP starts the transport.
- **High or low notes are missing**: check OCTAVE: it shifts your MIDI keyboard, and notes shifted past the MIDI range don't play.
- **Two oscillators on the same pitch swell and dip**: that's ANALOG drifting them against each other; turn ANALOG down for that sound.
- **Big chords distort**: turn the VCA LEVEL or VOLUME down, or DRIVE and RES down.
- **Aftertouch does nothing**: set PRESSURE, and check that your keyboard sends aftertouch (many don't).
- **Notes cut off when playing many**: 16 voices are shared by every note including release tails; shorter releases free voices sooner.
