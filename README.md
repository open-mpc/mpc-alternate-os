# mpc-alternate-os

How to run your own full-screen software on an Akai MPC standalone device in place of the MPC app, and get the
device back afterwards.

It is not a firmware replacement. MPC OS (Linux) keeps running; one program owns the screen, pads and sound card
at a time, and this repo is about swapping that program safely. Built and tested on the **MPC Live III, MPC OS
3.10.1, Developer Mode** only. Other devices are in the launcher's table but untested.

> Status: draft, private. It needs root SSH and stops the MPC app. Use at your own risk, and read
> [docs/recovery.md](docs/recovery.md) first.

## Contents

| Path | What |
| --- | --- |
| [docs/how-it-works.md](docs/how-it-works.md) | The hardware seen from Linux: stopping MPC, screen, touch, sound, pads, remote control |
| [docs/app-structure.md](docs/app-structure.md) | What an app must do, the folder layout, and the `app.conf` |
| [docs/recovery.md](docs/recovery.md) | If an app hangs or MPC does not come back |
| [docs/findings.md](docs/findings.md) | Phase 0 results, measured on the device |
| [surface/live3.md](surface/live3.md) | Live III pad, button and light messages (reference) |
| [launcher/](launcher/) | `mpc-launch`: runs an app, always restores MPC, carries the remote add-in |
| [probe/](probe/) | The probe and light-check scripts used to learn all of the above |
| [template/](template/) | The minimum an app needs (notes now, code later) |
| [examples/melodee-groove/](examples/melodee-groove/) | Worked example: OpenMPC Groove, a large-screen Melodee |

## Quick start

```sh
cd launcher
./install.sh root@HOST                   # launcher + remote add-in (stops nothing)
./install.sh root@HOST starter path/to/app-binary
ssh root@HOST /data/mpc-launcher/mpc-launch start starter 120   # back to MPC within 2 minutes
```

## Licence

MIT, except where a folder says otherwise. Apps linked from `examples/` have their own licences (Melodee is AGPL).
