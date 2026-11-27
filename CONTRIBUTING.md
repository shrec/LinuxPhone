# Contributing

## Adding a phone

A phone is one folder: `devices/<vendor>-<codename>/`. Nothing outside it should need to change.

1. **Find a kernel.** You need a Linux kernel with a device tree (`.dts`) for the phone.
   For most phones this is mainline Linux or a close-to-mainline fork. Good places to look are the
   [postmarketOS device list](https://wiki.postmarketos.org/wiki/Devices) and the kernel forks it points to.
   A device or SoC vendor can just as well ship its own kernel tree here.
2. **`device.conf`:** copy `devices/xiaomi-jasmine/device.conf` and fill in:
   - `KERNEL_REPO`, `KERNEL_TAG`, `KERNEL_DEFCONFIG`: where the kernel comes from and which defconfig to start from.
   - `DTB`: the device tree path under `arch/arm64/boot/dts/`, without `.dtb`.
   - `CMDLINE`: the kernel command line.
   - `BOOTIMG_ARGS`: `mkbootimg` page size and offsets. postmarketOS's `deviceinfo` for the phone lists them as `deviceinfo_flash_*`.
   - Optional: `PACKAGES` (extra Debian packages, comma-separated), `SERVICES` (systemd units to
     enable), `FIRMWARE_URL` + `FIRMWARE_SHA512` (a tarball whose `usr/lib/firmware/` goes into the image).
   - Optional: `SCALE` (output scale for revive-shell, an integer such as `3`) and `FIRMWARE_FROM_DEB` (single files to take out of large firmware `.deb`s, as `pkg:file,file`).
   - Optional: `devices/<name>/rootfs/`, files copied over the OS-wide `rootfs/`. Prefer loading
     firmware from the phone's own partitions at boot, as `xiaomi-jasmine` does, over shipping copies.
3. **`kernel.config`:** options appended to the defconfig. Anything needed to reach the rootfs
   (storage controller, its clocks, ext4) must be `=y`, because the initramfs carries no modules.
   `build.sh` fails if an option doesn't stick.
4. Build with `./build.sh <your-device>`, test with `fastboot boot`, then add a row to the table in README.md.

## Changes to the OS itself

Files that every phone gets go in `rootfs/`, and packages go in the `--include=` list in `build.sh`.
Keep the base small. If something only one phone needs, it belongs in that phone's folder.

Open an issue before large changes, so we can agree on direction first.
