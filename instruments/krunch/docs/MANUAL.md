# Krunch manual

Krunch is a guitar amp built around **amp captures**: each of its four channels plays a capture of a real amp (a `.nam` file) that you choose, with its own gain, EQ, volume and noise gate; two cabinets per channel from your own impulse responses (IRs), spread across the stereo field if you like; a pedalboard of up to eight pedals in front of the amp or in its effects loop. Presets keep the whole rig by name, captures included: one per song or style, with the four channels as the instant switches within it.

This manual goes through the panel from top to bottom: the grille (presets and where the guitar comes from), the control plate (input, channels, the channel's amp, the playing channel and its cabinet) and the pedalboard. Drag a knob (or use the mouse wheel over it) to change it; its value shows under it; double-click to put it back to its default.

![The Krunch panel](images/panel.png)

## Quick start

1. **Plug in.** Plug your guitar into your audio interface's instrument input. In Krate's top bar, open **AUDIO** and choose that interface input as input 1 (or 2).
2. **Pick the input.** Set Krunch's **INPUT** knob to that input (**In 1** or **In 2**). The grille shows **GUITAR** and the input's name once it's connected; play and the **IN** meter moves. If the **CLIP** light flashes, turn the interface's own gain down.
3. **Get some amp captures.** Krunch has no amp of its own: a channel plays the capture you give it, and a channel without one is silent (its button says **NO AMP**). Captures are `.nam` files, an open format; thousands are shared for free on community capture sites. Get **amp-only** captures (the amp into a load, no speaker: they take your cabinet IRs) or **full-rig** captures (a miked cabinet in them). Choose ones made at 48 kHz, the rate the player runs at.
4. **Put them on Krunch.** From your phone or another computer, open **Krate Remote**, go to **Files ▸ Places ▸ Amp captures (Krunch)** and upload or drop them. Or press **CHOOSE AMP ▾** and **IMPORT** them from this computer.
5. **Choose one per channel.** Press **1**, **2**, **3** or **4** under **CHANNEL**, then **CHOOSE AMP ▾** and click a capture: it plays at once, so you can play and compare. A clean one on 1, a crunch on 2, high gain on 3 and 4, say.
6. **Choose cabinets.** For an amp-only capture, click the **A** bar under **CABINET** and import your IRs (WAV files, folders or zip packs); click one to hear it at once. For a full-rig capture, set **CABINET** to **OFF**.
7. **Shape it.** **GAIN** drives the capture harder or softer (noon: as it was captured), **BASS**, **MID**, **TREBLE** and **PRESENCE** shape it after, **VOLUME** balances the channel with the others, **LEVEL** sets how loud it all is.
8. **Add pedals.** Click **+** (**ADD PEDAL**) on the pedalboard and choose one; its footswitch turns it on and off.
9. **Keep it.** Press **SAVE AS…** on the grille and give the rig a name. Make a preset for each song or style you play; within one, the four channels switch instantly.

If the grille's **DELAY** line is orange, the trip from your guitar to the speakers is over 12 ms, which you can feel when you play: set a smaller buffer (128 frames is a good start) under **AUDIO**.

**Coming from an older Krunch?** Its own tube voices are gone. A channel or preset that played one is silent until you choose a capture for it; then save the preset again.

## How Krunch makes sound

```
guitar ─► front pedals ─► gate ─► AMP (the playing channel's capture, driven by GAIN, then BASS … PRESENCE) ─► channel VOLUME
       ─► loop pedals (if the channel's LOOP is on) ─► CABINET (IRs A and B, the built-in speaker, or off) ─► LEVEL ─► out
```

Only the playing channel's capture runs. Switching channels crossfades over 15 ms, so it never clicks, and each channel keeps its own capture, knobs and cabinet. The amp is mono up to the cabinet; a stereo IR, **SPACE** or the **Dual Head** pedal makes it stereo.

## The grille: presets and input

| Control | What it does |
| --- | --- |
| **The preset name** | The preset you're playing, in green, with a **\*** once anything has changed since it was loaded or saved; **NO PRESET** if none. Click it for the list of presets: click one to load it, **RENAME** it, or **DELETE** it (press twice: **SURE?**). |
| **◂ ▸** | The previous or next preset, round the list. Both are MIDI-learnable, for a foot controller. |
| **SAVE** | Saves the rig over the current preset (lit orange while it's modified). With no preset, it asks for a name. |
| **SAVE AS…** | Saves the rig as a new preset: type a name and press **SAVE** (or **CANCEL**). A name that's taken is replaced, and it says so first. |
| **HELP** | Opens this manual. |
| **GUITAR** | The interface input Krunch is playing. **WAITING FOR** that input means the device isn't there yet (is it plugged in?); **NO INPUT CHOSEN** means none is chosen under **AUDIO** for Krunch's **INPUT**. |
| **DELAY** | How long your guitar takes to reach the speakers, and the buffer size. Under about 10 ms feels like playing an amp; over 12 ms it turns orange. **AUDIO OFFLINE**: Krate isn't connected to the audio system. |

A preset is the whole rig: every knob of the four channels, which channel plays, each channel's amp capture and cabinet IRs, and the pedalboard. Keep one per sound you need: one per song, or one per style (a clean jazz rig, a crunch rig, a high-gain rig), each with four channels to switch between while you play. Presets are kept in `~/.local/share/krunch/presets.json`. Saving a song's sounds on the Song screen keeps the same snapshot with the song.

## Input, meters and channels

| Control | What it does |
| --- | --- |
| **INPUT** | Which of Krate's two inputs Krunch plays: **In 1** or **In 2**, as set under **AUDIO** in the top bar. Set it to the interface input your guitar is plugged into. |
| **IN** | The input meter: how hot the guitar comes in. |
| **CLIP** | Lights when the input clips: the interface's gain is too high. Turn it down on the interface. |
| **CHANNEL** **1** – **4** | Picks the playing channel; the lit one is playing. Under each, the name of its capture, or **NO AMP** (silent). Each is learnable, to switch channels from a foot controller. |
| **GATE SHUT** | Lights while the playing channel's noise gate is closed (you're not playing). |
| **LEVEL** | Krunch's output level, −60 … +12 dB (all the way down: silent). |
| **OUT** | The output meter. |

## The channel's amp

The box at the top of the plate, **AMP · CHANNEL** and the playing channel's number, shows which capture that channel plays.

| Control | What it does |
| --- | --- |
| **The capture's name** | The amp the playing channel plays. **No amp on this channel** when it has none: the channel is silent, and a hint says how to get one. **Missing:** before the name (in amber, on the channel's button too) when its file isn't in the captures folder any more (deleted or renamed, or a preset from another Krunch): the channel is silent until the file is back or you choose another. |
| **CHOOSE AMP ▾** | Opens the amp picker for the playing channel (below). |

