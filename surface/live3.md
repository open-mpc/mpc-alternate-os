# Live III surface reference

Verified by eye on the device unless marked.

**Start-up** (Public port): replay the 15 SysEx messages of `probe/live3_init.txt`, 3 ms apart, then send
`F0 47 00 58 52 00 04 13 75 21 69 F7` every 3 s. Controller id 0x58.

**Pad and step lights:** `F0 47 7F 58 65 00 04 idx r g b F7`
- Pad N (1-16) is indices 4(N-1)..4(N-1)+3, in the order bottom-left, bottom-right, top-left, top-right.
- Step keys are 0x40-0x4F, left to right (0x40/44/48/4C are the beat positions).

**Button lights:** CC on the Private port. 1 dim, 3 bright; mode buttons 4 (others) and 5 (current).
- Transport lights use their own CC numbers: PLAY 0x56, STOP 0x55, REC 0x65, OVERDUB 0x54 (found from a stock capture, `probe/transport_capture.log`). Unit tested, not yet confirmed lit in an app.
- MAIN: no known light.

**Inputs:** see [how-it-works](../docs/how-it-works.md).
