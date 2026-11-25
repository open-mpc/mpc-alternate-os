# What you get to build an app

| Piece | Where | Licence | State |
| --- | --- | --- | --- |
| Launcher: install, start with a time limit, self-test first, MPC always restored, remote add-in preloaded | `launcher/` | MIT | Used daily on the Live III |
| Minimal app (DRM screen, life cycle) | `template/` | MIT | Runs on the Live III |
| Hardware layer: screen, touch, sound, pads, buttons, lights, step keys, Q-Links | sample-base, branch `live3` | AGPL-3.0 | Live III done; Force not yet |
| Build image for arm64 | sample-base `tools/Dockerfile` (Debian bookworm, under QEMU) | AGPL-3.0 | Used for every build |
| Example of a full app | `examples/melodee-groove/` (description) | AGPL-3.0 | Phase 3 of its own plan |
| Remote control: screenshots, taps, button presses, MCP | mpc-addin-remote (preloaded by the launcher) | | Works with every app |

Not here yet: a documented app API for sample-base, a shared UI kit for the 1280 x 800 screen, Force support,
packaging apps for a catalog.

Choosing a base: start from `template/` for a permissive app that brings its own drawing and input; start from
sample-base's `starter` for everything ready-made, under the AGPL.