## Amp captures

A capture is a real amp, modelled: a small file made by playing recordings through the amp and training a model on how it answered, distortion and feel included. Krunch reads `.nam` capture files, an open format; thousands are shared for free on community capture sites by players who made them. Most are made at 48 kHz, the rate the player runs at (another rate plays too, with a note in the picker).

![The amp picker: no amp, then the captures](images/captures.png)

| Control | What it does |
| --- | --- |
| **The list** | **No amp (the channel is silent)**, then every capture in Krunch's captures folder. Click one to play it at once on that channel (it loads in the background), so you can play and compare; the one playing is lit. |
| **IMPORT** | Opens a file dialog: the `.nam` captures you choose are copied into the captures folder. You can also drop files on the picker. |
| **CLOSE** | Closes the picker. |

**Getting captures in.** Three ways, all into Krunch's captures folder, `~/.local/share/krunch/captures`:

- **IMPORT** in the picker, or drop the files on it;
- copy them into the folder yourself (files that land there show up by themselves);
- from your phone or another computer, **Krate Remote**: **Files ▸ Places ▸ Amp captures (Krunch)**, then upload or drop them.

**Amp-only or full rig?** An amp-only capture (the amp into a load, no speaker) wants a cabinet after it: keep the channel's **CABINET** **ON** and choose IRs. A full-rig capture already has a miked cabinet in it (look for "cab", "mic" or "full rig" in its name or description): set the channel's **CABINET** to **OFF**, or you hear two cabinets, muffled and boxy.

