# Stage 1: boot to a terminal (Xiaomi Mi A2)

## Goal
Replace Android completely with our own Linux OS on old phones. No Google services, no Halium or
libhybris, no Android userspace. Stage 1 ends when the phone boots to a text console that you can
use with an on-screen keyboard and reach over USB SSH. The UI, Wi-Fi and the app market are later stages.

## Decisions
- **Kernel:** mainline Linux via [sdm660-mainline](https://github.com/sdm660-mainline/linux), tag `v7.2.15-sdm660`
  (the same one postmarketOS ships). Starts from `sdm660_defconfig`, and the device folder adds a small fragment on top.
  Drivers and firmware are the manufacturer's/upstream's. The project's own work is the OS.
- **Userland:** Debian arm64, built with `mmdebstrap`. We use **forky** (testing) for now because
  `buffyboard` isn't in trixie. Move to stable once it lands there.
- **First device:** Xiaomi Mi A2 (`jasmine_sprout`, SDM660). Its Android One bootloader unlocks with `fastboot flashing unlock`.
- **Terminal:** fbcon on simpledrm (the bootloader-initialised framebuffer), Terminus 16×32 font,
  `buffyboard` on-screen keyboard, getty on tty1. USB NCM gadget with systemd-networkd DHCP server, so
  `ssh phone@172.16.42.1` works.
- **Boot:** Android boot image v0 (`mkbootimg`, offsets from postmarketOS deviceinfo) with an appended DTB.
  A tiny busybox initramfs mounts the rootfs by filesystem label `lp_root`. It ignores whatever `root=`
  the Android bootloader appends, which is the reason an initramfs exists at all. Storage drivers are built in, so the initramfs needs no modules.
- **Rootfs:** ext4 image flashed to `userdata`. `x-systemd.growfs` grows it to the full partition on first boot.
- **AVB:** an empty vbmeta with flags=2 disables verification, the same as `avbtool make_vbmeta_image --flags 2`.
- **Device ports:** one folder `devices/<codename>/` with `device.conf` and `kernel.config`. Contributors and
  vendors add phones without touching the OS.
- **Build:** everything runs in Docker, with work on a Docker volume because bind mounts can't hold a rootfs.
  One command: `./build.sh <device>`.

## Risks and fallbacks
- If the mainline display driver (msm DRM, loaded as a module) takes the screen over and fails, blacklist `msm` and keep simpledrm.
- If the touchscreen doesn't work, buffyboard has no input, but USB SSH still works.
- If Windows doesn't bind its NCM driver to the gadget, add an RNDIS function as a fallback.

## Verification
1. `./build.sh xiaomi-jasmine` produces `boot.img`, `rootfs.img` and `vbmeta.img`, and fails if any kernel option didn't stick.
2. `fastboot boot boot.img` shows the kernel log on screen, which stops at "rootfs not found" before anything is flashed.
3. After flashing, `ssh phone@172.16.42.1` from the PC works.
4. `uname -a` typed on the on-screen keyboard shows on tty1.
5. Record idle `free -m` in the README as the baseline.

## Later stages
2) Wi-Fi + firmware · 3) GPU + Wayland compositor + our own UI · 4) Redmi and Samsung A-series ports · 5) app market
