# Krimson manual

Krimson is a four-part virtual analog synthesizer: four complete sounds (slots **A–D**) that you can play one at a time, layer, split across the keyboard, or play from four MIDI channels at once. Each slot has two oscillators, a multi-mode filter, three envelopes, two LFOs, an arpeggiator, morph, and its own effects. Sixteen voices are shared between the slots.

This manual goes through the panel from left to right. Every control you can turn, press or pick is described here: what it does, its range, and how to use it. Most knobs show their value in a bubble while you drag them; double-click a knob to put it back to its default.

## Quick start

1. **Pick a sound.** Click slot **A**, then step through programs with **▲ ▼** next to the program number. Programs are numbered **11–88** (bank 1–8, program 1–8). Banks 1 and 2 are general sounds; 3–8 are 80s and 90s club, industrial and house sounds.
2. **Play.** Use your MIDI keyboard or click the on-screen keys. Move the **MOD** wheel: on many programs it opens the filter or changes the sound (that's *morph*).
3. **Change it.** Turn **FREQ** (the filter cutoff) and **RES** first: they change the sound the most. Then try **ATTACK** and **RELEASE** in AMP ENV.
4. **Keep it.** Press **STORE**, choose a number with **▲ ▼**, press **STORE** again.
5. **Layer two sounds.** Shift-click slot **B**: both A and B now play from the keyboard.

Everything you change is kept automatically when you quit; programs are only changed when you STORE.

## How a slot makes sound

Each note a slot plays gets a *voice*. In every voice:

```
OSC 1 ─┐
       ├─ MIX ─► FILTER ─► AMP ─► pan ─► the slot's effects ─► OUTPUT
OSC 2 ─┘   (ring mod, FM, sync between them)
```

- The **oscillators** make the raw tone.
- The **filter** shapes it (dark to bright, hollow, resonant).
- The **amp** makes it louder and quieter over time, following the AMP ENV.
- **Envelopes** and **LFOs** move things over time: the FILTER ENV moves the cutoff, the MOD ENV moves the second oscillator, the LFOs wobble pitch, filter, level and more.
- After the voices, the slot's **effects** (drive, chorus, delay, reverb) and its **output** (level and pan).

The panel always shows and edits **one slot**, the one whose button is outlined. Each slot keeps its own settings.

## Master, octave, wheel and program

The left of the panel holds controls for the whole instrument and the edited slot's program.

| Control | What it does |
| --- | --- |
| **MASTER LEVEL** | The whole instrument's volume (all four slots). |
| **OCTAVE ◂ ▸** | Shifts what your MIDI keyboard plays by −3 to +3 octaves (the on-screen keyboard shows the shifted range). |
| **TEMPO** | Standalone only: the tempo for the arpeggiator and tempo-synced delay. In the Krate rack the transport bar sets the tempo. |
| **WHEEL** | What the mod wheel does for the edited slot: **Morph** (moves the sound towards its morph targets, see *Morph*) or **Vibrato** (pitch vibrato, up to 0.7 semitones at full). |
| **MORPH** | Switches the knobs into morph-target editing (see *Morph*). |
| **CLEAR** | Clears the edited slot's morph targets. |
| **PROGRAM** | The edited slot's program number, with its name under it. A dot after the number means you've changed the sound since it was loaded. **▲ ▼** step through all 64 programs. |
| **STORE** | Saves the edited slot's sound into a program (see *Programs and performances*). |

## Slots, layering and splits

Krimson has four slots, **A–D**. Each is a complete sound with its own MIDI channel.

- **Click a slot button** to play and edit that slot alone: only it lights up, the keyboard plays it, and the panel shows it.
- **Shift-click** more slot buttons to **layer** them: every lit slot plays from the keyboard; the last one you lit is the one you edit. Shift-click a lit slot to take it out again.
- **SLOT SETUP** (under the slot buttons) sets up the edited slot:

| Control | What it does |
| --- | --- |
| **CH ◂ ▸** | The slot's own MIDI channel (default: A = 3, B = 4, C = 5, D = 6). Notes on that channel always play the slot, whatever is lit, so a sequencer or a second keyboard can play each slot directly. |
| **ZONE ◂ ▸** | **Whole**, **Lower** or **Upper**: with a split set, a Lower slot plays only below the split key and an Upper slot only from it up. |
| **SPLIT ◂ ▸** | The split key (**Off** = no split). Set slot A to Lower and slot B to Upper for a bass on the left hand and a lead on the right. |
| **ON** | Turns the slot on or off. An off slot doesn't play, from the keyboard or its channel. |
| **SPLIT OFF** | Clears the split. |

**Which slot hears what**: a message on a channel that belongs to an on slot goes to that slot. Any other channel counts as *the keyboard*, and goes to the lit slots (by zone when a split is set). The mod wheel, pitch bend, sustain pedal and aftertouch follow the same rule.

## Arpeggiator

Each slot has its own arpeggiator, locked to the tempo: it plays the keys you hold, one after another.

| Control | What it does |
| --- | --- |
| **ARP** | Turns the edited slot's arpeggiator on. Standalone, it also starts the transport. |
| **HOLD** | Latches the keys: lift your hands and the arpeggio keeps going. Play a new chord to replace it. |
| **MODE** | **Up**, **Down**, **Up & Down** or **Random**. |
| **RANGE** | 1 to 4 octaves: the pattern repeats this many octaves up. |
| **RATE** | **1/8**, **1/16**, **1/8T** (eighth-note triplets) or **1/16T**. |

The arpeggio plays in the slot's voice mode: in **Poly** each step gets its own voice (release tails overlap, good for plucks), in **Mono**, **Legato** or **Unison** it's one line. Each note lasts half a step. Slot A can arpeggiate while slot B plays chords.

## Voice

| Control | What it does |
| --- | --- |
| **VOICE MODE** | **Poly**: chords, each note its own voice. **Mono**: one note at a time, the last key wins, every note restarts the envelopes. **Legato**: one note, and notes played while another is held slide on without restarting the envelopes (classic for basses and leads). **Unison**: one note made of a stack of detuned voices: huge leads and basses. |
| **GLIDE** | Portamento: the pitch slides from the last note to the new one (Off to 2 s). In Legato it only glides between held notes. |
| **DETUNE** | In Unison: how far apart the stacked voices are tuned (the outer ones up to ±35 cents). A little = thick; a lot = a wide, buzzy supersaw-like sound. |
| **VOICES** | In Unison: **3**, **5** or **7** stacked voices, spread across the stereo field. More voices are each quieter, so the level stays about the same. |
| **BEND** | Pitch-bend range: 1 to 12 semitones. |

## LFO 1 and LFO 2

LFOs (low-frequency oscillators) wobble something up and down, over and over.

**LFO 1** runs separately in every voice:

| Control | What it does |
| --- | --- |
| **LFO 1 RATE** | 0.05 Hz (one cycle every 20 s) to 40 Hz. |
| **AMOUNT** | How much it moves its destination. For pitch the effect grows faster than the knob (gentle vibrato at low settings, up to ±12 semitones at full). |
| **WAVE** | **Triangle** (smooth), **Saw** (ramps), **Square** (jumps between two values: trills), **S&H** (sample and hold: a new random value each cycle, the classic "computer" sound), **Soft random** (smooth random wandering). |
| **DEST** | **OSC 1+2** (pitch: vibrato), **OSC 2** (only the second oscillator's pitch: great with sync), **Filter** (the cutoff, up to ±4 octaves: wah and wobble), **Pulse width** (moving PWM: a chorused, alive pulse), **FM** (moves the FM amount). |

**LFO 2** is one per slot (all its voices move together) and always a triangle:

| Control | What it does |
| --- | --- |
| **LFO 2 RATE** | 0.05 to 40 Hz. |
| **AMOUNT** | How much. |
| **DEST** | **Amp** (tremolo: the level pulses), **Filter** (±2 octaves), **OSC 2** (the second oscillator's pitch). |

**Tip**: a slow LFO 1 to Pulse width with a Pulse wave gives an instant analog-string shimmer; LFO 1 on S&H to Filter with high resonance is the classic random-bleeps sound.

## Mod envelope

A simple attack–decay envelope that shapes the start of every note. It always starts from zero.

| Control | What it does |
| --- | --- |
| **ATTACK** | How long it takes to rise (0.5 ms to 10 s). |
| **DECAY** | How long it takes to fall back to zero. |
| **AMOUNT** | How much, up or down (−100 % to +100 %). |
| **DEST** | **OSC 2 pitch** (up to ±4 octaves: with sync on, the famous sweeping sync scream; on its own, a pitch "blip" or laser zap), **FM** (adds FM at the start of the note: metallic, bell-like attacks), **Pulse width**. |

## Oscillators

Two oscillators per voice. All waves are band-limited, so they stay clean high up the keyboard.

| Control | What it does |
| --- | --- |
| **OSC 1** | **Sine** (pure, soft), **Triangle** (soft, a little hollow), **Saw** (bright and buzzy: strings, brass, leads, basses), **Pulse** (hollow, woody; its width is set by PULSE WIDTH). |
| **OSC 2** | **Triangle**, **Saw**, **Pulse** or **Noise** (wind, snares, breaths, sweeps). |
| **PULSE WIDTH** | For Pulse waves: from a square (50 %, hollow) to a thin pulse (5 %, nasal). |
| **SEMITONES** | Tunes OSC 2 from −24 to +24 semitones against OSC 1: +7 or +12 for fifths and octaves, odd values for clangy, inharmonic sounds with sync or ring. |
| **FINE** | Fine-tunes OSC 2 (±50 cents): a few cents against OSC 1 gives a thick, slowly beating sound. |
| **MIX** | The balance between OSC 1 and OSC 2 (at the middle, both at full). |
| **FM** | Frequency modulation: OSC 2 wobbles OSC 1's frequency at audio rate. A little adds bite; more gives bells, metal and growls. Try OSC 1 Sine, OSC 2 Triangle, SEMITONES +12 or +19. |
| **KBD** | On (lit): OSC 2 follows the keyboard. Off: it stays at one pitch (middle C plus SEMITONES), a drone or a fixed formant. |
| **SYNC** | Hard sync: OSC 2 restarts every time OSC 1 does. Moving OSC 2's pitch (SEMITONES, the MOD ENV or LFO 1 on OSC 2) then gives the tearing, vocal sync lead. Hear OSC 2 with MIX turned towards it. |
| **RING** | Ring modulation: OSC 2 becomes OSC 1 × OSC 2: bell, clang and robot tones. Hear it with MIX towards OSC 2. |

## Filter

The filter removes part of the sound: the most important part of sculpting a tone.

| Control | What it does |
| --- | --- |
| **TYPE** | **LP 12**: a low-pass (removes the highs), gentle (12 dB/octave), bright and lively. **LP 24**: a steep four-pole low-pass ladder (24 dB/octave): darker, rounder, fat basses and classic squelch. **Band-pass**: only a band around the cutoff (thin, nasal, vocal). **High-pass**: removes the lows (thin, airy). **Notch + LP**: a notch at the cutoff, smoothed by a low-pass (phasey, hollow). |
| **FREQ** | The cutoff frequency, 20 Hz to 20 kHz: lower is darker. |
| **RES** | Resonance: a peak at the cutoff that makes it whistle, squelch or sing. High resonance compresses and growls rather than screaming, as on analog filters. With LP 24, high resonance thins the bass a little, as a real ladder does. |
| **DRIVE** | Saturates the sound going into the filter: from warm to gritty. |
| **ENV AMT** | How far the FILTER ENV opens the cutoff (up to 8 octaves). Pluck, bass and brass sounds use a lot. |
| **VELOCITY** | How much playing softer reduces the filter envelope: 0 = every note opens the same; 100 % = soft notes stay dark, hard notes snap open. |
| **KBD TRACK** | How much the cutoff follows the keyboard: **0** (same cutoff everywhere: high notes get darker), **1/3**, **2/3**, **1** (fully: the same brightness on every note). |
| **FILTER ENV A D S R** | The filter envelope's **A**ttack, **D**ecay, **S**ustain level and **R**elease (times 0.5 ms to 10 s). |

**How an envelope works**: when you press a key it rises over the **attack**, falls over the **decay** to the **sustain** level, stays there while you hold the key, then falls to zero over the **release** when you let go.

## Amp envelope

Shapes each note's loudness.

| Control | What it does |
| --- | --- |
| **ATTACK** | How fast the note fades in: short for plucks and basses, long for pads and swells. |
| **DECAY** | How fast it falls after the attack, to the sustain level. |
| **SUSTAIN** | The level while the key is held: 100 % for organs and pads, 0 for plucks, pianos and percussive sounds. |
| **RELEASE** | How long the note rings after you let go. |
| **VELOCITY** | How much playing softer makes the note quieter: 0 = every note the same; 100 % = fully touch-sensitive. |
| **GAIN** | The voice's level into the slot (use it to balance sounds; the slot's LEVEL is in OUTPUT). |

## Effects

Each slot has its own effects, in this order: drive, chorus, delay, reverb.

| Control | What it does |
| --- | --- |
| **CHORUS** | **Off**; **Chorus** (a wide, smooth stereo chorus); **Ensemble** (three copies per side with a shimmer: the thick string-machine sound). Both are darkened and softly saturated like analog chorus chips. |
| **DELAY** | Echo level (0 = off). |
| **FEEDBACK** | How many repeats. |
| **TIME** | Tempo-synced: **1/16**, **1/8T**, **1/8**, **1/8.** (dotted), **1/4**, **1/4.**, **1/2**. Dotted eighths make the classic galloping echo. |
| **REVERB** | Reverb level (0 = off). |
| **SIZE** | From a small room to a long hall. |
| **DRIVE** | Grit on the slot's sound before the chorus, delay and reverb: from warm to distorted (0 = off). Different from the filter's DRIVE: this one is after the filter, so it distorts the filtered sound. |

## Output

| Control | What it does |
| --- | --- |
| **LEVEL** | The slot's volume, to balance layered or split slots. |
| **PAN** | The slot's place from left to right. |
| **ANALOG** | How alive the slot is (0–100 %, 30 % by default). At 0 every note is pitch-perfect and starts the same way, like a digital synth. Turned up, each note's oscillators start at a random point in their cycle, sit a few cents apart, drift slowly, and the filter sits a little differently per note: chords and pads breathe, unison spreads wider. Keep it at 0 for designed percussive hits that need the exact same attack every time. |
| **PRESSURE** | What aftertouch does, if your keyboard sends it: **Off**, **Filter** (pressing harder opens the cutoff, up to 2.5 octaves), **Vibrato**, or **Filter + vib**. |

## Wheels and keyboard

- **PITCH** wheel: bends the pitch by the slot's BEND range. It springs back to the middle.
- **MOD** wheel: morph or vibrato, as WHEEL is set. Both on-screen wheels follow your MIDI keyboard's wheels.
- **The on-screen keyboard** plays the lit slots and lights the keys that sound. It's laid out like your MIDI keyboard (set your keyboard's keys and first note under **KEYS** in Krate's top bar).
- The **sustain pedal** holds the notes of the slots it reaches.

## Morph

Morph lets one control (the mod wheel, or how hard you play) move *many* knobs at once, from the slot's sound to a second version of it.

1. Press **MORPH**: it lights up and every knob now shows and sets its **morph target** (starting from its current value). Turn the knobs you want to move: a blue arc on each ring shows the way from its value to its target.
2. Press **MORPH** again: the knobs set their normal values again.
3. Now the **mod wheel** sweeps every knob with a target from its value to its target (WHEEL set to **Morph**). With WHEEL on **Vibrato**, each note's **velocity** morphs instead: soft notes play the panel's sound, hard ones the targets.
4. With MORPH lit, **right-click** a knob to clear its target; **CLEAR** clears them all.

Only knobs morph: waves, filter type, switches and lists don't. **Ideas**: cutoff and resonance up for a wheel-controlled filter sweep; FM and SEMITONES for a sound that turns metallic; attack and release longer so hard playing sounds different from soft; DELAY and REVERB up to wash a lead out with the wheel.

## Programs and performances

**Programs** are single-slot sounds: 8 banks of 8, numbered **11** to **88**. A program keeps everything on the panel for one slot, including its morph targets.

- **Choose**: **▲ ▼**, or Program Change from your keyboard (on a slot's channel: that slot; on the keyboard channel: the lit slots).
- **Store**: press **STORE** (it turns red), choose the number with **▲ ▼** (it blinks), press **STORE** again. Storing replaces what was there.

**Factory programs**: bank 1 (11 Sync Lead, 12 FM Bells, 13 Ring Clang, 14 Acid 12, 15 Unison Lead, 16 Saw Bass, 17 Morph Pad, 18 Pluck), bank 2 (21 Strings, 22 Brass, 23 Notch Sweep, 24 Rave Stab, 25 Sub Bass, 26 Perc Organ, 27 Arp Pluck, 28 Noise Sweep), bank 3 industrial and EBM, bank 4 acid and techno basses, bank 5 rave and stabs, bank 6 house, bank 7 leads and arpeggios, bank 8 pads and effects. Many respond to the mod wheel.

**Performances** keep all four slots at once (their sounds, channels, zones, which are lit, the split): 8 of them. **PERF ◂ n ▸** chooses one (a dot after the number means it's empty), **LOAD** recalls it, **SAVE** writes the current setup into it. Use them for a whole song's setup: a split bass and lead, or four layered parts.

## MIDI and MIDI Learn

- **Channels**: each slot listens to its own channel (3–6 by default); any other channel plays the lit slots.
- **MIDI Learn** (in Krate: **LEARN** in the top bar): click a knob or button, then move a knob or press a button on your controller; it's mapped. Learned knobs edit the *edited* slot. **Right-click** a control in Learn to remove its mapping.
- **Learnable buttons**: ARP, HOLD, MORPH, the bank and program buttons, so a controller can switch arpeggios or programs while you play.
- **Program Change** loads programs; **CC 1** (mod wheel), **pitch bend**, **CC 64** (sustain) and **aftertouch** are understood; CC 120 and 123 stop every note.

## Sound design recipes

Start each from a simple program (or set OSC 1 to Saw, MIX to OSC 1, filter LP 24 open, amp envelope fully up).

- **Fat bass**: VOICE MODE Mono or Legato; OSC 1 Saw, OSC 2 Pulse an octave down (SEMITONES −12), MIX at the middle; LP 24, FREQ low, RES a little; ENV AMT up, FILTER ENV decay short, sustain low; AMP ENV sustain 100 %, release short.
- **Acid**: OSC 1 Saw or Pulse alone; LP 12 or LP 24; RES high; ENV AMT up with a short filter decay; filter VELOCITY up; GLIDE a little; Legato. Play the cutoff while a sequence runs.
- **Pluck**: AMP ENV attack 0, decay short, sustain 0, release short; filter ENV AMT up with a short decay; DELAY on 1/8. dotted.
- **Pad**: two Saws, FINE a few cents; LP 12, FREQ in the middle; AMP ENV attack and release long, sustain full; CHORUS Ensemble; REVERB up; ANALOG 40–60 %.
- **Supersaw lead**: VOICE MODE Unison, VOICES 7, DETUNE 40–60 %; Saws; LP 12 fairly open; CHORUS Chorus; DELAY on.
- **Sync lead**: SYNC on; MIX towards OSC 2; MOD ENV to OSC 2 pitch, AMOUNT +, a medium decay (the sweep at each note's start); or LFO 1 to OSC 2 for a moving tear.
- **FM bell**: OSC 1 Sine, OSC 2 Triangle, SEMITONES +19; FM 30–60 %; AMP ENV attack 0, long decay, sustain 0; MOD ENV to FM with a short decay for the strike.
- **Ring-mod clang**: RING on, MIX towards OSC 2, SEMITONES +13 or +6; short amp decay; REVERB.
- **Noise snare or sweep**: OSC 2 Noise, MIX to OSC 2; Band-pass or High-pass; a short amp envelope for a snare, LFO 1 or the filter envelope slowly on the cutoff for a sweep.

## Troubleshooting

- **No sound from the keyboard**: is a slot lit (outlined)? Is it **ON**? Is your keyboard on a channel that belongs to an *off* slot? Is MASTER LEVEL or the slot's LEVEL down?
- **Low or high notes don't play**: check **OCTAVE**: it shifts your MIDI keyboard; notes shifted past the MIDI range don't play.
- **A slot plays when you didn't expect it**: notes on a slot's own channel always play it; change its **CH** or turn it **OFF**.
- **The sound is quieter than a program you remember**: VELOCITY in AMP ENV makes soft playing quieter; play harder or turn VELOCITY down.
- **The wheel does nothing**: WHEEL on Morph only does something if the slot has morph targets (MORPH, then turn knobs). Try WHEEL on Vibrato.
- **The arpeggio doesn't start**: in the Krate rack it follows the rack's PLAY; standalone, ARP starts the transport.
- **Aftertouch does nothing**: set PRESSURE, and check that your keyboard sends aftertouch (many don't).
