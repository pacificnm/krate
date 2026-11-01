# Settings

The **⚙** tab (the last one; **Ctrl+,**, or a learned button: `rack:settings`) holds everything that isn't an instrument: where Krate keeps things, audio, your MIDI keyboard and inputs, updates, crash reports, and where every file is. A red dot on ⚙: a restart is waiting, or an update is out.

## Folders

| Folder | Holds | Default |
| --- | --- | --- |
| Recordings | the audio tracks' takes | `~/.local/share/krate/recordings` |
| Song library | the songs you've added and the library's database | `~/.local/share/krate/library` |
| Instruments' sounds | each instrument's patterns, patches, kits, samples and cabinets, in `<folder>/<instrument>` (`korrode`, `kessel2`, ...) | `~/.local/share` |
| Exports | EXPORT's WAV files | `~/Music/Krate` |
| Krate's data | sequences, the sound library's index, crash reports, logs (and recordings and songs while they're at their defaults) | `~/.local/share/krate` |

**CHOOSE…** picks a folder, **DEFAULT** goes back. Then:

- **MOVE MY FILES THERE**: at the next start, before anything opens them, what's in the old folder goes to the new one (renamed if it's on the same drive, otherwise copied, checked file by file and the old one removed), and Krate uses it. Only into a folder that's new or empty: Krate never mixes your files into someone else's. If it can't (the drive's full, the folder isn't empty), nothing moves and the folder stays as it was; the Folders section says what happened at that start.
- **USE WHAT'S THERE**: at the next start Krate uses the new folder as it is (a library on another drive, say); your files stay where they were.
- Exports change at once (nothing to move).
- A change waits for Krate to start again: **RESTART KRATE NOW** (on the player, the kiosk starts it again in a few seconds). **CANCEL** on a folder takes it back.
- Instruments' sounds: only Krate's instrument folders move, all of them or none. The instruments' own apps (Korrode, Kessel...) use the same folder, so they keep sharing sounds and patterns with the rack.

Plugin folders: CLAP plugins are looked for in the folders you add, then the standard ones (`~/.clap`, `/usr/lib/clap`, ...); a change looks again at once. The sound library's folders are on the Sounds screen.

## Audio, MIDI and keys

- **Audio**: the output, the audio interface's inputs, the buffer size (the same as the AUDIO popup).
- **MIDI inputs**: every MIDI device plugged in (the default: two keyboards play together, told apart by channel), or only those whose name matches what you type.
- **Program Change** n focuses the nth instrument in the rack, or (off) goes to the instrument as a program change.
- **Your MIDI keyboard**: its keys, first note and octave range (the same as the KEYS popup).

## Updates, crash reports, about

- **Updates** ([UPDATES.md](UPDATES.md)): the version you have, checking by itself (on or off), CHECK NOW, UPDATE TO.
- **Crash reports** ([CRASHES.md](CRASHES.md)): how many and the last, VIEW REPORTS.
- **About**: where the settings and every other file are.

## The file

`~/.config/krate/settings.json` (fixed: it says where the rest is):

```json
{
    "version": 1,
    "folders": { "recordings": "/mnt/big/takes" },
    "pending": { "library": { "path": "/mnt/big/songs", "move": true } },
    "pluginFolders": ["/opt/clap"],
    "midiInputs": "",
    "programChangeFocus": true,
    "autoCheckUpdates": true
}
```

A folder missing from `folders` (or "") is its default; `pending` waits for the next start. Code: `shared/app/Locations` (where each folder is, moving one: Krate and the instruments' apps), `apps/krate/SettingsController` (the `Settings` QML singleton, the start's moves), `qml/SettingsScreen.qml`, `AudioSettings.qml` and `KeysSettings.qml` (shared with the popups). Tests: `tst_settings`.
