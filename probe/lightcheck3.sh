#!/bin/sh
# Live III light check: stops the MPC app, brings its controller up the way MPC does (force-link start-up +
# heartbeat), shows four light patterns, then starts MPC again (always, via trap). About 90 s.
# lightcheck3.sh: quadrants one at a time (10 s each) then button values, about 2 min.
# Run detached:  setsid sh /tmp/p0/lightcheck.sh >/dev/null 2>&1 &     Log: /tmp/p0/lights.log
# Needs /tmp/p0/live3_init.txt: force-link's spoof/live3_init.syx as hex, one SysEx message a line.
#
# What to look at (each part starts with all 16 pads flashing white once):
#   1. 15 s: ONE pad lit, its four lights red, green, blue, white (light index 0, 1, 2, 3).
#            -> which pad, and which corner is red / green / blue / white?
#   2. 16 s: pads fill one by one, 1 s each (index groups 0-3, 4-7, ...). -> the order they fill in.
#   3. 16 s: the step keys fill one by one (index 0x40 ...). -> the order.
#   4. 40 s: button lights. Every button lights with one value for 5 s, values 0 to 7 in turn; the number of
#            step keys lit (from the left) = the value. -> what PLAY, REC, MAIN, SHIFT, MUTE, ... look like per value.
D=/tmp/p0
LOG=$D/lights.log
PUB=hw:0,0,0
PRIV=hw:0,0,1
: > $LOG
log() { echo "$(date +%T) $*" >> $LOG; }
restore() { kill $HB 2>/dev/null; log "starting acvs"; systemctl start acvs; log "acvs $(systemctl is-active acvs)"; }
trap restore EXIT INT TERM

pub() { amidi -p $PUB -S "$*" 2>> $LOG; }
priv() { amidi -p $PRIV -S "$*" 2>> $LOG; }
rgb() { printf 'F0 47 7F 58 65 00 04 %02X %02X %02X %02X F7 ' "$1" "$2" "$3" "$4"; } # index r g b
all_pads() { m=; i=0; while [ $i -lt 64 ]; do m="$m$(rgb $i $1 $2 $3)"; i=$((i + 1)); done; pub "$m"; }
all_steps() { m=; i=64; while [ $i -lt 80 ]; do m="$m$(rgb $i $1 $2 $3)"; i=$((i + 1)); done; pub "$m"; }
flash() { all_pads 127 127 127; sleep 0.4; all_pads 0 0 0; sleep 0.6; }
# Every Live III button note seen (FINDINGS 17 + mpc-addin-remote).
BUTTONS="0A 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A 6B 6D 6C 0B 27 28 09 23 24 25 26 7A 72 2D 43 29 32 2B 2C 00 7B 34 31 79 0C 70 21 35 4A 44 45 46 47 48 36 22 20 49 50 51 52 53 37 04"

log "stopping acvs"
systemctl stop acvs
sleep 2
log "acvs $(systemctl is-active acvs)"

# Start-up: MPC's captured messages, one at a time, 3 ms apart.
while read -r msg; do
    [ -n "$msg" ] && pub "$msg" && sleep 0.003
done < $D/live3_init.txt
log "start-up sent"
( while true; do pub "F0 47 00 58 52 00 04 13 75 21 69 F7"; sleep 3; done ) &
HB=$!
all_pads 0 0 0; all_steps 0 0 0

log "=== 1b: pad 1, one index at a time in white, 10 s each; step keys lit = index + 1"
q=0; while [ $q -lt 4 ]; do
    all_pads 0 0 0; all_steps 0 0 0
    pub "$(rgb $q 127 127 127)"
    s=0; while [ $s -le $q ]; do pub "$(rgb $((64 + s)) 127 60 0)"; s=$((s + 1)); done
    log "index $q $(systemctl is-active acvs)"; sleep 10; q=$((q + 1))
done
all_pads 0 0 0; all_steps 0 0 0

log "=== 4: button values 0-7, 10 s each"
flash
v=0; while [ $v -le 7 ]; do
    m=; for b in $BUTTONS; do m="$m B0 $b $(printf %02X $v)"; done
    priv "$m"
    all_steps 0 0 0
    s=0; while [ $s -lt $v ]; do pub "$(rgb $((64 + s)) 127 127 127)"; s=$((s + 1)); done
    log "value $v $(systemctl is-active acvs)"; sleep 10; v=$((v + 1))
done
m=; for b in $BUTTONS; do m="$m B0 $b 00"; done; priv "$m"
all_pads 0 0 0; all_steps 0 0 0
log "=== done"
