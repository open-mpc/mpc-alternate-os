# mpc-launcher

mpc-launcher runs your own software on an Akai MPC standalone device in place of the MPC app, and always gives the
device back to MPC afterwards. While your software runs, you can still watch and drive the device from a computer,
the same way as when MPC is running.

It is built and tested on the MPC Live III (MPC OS 3.10.1 in Developer Mode).

## What it is for

An MPC standalone device is a Linux computer with a screen, a touchscreen, pads, knobs and a sound card. Normally
one program uses all of that: the MPC app. Developer Mode in MPC OS 3.10.1 gives a root shell, so other programs
can use the hardware too, but only one program can own the screen, the sound card and the pads at a time. To run
something else, MPC has to stop first.

That is where things go wrong without a launcher:

- If your program crashes, the device is left with a black screen and no MPC until someone reboots it.
- If you are not at the device, you cannot see what your program is doing. MPC's remote addin stops with MPC.
- Every app needs the same steps: stop MPC, start the app, restart MPC. Each app would write them again, slightly
  differently, and each one could get it wrong.

mpc-launcher does those steps once, for any app. It is the base for running a custom OS on MPC hardware:

- **OpenMPC Groove / Melodee**: the groovebox this was made for.
- **Any sample-base app**: sample-base is the hardware layer Melodee is built on; its starter and demo apps run too.
- **Hardware tests and tools**: a probe, a light-decoding tool, a benchmark, anything that needs the device to
  itself for a while.
- **Other people's apps** later, once more devices are in the table.

## How it works

You install apps into folders on the device, then tell the launcher which one to start:

```
mpc-launch start melodee 600
```

What happens then, in order:

1. **Self-test first, while MPC is still running.** The launcher runs the app's self-test (for sample-base apps:
   load every library, render some sound). If that fails, it stops here and MPC is never touched.
2. **The app becomes a system service.** The launcher creates a temporary systemd service called
   `mpc-launcher-app` with two rules attached:
   - before the app starts: stop the stock MPC service (`acvs` on the Live III);
   - after the app ends, **for any reason**: start the stock MPC service again.

   Because systemd runs the second rule, MPC comes back if the app exits normally, if it crashes, if it is killed,
   if the time limit runs out, or if you run `mpc-launch stop`. The app cannot forget to do it.
3. **The remote addin rides along.** The remote addin (`mpc-addin-remote`) is the same library MPC loads to show its
   screen in a browser and to answer the `mpc` MCP tools. The launcher loads it into the app. Two settings tell the
   addin it is in an app rather than in MPC:
   - `MPC_REMOTE_ADDIN_PROCESS`: the app's program name, so the addin starts in it;
   - `MPC_REMOTE_ADDIN_SURFACE`: which MIDI port the app reads the pads and buttons from, so remote button presses
     reach the app.

   The addin listens on the same port as in MPC (6720, local to the device), so one SSH tunnel works whether MPC or an
   app is running: screenshots, taps, button presses, device status.
4. **Optional private folders.** An app made for an SD card (Melodee expects `/media/<card>/Melodee/...`) can be given
   a folder of the internal drive that it alone sees as its card. This is a mount inside the app's service only:
   MPC and the rest of the system see nothing change, and it is gone when the app stops.
5. **A time limit, if you give one.** `600` above means: back to MPC after 10 minutes at the latest. Useful when
   testing unattended. `0` means no limit.

```
 your computer                          the MPC device
 ─────────────                          ──────────────────────────────────────────────────────
 ssh … mpc-launch start melodee ──────▶ self-test ──fail──▶ stop here, MPC keeps running
                                            │ ok
                                            ▼
                                        stop MPC (acvs) ──▶ run melodee  ◀── remote addin inside
                                                                │             (screen, touch, buttons)
 ssh -L 6721:127.0.0.1:6720  ◀──────────────────────────────────┘
 browser / MCP tools                    melodee ends (exit, crash, time limit, stop)
                                            │
                                            ▼
                                        start MPC again (always)
```

## Starting from the device: the app picker

`mpc-launch menu` starts a **session**: MPC stops once, and the device shows a list of the installed apps.

