# Example: OpenMPC Groove (Melodee, large screen)

A large-screen edition of Melodee (by Kerem Kiliç, Ellic Studio; AGPL-3.0) running on the Live III through this
launcher: 1280x800 frame, Mixer, Step edit and Patterns pages, touch, pad and step lights.

It lives in its own repo and is not copied here. What it shows about this platform:

- sample-base `live3` as the hardware layer; Melodee on top.
- The app is installed with `./install.sh root@HOST melodee path/to/melodee`.
- Touch handled on the MIDI thread, large pages registered in the frame, lights driven from the surface layer.

Credit and licence: keep Kerem Kiliç's (Ellic Studio) copyright headers; replace the boot art before any public release.
