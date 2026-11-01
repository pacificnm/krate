# Krate: a dedicated player

A small PC that boots straight into Krate, full screen, with your audio interface and MIDI keyboard: nothing else on screen, nothing else running. It uses **cage**, a kiosk compositor that shows one app and nothing else, so there's no desktop to install.

Written for Debian 13 (trixie) on a small PC with a USB audio interface and a USB MIDI keyboard. Both are class-compliant: no drivers.

## 1. Install Debian

From the netinst image:

1. Make a user (say `jaimie`) and give it `sudo` (leave the root password empty and the installer does).
2. At **Software selection**, untick every desktop and **GNOME**/**KDE**; tick only **SSH server** and **standard system utilities**.
3. Finish, reboot, log in on the console (or over SSH).

## 2. Install Krate and cage

Copy the Krate package over (`scp dist/krate_*.deb jaimie@player:`), then on the player:

```
sudo apt update
sudo apt install cage qt6-wayland pipewire-jack wireplumber ./krate_*.deb
```

PipeWire comes with them; it runs in your user session, started with the kiosk's login.

## 3. Low-latency audio

Real-time priority for the audio (Debian's PipeWire gives it to the `pipewire` group):

```
sudo usermod -aG pipewire jaimie
```

Keep the CPU at full speed, threaded interrupts for steadier timing, and USB devices never powered down (so the interface can't drop out). Edit `/etc/default/grub` and add to `GRUB_CMDLINE_LINUX_DEFAULT`:

```
threadirqs usbcore.autosuspend=-1 cpufreq.default_governor=performance
```

then `sudo update-grub`. No sleeping, ever:

```
sudo systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

Debian's standard kernel is fine; a real-time kernel isn't needed.

## 4. Start Krate at boot

The package installs a service for it, off until you turn it on, for your user:

```
sudo systemctl enable krate-kiosk@jaimie
sudo systemctl set-default graphical.target
sudo reboot
```

The PC boots into Krate, full screen, logged in as you. If Krate quits or crashes it starts again in two seconds.

## 5. First start

In Krate:

- **AUDIO** (top right): your interface as the output; its inputs for the guitar (Krunch) and audio tracks; buffer **128** (about 3 ms at 48 kHz) or **256** if you hear crackles. Krate remembers it.
- The keyboard connects by itself (`MIDI in` in the status line).
- Add the instruments and tracks you play from **+**, set up the mixer: it's all kept.

## Living with it

| | |
| --- | --- |
| A text console | **Ctrl+Alt+F2** (log in there); **Ctrl+Alt+F7** back to Krate. |
| Over the network | `ssh jaimie@player`. |
| Krate's log | `journalctl -u krate-kiosk@jaimie` |
| Stop / start Krate | `sudo systemctl stop krate-kiosk@jaimie`, `start`, `restart` |
| A new Krate | Copy the new `.deb`, `sudo apt install ./krate_*.deb`, `sudo systemctl restart krate-kiosk@jaimie`. Your sounds, songs and settings stay. |
| Your sounds | Copy them over (`rsync -a ~/Samples jaimie@player:`), then **Sounds** → **ADD FOLDER**. |
| Back to a normal boot | `sudo systemctl disable krate-kiosk@jaimie` |
| Plugin windows | A CLAP plugin's editor (**EDITOR** on its slot) fills the screen while it's open; close it to get back to Krate. |

## Troubleshooting

| | |
| --- | --- |
| Black screen, back to a login prompt | `journalctl -u krate-kiosk@jaimie -b`: cage needs the PC's graphics (built-in graphics work); `cage` and `qt6-wayland` must be installed. |
| No sound | On the console, as your user: `wpctl status` lists the interface; in Krate's **AUDIO** choose it as the output. |
| Crackles | A larger buffer (256); check `groups` lists `pipewire` (log out and in, or reboot, after adding it). |
| Keyboard not seen | `aconnect -l` (or `pw-jack jack_lsp | grep -i midi`) should list it; unplug and plug in again. |

## How it's set up

`krate-kiosk@.service` (in `/usr/lib/systemd/system`) runs `cage -d -s -- krate-kiosk` on console 7 as the user named after `@`, with a full login session (`/etc/pam.d/krate-kiosk`: logind, so cage gets the screen and the keyboard, and the user's systemd, so PipeWire starts). `krate-kiosk` starts Krate through PipeWire's JACK (`pw-jack`), as the menu entries do. cage runs its own XWayland, so plugin editors (X11 windows) open too. Krate starts maximized to the screen; its tabs scroll when the rack holds more instruments than fit.
