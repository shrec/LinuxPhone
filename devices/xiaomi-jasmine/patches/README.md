# Kernel patches

`build.sh` applies `*.patch` here in name order, with `git apply`, to a clean checkout of
`KERNEL_TAG` (see `device.conf`). A patch that does not apply fails the build.

## 0001-ipa-v2.6l-sdm660-mainline-pr250.patch

- **Source:** https://github.com/sdm660-mainline/linux/pull/250, "IPA2 support (New iteration)".
  Head commit `77e3fe8f83d0a6dae0e9e1f833baa7995a1d3d85`, base `4f433ddb904f09ee010592d449abf191d3badaad`.
  The file is a `git diff` of that range and applies cleanly to `v7.2.15-sdm660`.
- **Status:** the PR is open, so this is experimental. Its `sdm630.dtsi` IPA node says the
  register split is illustrative.
- **Enables:** the IPA v2.6L driver. IPA probes ("IPA driver setup completed successfully"),
  `rmnet_ipa0` appears, and ModemManager lists the modem. That is the path for calls and SMS.
  Without the patch ModemManager refuses the modem ("Failed to find a net port in the QMI modem").
  Mobile data is not verified.
- **Depends on diag-router:** it must run from boot (`diag-router.service`, `DIAG_ROUTER_REF` in
  `device.conf`). Without it the modem crashes before ModemManager can talk to it.
- **Measured** on the Mi A2 on 2026-11-28: no modem crashes, firmware MPSS.AT.3.1, ModemManager
  state `failed: sim-missing` because no SIM was inserted.

## 0002-ipa-v2-skip-modem-exception-reset.patch

- **Source:** ours, on top of 0001.
- **Fixes:** every modem stop hung forever, so `systemctl reboot` never finished (sshd gone,
  screen black, kernel still answering ping). On a modem stop the IPA driver resets the modem
  endpoints through the command channel. On v2.6L that channel faults in the SMMU and the
  pipeline-clear tag never arrives. `ipa_cmd_pipeline_clear_wait()` waits without a timeout,
  and rmtfs (which stops the modem on SIGTERM) sat in D state with it.
- **Change:** skip that reset on IPA v2.x, like 0001 already skips `modem_pause_all`.
- **Measured** on the Mi A2 on 2026-11-28: `systemctl stop rmtfs` stops the modem in 1 s
  (was: never), the modem and Wi-Fi come back on `systemctl start rmtfs`, and
  `systemctl reboot --reboot-argument=bootloader` lands in fastboot (systemd shutdown 2.6 s).
