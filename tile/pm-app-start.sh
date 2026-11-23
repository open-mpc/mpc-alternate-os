#!/bin/sh
# OpenMPC Launcher tile: start the page daemon (launcher-page) in its own unit. The mode slot shim (modeslot.so, next
# to this file in /data/mpc-addins/openmpc-launcher/) runs this script, detached, each time MPC starts. The file keeps
# the shim's fixed name "pm-app-start.sh".
D=$(cd "$(dirname "$0")" && pwd)
U=openmpc-launcher-page
BIN="$D/launcher-page"
log() { echo "openmpc-launcher: $*" >&2; }
[ -x "$BIN" ] || { log "no $BIN"; exit 1; }
pid=$(systemctl show -p MainPID --value "$U" 2>/dev/null)
if [ -n "$pid" ] && [ "$pid" != 0 ] && [ "$(readlink "/proc/$pid/exe")" = "$BIN" ]; then exit 0; fi
systemctl stop "$U" 2>/dev/null
systemctl reset-failed "$U" 2>/dev/null
systemd-run --unit="$U" --collect --quiet "$BIN" /run/openmpc-page /data/mpc-launcher && log "started $BIN"
