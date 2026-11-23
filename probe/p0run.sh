#!/bin/sh
# Phase 0 probe run on the Live III: stops the MPC app, tests screen, sound, surface and touch, then starts MPC again.
# Surface and touch are recorded for the whole run (75 s), so pressing at any time counts.
# Run detached: setsid sh /tmp/p0/p0run.sh >/dev/null 2>&1 &   Log: /tmp/p0/p0.log
D=/tmp/p0
LOG=$D/p0.log
: > $LOG
log() { echo "$(date +%T) $*" >> $LOG; }
restore() { log "starting acvs"; systemctl start acvs; log "acvs $(systemctl is-active acvs)"; }
trap restore EXIT INT TERM

log "stopping acvs"
systemctl stop acvs
sleep 2
log "acvs $(systemctl is-active acvs)"

log "=== SCREEN: test card now (red square = landscape top left)"
$D/p0probe drm 75 >> $LOG 2>&1 &

log "=== SURFACE + TOUCH recording for 75 s"
amidi -p hw:0,0,1 -d -t 75 > $D/private.txt 2>&1 &
amidi -p hw:0,0,0 -d -t 75 > $D/public.txt 2>&1 &
$D/p0probe touch 75 > $D/touch.txt 2>&1 &

sleep 5
for n in 1 2 3; do
  first=$(( (n - 1) * 2 ))
  log "=== TONE $n: channels $first+$(( first + 1 )), 4 s"
  $D/p0probe tone 4 $first >> $LOG 2>&1
  sleep 2
done
wait
log "private bytes: $(wc -c < $D/private.txt)  public bytes: $(wc -c < $D/public.txt)  touch lines: $(wc -l < $D/touch.txt)"
log "=== done"