- **Pick an app:** tap it, turn the wheel and press PLAY, or press the pad with the app's number.
- **Leave an app:** MAIN (on the Live III; it stands in for the MPC Sample's wheel push). You are back in the list.
- **Back to MPC:** the list's last row, or MAIN in the list. MPC starts again.

The picker is a small sample-base app (`--app menu`, sample-base branch `live3`); it lists every folder in
`apps/` with an `app.conf`, by its `TITLE`. The remote addin follows from the picker to the app and back, on the same
port, so a session can be driven remotely too (`press_button PLAY`, `press_button MAIN`).

**From the device, with no computer:** copy `autorun/autorun.sh` to the top folder of a USB stick or SD card. With
Developer Mode on (SHIFT + MENU, then Developer), attaching the drive makes MPC ask
"... contains an autorun script. Do you want to run it now?". Yes runs it as root (through Akai's own
`az01-script-runner`), which opens the picker. This is Akai's documented developer route: nothing in MPC is changed.

Not tried yet with a real drive and the real prompt (needs someone at the device); the script runner itself was
tested on 2026-11-23 and runs a command as root with a normal PATH. Also to find out: whether MPC asks at power-on
when the drive is already in, which would make a stick a "boot into the picker" key.

## On the device

Everything lives in `/data/mpc-launcher`, which survives reboots:

```
/data/mpc-launcher/
  mpc-launch                     the launcher (a shell script)
  autorun.sh                     a copy of the drive script, to copy onto a stick from the device
  menu/sample-base               the app picker
  remote/mpc_remote_addin.so     the remote addin, standalone-apps build
  remote/mpc_remote_addin.conf   its settings (port 6720, local only)
  apps/melodee/app.conf          one folder per app
  apps/melodee/bin/melodee
  apps/starter/app.conf
  apps/starter/bin/sample-base
```

While an app runs: its output is in `/run/mpc-launcher/app.log`, the name of the running app in
`/run/mpc-launcher/app`. `/run` is cleared at reboot.

### An app's settings

`app.conf` is a few lines of shell variables:

```sh
TITLE="Melodee"                                            # the name in the picker (default: the folder name)
EXEC=bin/melodee                                           # the program, inside the app's folder
ARGS="--runtime-dir /run/mpc-launcher/melodee --seconds @SECONDS@"   # its arguments; @SECONDS@ = the time limit
SELF_TEST="--self-test /run/mpc-launcher/self-test.wav"    # run before MPC stops; leave empty for none
BIND="/nvme/MelodeeCard:/media/card"                       # optional: FOLDER:WHERE-THE-APP-SEES-IT, space-separated
```

The examples used here are in `apps/`.

### What it knows about each device

| Device | Stock service | Surface port for the remote | State |
| --- | --- | --- | --- |
| MPC Live III | `acvs.service` | `Public` (where pads and buttons arrive while MPC is stopped) | tested |
| MPC Sample | `ac50.service` | `Private` | in the table, untested |
| Other `acvs` devices (Live II, One, X, Key, Force) | `acvs.service` | `Private` | in the table, untested |

## Commands

From your computer (the Live III is `root@HOST`):

```sh
./install.sh HOST                             # install or update the launcher and the remote addin
./install.sh HOST melodee path/to/melodee     # also install an app (settings from apps/melodee.conf)

ssh HOST /data/mpc-launcher/mpc-launch list            # installed apps
ssh HOST /data/mpc-launcher/mpc-launch menu            # the app picker (a session, until Back to MPC)
ssh HOST /data/mpc-launcher/mpc-launch start APP 600   # run APP, back to MPC within 600 s
ssh HOST /data/mpc-launcher/mpc-launch status          # what runs now, MPC's state, the last log lines
ssh HOST /data/mpc-launcher/mpc-launch stop            # end the app, MPC starts again

ssh -N -L 6721:127.0.0.1:6720 HOST                     # the remote: http://localhost:6721, or the mpc MCP tools
```

`install.sh` never stops anything. It copies new files beside the old ones and then swaps them in, so a running
app's program is never overwritten in place.

## What it does not do (yet)

- **Start with no drive.** The stick route needs a drive. Two ways without one, both needing MPC changes, are
  sketched: an "Apps" tile in MPC's MENU grid (the Mode Slot shim in the Plugin Manager repo hosts such a page, but
  takes only one of MPC's free mode slots today), or a button combination watched by a small addin inside MPC.
- **Other devices, tested.** The Force (32-bit, MockbaMod) and the MPC Sample are in the table but not tried.
- **Lights, fine details.** sample-base (`live3`) brings the Live III's pads, step keys and buttons up and lights them
  (checked by eye: pad and step order are right). MAIN does not light yet: its light message is not known.

## Related

- `mpc-addin-remote`, branch `standalone-apps`: the remote addin with the two settings above.
- sample-base, branch `live3`: sample-base on the Live III (screen, sound, pads).
- Melodee (OpenMPC Groove): Melodee, built on that branch.