Captures play at a common loudness (when the file says how loud it is), so switching between them doesn't jump. A channel's capture is kept with it, in presets and with songs' sounds.

## The playing channel

The box in the middle of the plate shows the playing channel's controls, its border in the channel's colour, its capture's name at the top. Every channel has its own; every knob at noon plays the capture as it was captured.

| Control | What it does |
| --- | --- |
| **GAIN** | How hard you drive the capture, −12 … +12 dB (noon: as it was captured). Up, more distortion and compression, as far as the captured amp's own gain setting allows; down, it cleans up. |
| **BASS** | A shelf at 120 Hz after the capture, ±12 dB (noon: flat). |
| **MID** | A peak at 700 Hz, ±10 dB: push it to cut through a band, scoop it for a heavier rhythm. |
| **TREBLE** | A shelf at 2.5 kHz, ±12 dB. |
| **PRESENCE** | A shelf at 5 kHz, ±8 dB: bite and air, or a softer edge. |
| **VOLUME** | The channel's own level, −40 … +6 dB (all the way down: silent). Use it to balance the channels, so switching from rhythm to lead jumps by as much as you want. |
| **GATE** | The noise gate, before the amp: **Off**, or a threshold of −80 … −20 dB. Turn it up until the hiss and hum stop between notes, but not so far that it cuts your notes' tails. Out of the box it's on for channels 3 and 4, where high-gain captures usually go. |
| **LOOP** (**FX LOOP**) | The channel's effects loop on or off (lit: on). On, the pedals placed in **LOOP** play between the amp and the cabinet; the line under it counts them. Learnable (it switches the playing channel's loop). |

## Cabinet

Each channel has its own cabinet: two IR slots, **A** and **B**, mixed, or Krunch's built-in speaker when neither is chosen, or none at all.

| Control | What it does |
| --- | --- |
| **CABINET** **ON** / **OFF** | The channel's cabinet on or off. **OFF**: no IR and no speaker, the amp straight out ("bypassed" shows), for a full-rig capture with its cabinet in it. Learnable (it switches the playing channel's cabinet). |
| **A**, **B** | The channel's two IRs. Click one to open the IR library and choose. **A** with no IR is the **Built-in speaker**; **B** can be **None**. |
| **CAB MIX** | The balance between A and B, from **A 100 / B 0** to **A 0 / B 100** (with both IRs chosen). Two different cabinets or mics blended make a fuller sound than either. |
| **SPACE** | Stereo spread between the two cabinets, 0 … 10: at 0 both play in the centre; turning up moves A to the left and B to the right, hard left and right at 10. With one IR or none it does nothing. |
| **IRs in the library** | How many IRs the library holds. |

**The IR library** (click **A** or **B**):

| Control | What it does |
| --- | --- |
| **Search** | Filters the list by pack, folder or name. |
| **The list** | Click an IR to play it at once, so you can compare while you play; double-click to choose it and close. **STEREO** and its length show on the right. |
| **★** | Marks a favourite: favourites are listed first. |
| **IMPORT FILES…** | Adds WAV files or zip packs to the library. |
| **IMPORT FOLDER…** | Adds a whole folder (WAVs and zips, with its subfolders). |
| **DONE** | Closes the library. |

You can also drop IR files, folders or packs anywhere on the amp. Each IR is copied into the library once (the same file is never added twice) and played at a common loudness, so switching IRs doesn't jump. A stereo IR makes the amp stereo.

## The pedalboard

