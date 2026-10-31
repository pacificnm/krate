# Packaging

Krate installs as one Debian package, `krate`, with all eight apps: **Krate** (the rack) and each instrument on its own (Korrode, Kessel, Kalliope, Krimson, Kobalt, Klang, Konnekt). Each has a menu entry (KDE: *Multimedia*) and an icon.

## Build the .deb

```sh
packaging/build-deb.sh
```

It configures a clean release build in `build-deb/` (your `build/` is left alone), builds it, runs the tests, and writes `dist/krate_<version>_amd64.deb`.

## Install, update, remove

```sh
sudo apt install ./dist/krate_*.deb   # installs (and pulls in what it needs)
sudo apt remove krate                 # removes the apps; your sounds and songs stay
```

Installing a newer `.deb` the same way updates it.

## What goes where

| Path | What |
| --- | --- |
| `/usr/bin/krate`, `/usr/bin/korrode`, ... | The apps |
| `/usr/share/applications/<app>.desktop` | Menu entries: `pw-jack <app>`, so they play through PipeWire |
| `/usr/share/icons/hicolor/scalable/apps/<app>.svg` | Icons |
| `/usr/share/doc/krate/` | Licence and README |

Your own data is never in the package: patches, kits, patterns, songs and settings live in `~/.local/share/<app>` and `~/.config/<app>`, and are kept when the package is removed or updated.

## Dependencies

The Qt 6 and JACK libraries are found by `dpkg-shlibdeps`. What Qt loads at run time isn't found that way: its QML modules (QtQuick, Controls, Dialogs, Shapes and what they use), the platform plugins and the SQLite driver (the song and IR libraries), with `pipewire-jack` (for `pw-jack`), are listed in `packaging/CMakeLists.txt`; `qt6-wayland` is recommended for Wayland sessions.

## Builds and releases (GitHub Actions)

`.github/workflows/build.yml` runs on every push to `main` and every pull request: a release build on Debian 13 (Qt 6.8), every test (offscreen), and the `.deb`, attached to the run as the `krate-deb` artifact (the run's page, *Artifacts*).

A release is a tag:

```sh
git tag v0.2.0          # the version: the package becomes krate_0.2.0_amd64.deb
git push origin v0.2.0
```

The same build runs, and if it passes a GitHub Release named *Krate 0.2.0* is published with the `.deb` attached and notes generated from the commits since the last release. Tags look like `v<major>.<minor>.<patch>`.
