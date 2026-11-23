#!/bin/sh
# Copies the launcher, the remote addin and one app to a device. Stops nothing.
#   ./install.sh HOST                       the launcher, the remote addin and the app picker
#   ./install.sh HOST APP BINARY [CONF]     also the app: BINARY into apps/APP/bin/, CONF (default apps/APP.conf) as app.conf
# HOST is user@address (the Live III: root@192.168.2.217). The remote addin comes from $REMOTE_SO, default
# ~/mpc-addin-remote/build/aarch64/mpc_remote_addin.so (branch standalone-apps).
set -eu
HOST=${1:?host}
HERE=$(cd "$(dirname "$0")" && pwd)
REMOTE_SO=${REMOTE_SO:-$HOME/mpc-addin-remote/build/aarch64/mpc_remote_addin.so}
# The picker is sample-base's `menu` app (branch live3): $MENU_BIN, default its build in the OpenMPC Groove tree.
MENU_BIN=${MENU_BIN:-$HOME/openmpc-groove/upstream/sample-base/build/sample-base}
R=/data/mpc-launcher
ssh "$HOST" "mkdir -p $R/remote $R/apps $R/menu"
# Beside, then moved into place: a binary a running app has mapped is never written over.
scp -q "$HERE/device/mpc-launch" "$HOST:$R/mpc-launch.new"
scp -q "$REMOTE_SO" "$HOST:$R/remote/mpc_remote_addin.so.new"
scp -q "$HERE/device/remote.conf" "$HOST:$R/remote/mpc_remote_addin.conf"
scp -q "$MENU_BIN" "$HOST:$R/menu/sample-base.new"
scp -q "$HERE/autorun/autorun.sh" "$HOST:$R/autorun.sh"
ssh "$HOST" "cd $R && mv -f mpc-launch.new mpc-launch && chmod +x mpc-launch && mv -f remote/mpc_remote_addin.so.new remote/mpc_remote_addin.so &&
  mv -f menu/sample-base.new menu/sample-base && chmod +x menu/sample-base"
if [ $# -ge 3 ]; then
    APP=$2; BINARY=$3; CONF=${4:-$HERE/apps/$APP.conf}
    NAME=$(basename "$BINARY")
    ssh "$HOST" "mkdir -p $R/apps/$APP/bin"
    scp -q "$BINARY" "$HOST:$R/apps/$APP/bin/$NAME.new"
    scp -q "$CONF" "$HOST:$R/apps/$APP/app.conf"
    ssh "$HOST" "cd $R/apps/$APP/bin && mv -f $NAME.new $NAME && chmod +x $NAME"
fi
ssh "$HOST" "$R/mpc-launch list"