Under the amp: up to eight pedals, left to right. Each is placed **FRONT** (in front of the amp, before the gate) or **LOOP** (in the channel's effects loop, between the amp and the cabinet, played while the channel's **LOOP** is on). Each group plays in the board's order, as the line at the top says: guitar → front pedals → amp → loop pedals → cabinet. Drives and fuzzes belong in front; time effects (delay, reverb, chorus) in the loop, after the distortion, so the echoes stay clean. A new pedal goes where its kind belongs.

![The pedalboard](images/pedals.png)

| Control | What it does |
| --- | --- |
| **+** (**ADD PEDAL**) | Adds a pedal at the end of the board, from a menu of all 15. |
| **The footswitch** | Turns the pedal on and off; its light shows it's on. Switching crossfades, so it never clicks. Learnable by its place on the board (pedal 1 … 8, the number under it). |
| **FRONT** / **LOOP** | Puts the pedal in front of the amp or in its effects loop. |
| **×** | Takes the pedal off the board (it fades out first). |
| **Drag** | Drag a pedal by its body to move it along the board. |

The board scrolls sideways once its pedals are wider than it. It's kept in `~/.config/krunch/pedals.json`, and in presets.

## Pedals: drive, fuzz and dynamics

| Pedal | Knobs | What it does |
| --- | --- | --- |
| **Compressor** | **SUSTAIN**, **ATTACK**, **LEVEL**, **BLEND** | Evens your playing out and lengthens notes. SUSTAIN: more squash (the level made up, so more isn't quieter); ATTACK: how fast it grabs, 0.5 … 50 ms (slower lets the pick through); BLEND: how much of the compressed sound against the dry (keep some dry for the pick's attack). Best first on the board. |
| **Green Drive** | **DRIVE**, **TONE**, **LEVEL** | The classic green mid-boost overdrive: the bass stays tight, the mids push. In front of a crunchy capture it tightens and sharpens the distortion; in front of a clean one, a smooth overdrive. LEVEL −24 … +12 dB. |
| **Round Fuzz** | **FUZZ**, **VOLUME**, **BIAS**, **TYPE** | The classic two-transistor round fuzz. FUZZ cleans up as you pick softer or roll the guitar's volume back. BIAS off centre makes quiet notes sputter (the dying-battery sound). TYPE: **Germanium** (softer, warmer) or **Silicon** (more gain, brighter). |
| **5-Band EQ** | **120**, **360**, **800**, **2K**, **4.5K**, **LEVEL** | Sliders at guitar-centred frequencies (Hz), ±15 dB, flat in the middle; LEVEL (the orange slider) ±12 dB. In front it shapes what drives the amp; in the loop, the amp's finished tone. |
| **10-Band EQ** | **31** … **16K** (octaves), **LEVEL** | Ten octave-wide sliders, ±12 dB; LEVEL ±12 dB. A double-wide case. |

## Pedals: modulation

| Pedal | Knobs | What it does |
| --- | --- | --- |
| **Phaser** | **SPEED**, **MODE** | The classic one-knob four-stage phaser: two notches moving through the low mids. MODE: **Smooth**, or **Deep** (resonant, more vocal). |
| **Flanger** | **RATE**, **RANGE**, **COLOR**, **MATRIX** | The jet-plane sweep. RANGE sets how far it sweeps; COLOR adds feedback for a metallic, resonant sound. MATRIX stops the sweep: RANGE then sets a fixed comb by hand. |
| **Vibe** | **SPEED**, **INTENSITY**, **MODE** | The classic lopsided, throbbing photocell swirl. MODE: **Chorus** (mixed with the guitar) or **Vibrato** (a pitch wobble alone). |
| **Chorus** | **RATE**, **DEPTH**, **MIX** | A copy of the guitar slightly detuned and swept: shimmer and width. |
| **Tremolo** | **RATE**, **DEPTH**, **SHAPE**, **LEVEL** | The volume pulsing, as an amp's tremolo. SHAPE from a smooth sine to a choppy square; DEPTH from a gentle pulse to full chop. Starts in the loop. |

## Pedals: time and space

