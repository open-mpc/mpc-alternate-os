#!/bin/sh
# mpc-launcher's start from the device: copy this file to the top folder of a USB stick or SD card.
# With Developer Mode on (MPC OS 3.10.1), MPC asks "... contains an autorun script. Do you want to run it now?"
# when the drive is attached; Yes stops MPC and shows the app picker. Leaving the picker ("Back to MPC" or MAIN)
# starts MPC again. Needs mpc-launcher installed on the device (/data/mpc-launcher).
L=/data/mpc-launcher/mpc-launch
mkdir -p /run/mpc-launcher
if [ -x "$L" ]; then
    "$L" menu > /run/mpc-launcher/autorun.log 2>&1
else
    echo "mpc-launcher is not installed" > /run/mpc-launcher/autorun.log
fi
