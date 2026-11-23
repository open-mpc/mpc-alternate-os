# OpenMPC Launcher tile

A MENU tile inside MPC that lists the apps installed for [mpc-launch](../launcher/) and starts the one you tap. It
is the "app" counterpart of the Plugin Manager App: the mode slot shim (`modeslot.so`, from the plugin-manager repo)
adds the tile, and `launcher-page` draws the page and handles touches.

Status: first version. The page renders offline; not yet installed or run on a device.

| File | What |
| --- | --- |
| `launcher-page.c` | Page daemon: scans `/data/mpc-launcher/apps/*/app.conf`, draws a list, runs `mpc-launch start APP 0` on a tap |
| `pm-app-start.sh` | Run by the shim at each MPC start (the shim fixes this name); starts the daemon in a systemd unit |
| `addin.manifest` | Addin description for the catalog packager |
| `build.sh` | Builds `launcher-page` (aarch64, Docker) |

Limits for now: the shim serves one page, so this and the Plugin Manager App cannot both be installed until the shim
hosts several (it picks a free slot, but each copy of it hooks the same functions). Touch only (no wheel or Q-Links
in a slot yet), and a plain 5x7 font.