| Pedal | Knobs | What it does |
| --- | --- | --- |
| **Delay** | **TIME**, **REPEATS**, **TONE**, **MIX** | Echoes that darken and soften as they go, 40 ms … 1.5 s; even full REPEATS never runs away. Turning TIME while it plays bends the pitch, as a tape delay does. |
| **Drum Echo** | **TIME**, **HEADS**, **SWELL**, **TONE**, **MIX** | The old multi-head magnetic drum echo: four heads at 1, 2, 3 and 4 times TIME. HEADS picks which play (1, 1+2, 1+3, All); SWELL feeds them back into a rhythmic swell. |
| **Reverb** | **DECAY**, **TONE**, **MIX** | A room to a hall, 0.4 … 8 s. |
| **Dual Head** | **DELAY**, **TONE**, **BALANCE** | Splits the amp's sound into two amps set apart: the left is the amp, the right a copy **DELAY** later (0 … 40 ms), voiced by **TONE** (darker … brighter) and at **BALANCE** (−12 … 0 dB). A few milliseconds make it wide; 20–40 ms sound like two amps across a room. With **SPACE** up and two IRs, each amp has its own cabinet (A left, B right). It works at the amp's output wherever it sits on the board (in the loop, it follows the channel's **LOOP**). |

## Pedals: signature rigs

A whole rig for one well-known sound in one pedal, each part brought in by its own knob (0 takes it out).

| Pedal | Knobs | What it does |
| --- | --- | --- |
| **Prism** | **FUZZ**, **VIBE**, **ECHO**, **TIME**, **SPACE**, **TONE**, **LEVEL** | The early-70s spacey singing lead and clean sound: a round fuzz, a slow vibe swirl, a multi-head drum echo (TIME sets its first head) and plenty of reverb (SPACE). Best in **FRONT** of a clean capture with lots of headroom. |

## MIDI and MIDI Learn

Krunch plays no MIDI notes, but its controls can be learned: press **LEARN** in Krate's top bar, click a knob or button on Krunch, then move a knob or press a button (or a footswitch) on your controller; they're linked.

- **Knobs**: every knob on the plate (**INPUT**, **LEVEL**, the channel's knobs, **CAB MIX**, **SPACE**).
- **Buttons**: **CHANNEL** 1–4, the playing channel's **LOOP** and **CABINET**, the pedals' footswitches by place (pedal 1 … 8), and the presets' **◂ ▸**. With a foot controller that's a whole floor rig: channels, loop, pedals and presets at your feet.

Every knob can also be automated from Krate's arranger.

## Tips

- **Hearing a second, dry guitar?** An armed audio track in Krate with **MONITOR** on **Auto** (or **On**) plays its input straight through, so you hear your dry guitar next to Krunch's. Disarm the track (or set its MONITOR to Off, or its source to Krunch's output if you want to record the amp).
- **No sound, or the wrong guitar?** Set Krunch's **INPUT** to the interface input your guitar is plugged into, and check that input under **AUDIO** in the top bar. The grille says **GUITAR** when it's connected. If the channel button says **NO AMP**, the channel has no capture: **CHOOSE AMP ▾**.
- **Muffled, boxy capture?** It's probably a full-rig capture: set **CABINET** to **OFF**.
- **Hiss between notes**: turn **GATE** up until it stops, on the high-gain channels especially.
- **Too much fizz** with high gain: lower **PRESENCE** and **TREBLE**, try another IR, or put a Green Drive in front with its DRIVE low and LEVEL up to tighten the amp instead of more GAIN.
- **Not enough gain?** **GAIN** only goes as far as the captured amp's own setting allows: pick a capture made with more gain, or put a drive pedal in front.
- **Rhythm and lead**: put a capture on a lead channel, set its **VOLUME** a few dB over the rhythm channel's, learn the **CHANNEL** buttons to a foot controller, and save the rig as a preset.
- **One preset per song**: when songs need different amps, save a preset for each and step through them with **◂ ▸** (learned to a footswitch) between songs.
- **Wide and big**: two different IRs on **A** and **B**, **SPACE** up, and a **Dual Head** at 10–20 ms.
