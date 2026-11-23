# Phase 0 findings: Live III with the MPC app stopped

Tested 2026-11-22 and 2026-11-23 on the Live III (MPC OS 3.10.1, developer mode, SSH as root). Tool: `p0probe` + `p0run.sh` in this folder. Each run stops `acvs` and starts it again at the end, even on failure.

| Area | Result | Details |
| --- | --- | --- |
| Stopping MPC | Works | `systemctl stop acvs`; `systemctl start acvs` brings it back. The /run drop-ins (addins, drum pad patch) stay. |
| Sound | Works | Card `hw:III` is the internal USB audio device (09e8:0058): 30 outputs, 26 inputs, S32_LE, 44,100 Hz only. 128-frame periods, buffer 384: 0 xruns in 3 x 4 s. The user heard all three pairs (outputs 0+1, 2+3, 4+5). No codec or amplifier setup needed. |
| Screen | Works through DRM | `/dev/fb0` alone shows nothing once MPC lets go. DRM works: `/dev/dri/card0`, master taken, connector 84 (DSI-1), mode 800x1280@60, CRTC 82, dumb buffer XRGB8888. Landscape (x, y) is drawn at portrait (y, 1279 - x): the red square appeared top left as intended. |
| Touch | Works | ILI210x Touchscreen, found by name (its event number changes between boots). Raw 0..2047 on both axes, raw x along the portrait panel's 800-pixel side. Landscape X = (2047 - raw_y) * 1280 / 2048, Y = raw_x * 800 / 2048: what mpc-addin-remote writes for a tap (measured 2026-11-23: landscape (100, 50) -> raw 128,1886). The first guess here had the axes the other way round; the diagonal drag (160,1934 -> 1908,120) fits both. The kernel drops a repeated position value, and EVIOCGABS keeps no value for MT axes: a reader that starts late reads slot 0 with EVIOCGMTSLOTS. |
| Surface input | Works, no handshake | All on the Public port `hw:0,0,0`: pads = channel 10 notes with poly aftertouch; buttons and step keys = channel 1 notes (step keys seen 0x54..0x68); Q-Links = CC 16..19; wheel = CC 100; other CCs seen 0x21, 0x22, 0x44, 0x77. A few button notes (0x44, 0x49) came on the Private port `hw:0,0,1` in one run. Map details: mpc-addin-remote's learned Live III map. |
| Surface lights | Not tested | MPC writes much more to the ports than it reads; output still to decode. |
| Start route | Found | MPC (`ScriptRunner::DriveAdded`) asks "contains an autorun script. Do you want to run it now?" when a drive with `autorun.sh` is attached in developer mode, then sends the command over UDP 127.0.0.1:8080 to `az01-script-runner`, which runs it as root via `system()`. Asks on each attach; whether it fires at boot for a drive already present is untested. |

What this means for the port: no audio power sequence, no surface handshake, a standard DRM path and a standard touch device. The Live III board in sample-base is mostly tables. Remaining for Phase 0: lights, the autorun prompt at boot, and the sample-base starter app itself.

## Lights: already decoded (2026-10-24, mpc-controllers)

`mpc-controllers examples/force-link` (FINDINGS.md section 17, `spoof/forcespoof.c` bridge, user-verified on the
device) brought the Live III's own controller up without MPC's surface code:

- Start-up: replay `spoof/live3_init.syx` (725 bytes of MPC's captured start-up: `52` challenge, `6C`/`6B` config
  tables, `62 00 01 65`, `41`, `5B`), one sysex at a time, 3 ms apart; then the heartbeat
  `F0 47 00 58 52 00 04 13 75 21 69 F7` every 3 s. Controller id 0x58.
- Pad and step-key RGB: `F0 47 7F 58 65 00 04 idx r g b F7`, idx 0x00-0x4F = 16 pads x 4 quadrants + 16 step
  buttons. The quadrant order was wrong in the bridge's first guess; `map.conf` overrides it.
- Button LEDs: CC on the Private port, value 0..7.
- With MPC running, the inputs come on Private; without the start-up they come on Public (our probe). That the start-up moves them to Private is my guess, untested.

Done in sample-base `live3` (`surface/live3_lights.h`, 0f818ad). **Light check 2026-11-23 (user watching):** the
start-up + heartbeat bring the controller up with MPC stopped; pad 1 lit from 0x00-0x03 and the pads filled in
ascending order (pad N = 4(N-1)..4(N-1)+3, as the code assumes); the step keys filled left to right from 0x40;
the buttons lit for values 0-7 and looked about right. Second and third runs (`lightcheck3.sh`, MPC kept stopped): a pad's lights in index order are
**bottom left, bottom right, top left, top right**. Buttons: SHIFT lights, but **PLAY, REC, STOP, OVERDUB and MAIN
stay dark for every CC value 0-7** (CC numbered like their notes 0x52, 0x49, 0x51, 0x50, 0x34). **Found 2026-11-23** (forcespoof tap,
`tap.conf`, stock MPC with the user pressing the transport; log `probe/transport_capture.log`): the transport
lights on its own CC numbers, not its notes. REC 0x49 -> light 0x65, OVERDUB 0x50 -> 0x54, STOP 0x51 -> 0x55,
PLAY 0x52 -> 0x56 (PLAY START 0x53 -> 0x57 by the pattern, unseen). Values 3 on, 1 off; REC/OVERDUB blink 3/1
with PLAY while recording. MAIN: MPC sends nothing when it is pressed, and CC 0x34 0-7 left it dark: unknown.
Fixed in sample-base `live3` (see git log). Run 2 was spoiled when something on the network
logged in by SSH and started MPC mid-check (10:20:14): keep other tools off the device during a check. The stock capture
(a stock capture) adds: each pad is four consecutive indices (the start-up sweep
paints fours), steps 0x40-0x4F (0x40/44/48/4C brighter = beats); button values: TAP TEMPO blinks 1/3, SHIFT 1 -> 3
held, so 1 dim and 3 bright; mode buttons 4 (others) and 5 (current). 
