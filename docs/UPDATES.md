# Releases and updates

## Releases

One version number, in `CMakeLists.txt` (`project(krate VERSION 0.2.0)`): the apps show it, the package carries it, the updater compares it. To release:

```sh
packaging/release.sh 0.3.0
```

It checks you're on `main`, level with GitHub, with nothing uncommitted and the tag free; sets the version, commits *Release 0.3.0*, tags `v0.3.0` and pushes both. GitHub Actions then builds, tests and publishes the release *Krate 0.3.0*: the `.deb`, a `SHA256SUMS` file and notes made from the commits since the last release. A tag that doesn't match `CMakeLists.txt`'s version fails the build, so the numbers can't drift apart.

Numbers: `major.minor.patch`. Bump the patch for fixes (0.2.1), the minor for new features (0.3.0); 1.0.0 when it's ready to call finished.

## Updating from inside Krate

- Krate asks GitHub for the latest release 15 seconds after it starts, then twice a day. When there's a newer one, the top bar says **Krate 0.3.0 is out: AUDIO ▸ UPDATE**.
- **AUDIO** (top right) shows the version you have, **CHECK** (ask now), and **UPDATE TO 0.3.0** with the release's notes. Press it: Krate says what's happening (downloading, installing), and on the player the kiosk starts the new Krate when it's done (about a minute). On a desktop, quit and start Krate again.
- Your sounds, songs, patterns and settings stay: they're in your home folder, not in the package.
- Without a network, or when something fails, it says so in AUDIO and nothing changes.

## How it's safe

Installing needs root, and Krate doesn't run as root. So the package installs:

- `/usr/libexec/krate/krate-update`, run as root by `krate-update.service`. It asks GitHub for the latest release itself (it takes no file from Krate or anyone), downloads the package, and installs it only if it matches the release's `SHA256SUMS`, is named `krate`, and is the release's version and newer than the one installed. It writes what it's doing to `/var/lib/krate/update-status` (Krate shows it), then restarts the kiosk if one runs.
- `/usr/share/polkit-1/rules.d/50-krate-update.rules`: whoever sits at the computer (a local, active session: the kiosk, or your desktop) may start that one service, nothing else. Over SSH it needs root as before: `sudo systemctl start krate-update` (or `su -` then `systemctl start krate-update`).

The package now depends on `curl`, `jq` (the updater's) and `polkitd`.

Trying the updater by hand without installing: `KRATE_UPDATE_DRY_RUN=1 KRATE_UPDATE_STATUS_DIR=/tmp /usr/libexec/krate/krate-update` (add `KRATE_UPDATE_INSTALLED=0.0.1` to pretend an old Krate).

Code: `packaging/update/` (the script, the service, the rule), `apps/krate/UpdateController` (the `Updates` QML singleton), the AUDIO popup in `TransportBar.qml`. Tests: `tst_updates`.

## From 0.1.0

0.1.0 has no updater: install 0.2.0 by hand once (as in [PLAYER.md](PLAYER.md#living-with-it)); after that, UPDATE does it.
