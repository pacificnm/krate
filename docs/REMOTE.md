# Krate Remote

A separate app, run on another computer, that manages a Krate player over the network. It changes the player's settings, sends files there and back, reads its crash reports and log, and restarts Krate. It logs in over SSH, just as you would from a terminal, so nothing new runs or listens on the player.

Start it from the menu (**Krate Remote**) or with `krate-remote`. It's in the same package as Krate.

## Connecting

| | |
| --- | --- |
| **Player** | The player's name on the network (`krate.local`, the hostname plus `.local`) or its address. |
| **User** | The user Krate runs as there (the kiosk's: `krate-kiosk@<user>`). |
| **Port** | SSH's port: 22 unless the player says otherwise. |

**CONNECT**. If the player asks for a password, a field appears for it. Once you're in, **SET UP KEY LOGIN** makes a key for this app (`~/.ssh/krate-remote`, on this computer) and adds it to the player's `~/.ssh/authorized_keys`. After that, no password is needed. A key you already use with ssh (`~/.ssh/config`, your agent) works too.

The first connection to a player accepts its identity (its host key). A player whose identity later changes (reinstalled, say) is refused until you remove the old one: `ssh-keygen -R krate.local`.

What it needs:
- this computer: `ssh` (the openssh-client package);
- the player: `openssh-server`, as set up in [PLAYER.md](PLAYER.md).

The top bar shows the player's name, its Krate version, and whether Krate is running (in the kiosk, on a desktop, or not at all).

## Settings

These are the settings from Krate's own ⚙ Settings screen ([SETTINGS.md](SETTINGS.md)). They are kept in the player's `~/.config/krate/settings.json`, with the same rules:

- **Folders**: **CHANGE…** asks for a path on the player. **FOLDER OPEN IN FILES** fills in the folder you're looking at in Files.
  - **MOVE MY FILES THERE** moves your files to the new folder at Krate's next start. Krate only moves them into a folder that's new or empty, and Krate Remote checks that before saving.
  - **USE WHAT'S THERE** makes Krate use the new folder as it is.
  - **CANCEL** takes back a change that's still waiting. **DEFAULT** goes back to the default folder.
  - Exports change at once.
  - The other folders wait for Krate to start again: **RESTART KRATE NOW**.
- **Plugin folders**, **MIDI inputs**, **Program Change** and **checking for updates**: Krate applies these at once, while it runs. It reads `settings.json` again whenever the file changes.

Each change re-reads the file first, then writes it whole, so a change made on the player in the meantime isn't lost. If the file there can't be read, Krate Remote doesn't write over it.

Audio, your MIDI keyboard's keys and the instruments' own settings are set on the player. Installing an update is done there too (⚙ ▸ Updates), because it needs whoever sits at the player.

## Files

**PLACES** lists the player's home folder, **Sounds** (Krate's sounds folder: what you put there is in the Sounds library by itself), Krate's folders (recordings, song library, instruments' sounds, exports, Krate's data) and the plugin folders. The path field takes any folder.

| | |
| --- | --- |
| **UPLOAD FILES…**, **UPLOAD FOLDER…**, or drop files from your file manager | Sends them into the folder that's open. A folder goes with everything in it. Names already there are asked about first. |
| **DOWNLOAD** (or a double click on a file) | Brings the selected files and folders to this computer's Downloads folder (**CHANGE…** picks another). A name taken here becomes `name (2)`. |
| **NEW FOLDER**, **RENAME**, **DELETE** | As they say. **RENAME** never writes over another name. **DELETE** asks first and takes folders with everything in them. |

A click selects a file, Ctrl+click adds to the selection, Shift+click selects a range, and a double click opens a folder.

Files go one at a time, each with its progress, and **CANCEL ALL** stops them. A file arrives under a temporary name and is renamed only once it's whole and the right size. A transfer that's cut off leaves nothing half-written under the real name, at either end.

What goes where:

| | |
| --- | --- |
| Samples and sample packs | **Sounds**, the first of the Places: Krate's sounds folder, always in the Sounds library. What lands there shows up in the library by itself a moment later (folders and all). Anywhere else: on the player **Sounds ▸ ADD FOLDER** (or **RESCAN**). |
| Klang's or Kessel's samples | Their `samples` folders (under Instruments' sounds) are in the Sounds library too, as are the files you send there. |
| CLAP plugins | `~/.clap`, or a folder added under Settings ▸ Plugin folders. |
| Recordings and exports | In their folders. Download them to mix or keep elsewhere. |
| Songs (`.gp`, `.mid`) | Any folder, then open them in Krate's **Song** screen, which adds them to the library. |

## Crash reports and log

The player's crash reports ([CRASHES.md](CRASHES.md)), newest first, and Krate's log for this session and the one before. A click reads them. The end of a long log is shown, up to its last megabyte.

## Restarting Krate

**RESTART KRATE** quits Krate on the player, as if it were stopped there (it saves as usual). The kiosk then starts it again within a few seconds, and any waiting folder moves happen then. If Krate runs on a desktop rather than in the kiosk, there's nothing to start it again, so Krate Remote says so and leaves it alone.

## How

- `apps/krate-remote/Shell`: the system's `ssh` with one connection kept open (`ControlMaster`). Every command runs under `sh` there, and every path is quoted for it.
  - When ssh wants a password, it runs Krate Remote as its askpass helper. The helper answers password prompts only.
  - `KRATE_REMOTE_SSH` swaps ssh for a stand-in (the tests use one that runs the commands locally).
- `RemoteSettings`: the settings file's changes, with Krate's own rules. The folders' defaults are worked out from the player's `XDG_DATA_HOME` and Music folder (`krate::locations::Roots`).
- `RemoteController` (`Remote` in QML): connecting, the player's state, settings, restarting, crash reports, key setup.
- `RemoteFiles` (`Files`): listings (`find -printf`, NUL-separated) and transfers. Each transfer is streamed through its own ssh: `cat` into a temporary file, the size checked, then a rename.
- On Krate's side, `SettingsController` watches `settings.json` and reads it again when something else changes it.

Tests: `tst_remote` uses a scratch "player" and a stand-in ssh. It covers awkward names quoted through the shell, settings changes (and an unreadable file left alone), a refused move, crash reports, files sent both ways (a 3 MB file byte for byte, folders with what they hold), renaming, deleting, a short upload never landing, and the askpass helper. `tst_settings` covers Krate taking a changed file while it runs.
