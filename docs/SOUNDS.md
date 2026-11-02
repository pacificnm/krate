# Krate: Sound library

Your sounds in one place: sample packs, drum hits, loops, kept in their folders, browsed by category, auditioned, and put onto Klang's pads or Kessel's drums.

## What it does

Open the **Sounds** tab (next to Arrange; key `O`, learnable).

| | |
| --- | --- |
| **ADD FOLDER** | Points the library at a folder of WAV or MP3 files, at any depth. Nothing is copied: the files are indexed where they are. Add as many folders as you like; ✕ takes one out of the library (the files stay). |
| **RESCAN** | Looks through the folders again: new files come in, deleted ones go. Your categories and favourites are kept. |
| **Categories** | Kick, Snare, Clap, Hi-hat, Cymbal, Tom, Percussion, Bass, Synth, Keys, Vocal, FX, Loop, Other, with how many of each; **All** and **★ Favourites** too. A sound's category is guessed from its file name, then its folders (`BD_02`, `Kicks/Hit 3`, `OpenHat 2`, `SD Rimmy`, `Shaker Fast`, `Sub Bass C1`...; a loop or anything with a BPM is a Loop). The **CATEGORY** buttons set it; yours is kept from then on. |
| **Search** | Finds names and folder names. |
| **Listen** | A click plays a sound once, over Krate's output, whatever the transport is doing; the arrow keys step through the list playing each; Space plays or stops. |
| **★** | Marks a favourite. |
| **SEND TO** | Pick an instrument in the rack that takes sounds (**Klang**, **Kessel**), then click where the sound goes: one of Klang's 16 pads, or one of Kessel's 16 drums. The sound is copied into that instrument's own samples folder, so it keeps working if the original moves. A right click empties a pad, or puts a Kessel drum back on its own synthesized sound. |

On Kessel, a drum playing your sample keeps its knobs: **TUNE** is the sample's pitch (the middle as recorded), **DECAY** fades it (the top: to its end), and its level, pan, sends and place in every pattern stay; **WRITE** keeps the drums' samples in the kit (see Kessel's KITS.md).

## Krate's own folders

These folders are always in the library, listed first. They can't be taken out of it.
- Krate's sounds folder, `~/.local/share/krate/sounds`.
- The instruments' samples folders: Klang's pads and Kessel's drums, `~/.local/share/klang/samples` and `~/.local/share/kessel/samples`, and their copies' if there are any.

The same sound in several places (your folder, Klang's samples, Kessel's) is listed once: files with the same contents are one sound, whatever their names. Anything put in one of them shows up in the library by itself a moment later, subfolders included (a pack keeps its folders). Files deleted from them leave the library.

Krate Remote sends sounds to the sounds folder (**Files ▸ Places ▸ Sounds**, [REMOTE.md](REMOTE.md)), but files sent to an instrument's samples folder show up too. Other folders you add are looked through when you press **RESCAN**.

## Where things are

| | |
| --- | --- |
| `~/.local/share/krate/sounds.db` | The library: folders, sounds, categories, favourites. |
| `~/.local/share/krate/sounds` | Krate's sounds folder: always in the library, watched (as are the instruments' samples folders). |
| Your folders | The sounds themselves, untouched. |
| `~/.local/share/klang/samples`, `~/.local/share/kessel/samples` | Copies of the sounds you sent to Klang or Kessel. |

## How

`SoundIndex` finds WAV and MP3 files, reads a WAV's length from its header (no decoding) and guesses categories from the words in names (camelCase and letters/digits split; two-word names like "bass drum" first). `SoundLibrary` keeps them in SQLite; a folder's scan (on a worker thread) adds new files with a guessed category, keeps what you set, drops files that are gone. Auditioning decodes the sound and hands it to the rack processor's previewer, mixed over the master after the mixer; a sound replaced is freed once the audio thread has moved on. Sending uses one interface every instrument can offer (`InstrumentInstance::sampleTargets`, `assignSample`, `clearSample`): Klang offers its pads, Kessel its drums.

Tests: tst_sounds (categories from realistic names, WAV headers, folders indexed, rescans keeping yours and dropping what's gone), tst_rack (the previewer), kessel tst_voices and tst_kessel (a drum's own sample). Checked live through PipeWire: a pack of 14 sounds categorized as their names say; a kick sent to Kessel's kick and Klang's pad 1 (copied into each); auditioned and finished by itself; Kessel's pattern recorded with its kick hits carrying the sample's 1 kHz and its other drums still synthesized.
