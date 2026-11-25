# App template

The smallest app the launcher can run: `src/hello.c` (MIT, plain C, libdrm only). It takes the screen over DRM, draws
a moving band, and leaves on SIGTERM, SIGINT or when `--seconds` runs out. It has the self-test the launcher runs
before it stops MPC. Copy this folder to start your own app.

```sh
./build.sh                                        # bin/hello for 64-bit ARM, in Docker
cp app.conf ../launcher/apps/hello.conf           # the app's settings, as install.sh reads them
../launcher/install.sh HOST hello bin/hello       # copy it to the device (stops nothing)
ssh HOST /data/mpc-launcher/mpc-launch start hello 30   # 30 seconds, then MPC comes back
```

What it shows, and what is left to you:

| | In the template | Next step |
| --- | --- | --- |
| Screen | DRM dumb buffer, full redraw, `drmModeDirtyFB` | Draw only what changed; one DirtyFB per frame with all rectangles (each call is costly) |
| Life cycle | self-test, `--seconds`, SIGTERM, screen given back | Keep the self-test honest: load what your app loads |
| Touch, pads, buttons, lights, sound | not here | Touch: the ILI210x evdev; pads and buttons: the Public raw MIDI port; sound: ALSA `hw:III` (see `../surface/live3.md` and `../docs/findings.md`) |

For all of that ready-made, use sample-base (branch `live3`): screen, touch, sound, pads, buttons, lights, step keys
and Q-Links on the Live III, with its own `starter` app. Note its licence: sample-base is AGPL-3.0, so an app built on
it is AGPL too. This template and the launcher are MIT.
