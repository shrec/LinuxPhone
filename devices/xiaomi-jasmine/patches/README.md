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
