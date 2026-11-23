# How it works

An MPC standalone is a Linux computer. In Developer Mode (SHIFT + MENU, then Developer) it gives a root shell.
Only one program can own the display, the sound card and the pad MIDI at a time, so MPC (`acvs.service` on the
Live III) has to stop before your app starts.

| Part | With MPC stopped (Live III) |
| --- | --- |
| Stop / start MPC | `systemctl stop acvs` / `start acvs`. Addins and drop-ins in `/run` stay. |
| Screen | `/dev/fb0` shows nothing; use DRM (`/dev/dri/card0`, connector DSI-1, 800x1280 portrait). Landscape (x, y) is drawn at portrait (y, 1279 - x). |
| Touch | ILI210x evdev device, found by name. Raw 0..2047; landscape X = (2047 - raw_y) * 1280 / 2048, Y = raw_x * 800 / 2048. |
| Sound | ALSA card `hw:III`, 30 out / 26 in, S32_LE, 44.1 kHz only. No power-up sequence needed. |
| Pads and buttons in | MIDI on the Public port: pads ch 10 notes + poly aftertouch, buttons and step keys ch 1 notes, Q-Links CC 16-19, wheel CC 100. |
| Lights out | Start-up SysEx replay, a 3 s heartbeat, then pad/step RGB SysEx and button CCs. See [surface/live3.md](../surface/live3.md). |
| Remote control | `mpc-addin-remote` is loaded into the app too: screenshots, taps, buttons on port 6720 (local), reached by an SSH tunnel. |

Details and measurements: [findings.md](findings.md).

## The launcher's job

`mpc-launch start APP SECONDS` runs the app's self-test while MPC is still up, then starts the app as a
transient systemd service whose pre-start stops `acvs` and whose post-stop starts it again. Because systemd does
the second step, MPC returns on exit, crash, kill or time limit. See [launcher/README.md](../launcher/README.md).
