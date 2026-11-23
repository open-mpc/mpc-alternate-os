# Structuring an app

An app is a program plus a few lines of settings, installed in its own folder on the device:

```
/data/mpc-launcher/apps/NAME/
  app.conf        TITLE, EXEC, ARGS, SELF_TEST, optional BIND
  bin/NAME
```

Rules every app should follow:

1. **Have a self-test** that runs without the hardware (load libraries, render a little sound) and exits non-zero on
   failure. The launcher runs it before touching MPC.
2. **Open the hardware yourself**: DRM for the screen, the touch evdev by name, `hw:III` for sound, the Public MIDI
   port for pads and buttons. Do the light start-up replay before lighting anything.
3. **Exit cleanly on SIGTERM** and honour `--seconds`/the time limit; the launcher restores MPC either way.
4. **Carry the remote add-in.** The launcher preloads it; keep your button handling reachable from its button presses
   so the app can be driven from a computer.
5. **Keep user data off the app folder.** Use `BIND` to give the app a private card folder.
6. **Do not edit MPC's files.** Nothing here changes MPC; an app only runs while MPC is stopped.

Reusable hardware layer: sample-base (branch `live3`) already has the Live III screen, sound, touch, pads and lights.
