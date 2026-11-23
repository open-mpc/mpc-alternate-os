# Recovery

The launcher restores MPC when the app ends for any reason. If something still goes wrong:

1. From a computer: `ssh root@HOST /data/mpc-launcher/mpc-launch stop`, then `systemctl start acvs`.
2. App hung and SSH works: `systemctl stop mpc-launcher-app; systemctl start acvs`.
3. No SSH, screen black: hold the power button to power off, then power on. Nothing is installed in MPC's own
   area, so a normal boot gives the stock MPC. Launcher files live in `/data/mpc-launcher` and do not auto-start.
4. Another tool logging in over SSH can start MPC while an app owns the hardware. Keep other tools off during tests.
5. `/tmp` and `/run` are cleared at reboot, so probe scripts must be copied again.

Always test with a time limit (`mpc-launch start APP 120`) until an app is trusted.
