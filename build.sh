#!/bin/bash
# Build a flashable image for one phone:  ./build.sh <device>   (see devices/)
# Output: out/<device>/{boot.img,rootfs.img,vbmeta.img}
set -euo pipefail
DEVICE=${1:?usage: ./build.sh <device>  — one of: $(ls "$(dirname "$0")/devices")}
SRC=$(cd "$(dirname "$0")" && pwd)
[ -f "$SRC/devices/$DEVICE/device.conf" ] || { echo "unknown device: $DEVICE" >&2; exit 1; }

# On the host: re-run inside the build container. Work happens on a Docker volume
# (a Windows/macOS bind mount can't hold device nodes or Linux permissions).
if [ ! -f /.dockerenv ]; then
  export MSYS_NO_PATHCONV=1                                # Git Bash: don't rewrite /src, /work
  HOST_SRC=$(cd "$SRC" && (pwd -W 2>/dev/null || pwd))     # Git Bash: D:/... form for Docker
  docker build -t linuxphone-build "$HOST_SRC"
  exec docker run --rm --privileged \
    -v "$HOST_SRC:/src" -v linuxphone-work:/work \
    linuxphone-build /src/build.sh "$DEVICE"
fi

. "$SRC/devices/$DEVICE/device.conf"
W=/work/$DEVICE; OUT=$SRC/out/$DEVICE; K=/work/linux-$KERNEL_TAG
mkdir -p "$W" "$OUT"
KMAKE=(make -C "$K" O="$W/kbuild" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)")

echo "== kernel $KERNEL_TAG"
[ -d "$K" ] || git clone --depth 1 --branch "$KERNEL_TAG" "$KERNEL_REPO" "$K"
# Boot logo: assets/logo.svg becomes the kernel's 224-colour framebuffer logo.
rsvg-convert "$SRC/assets/logo.svg" | pngtopnm | pnmquant 224 2>/dev/null | pamtopnm -plain \
  > "$K/drivers/video/logo/logo_linux_clut224.ppm"
"${KMAKE[@]}" "$KERNEL_DEFCONFIG"
cat "$SRC/devices/$DEVICE/kernel.config" >> "$W/kbuild/.config"
"${KMAKE[@]}" olddefconfig
# Kconfig silently drops options whose dependencies are missing; catch that here.
grep '^CONFIG_' "$SRC/devices/$DEVICE/kernel.config" | while read -r opt; do
  grep -qx "$opt" "$W/kbuild/.config" || { echo "kernel option did not stick: $opt" >&2; exit 1; }
done
"${KMAKE[@]}" Image.gz dtbs modules

echo "== rootfs (Debian forky arm64)"
# ponytail: forky (testing) because buffyboard isn't in trixie; move to stable once it lands there.
R=$W/rootfs; D=$SRC/devices/$DEVICE; rm -rf "$R"
# Files: the OS-wide overlay, then the device's own (devices/<name>/rootfs) on top.
mmdebstrap --mode=root --arch=arm64 --variant=minbase --components=main,non-free-firmware \
  --include=systemd-sysv,udev,dbus,kmod,iproute2,openssh-server,sudo,kbd,nano,less,busybox-static,buffyboard,iwd,wireless-regdb,systemd-resolved,systemd-timesyncd,dbus-user-session,foot,fonts-jetbrains-mono,hicolor-icon-theme,libgl1-mesa-dri,libegl1,qt6-qpa-plugins,qt6-wayland,qml6-module-qtquick,qml6-module-qtquick-window,phoc,layer-shell-qt,qml6-module-org-kde-layershell,qt6-svg-plugins,modemmanager,gnome-calls,chatty,gnome-contacts,phosh-osk-stevia,libglib2.0-bin${PACKAGES:+,$PACKAGES} \
  --customize-hook="for o in $SRC/rootfs $D/rootfs; do [ ! -d \$o ] || cp -r --no-preserve=mode,ownership \$o/. \"\$1\"/; done; chmod 755 \"\$1\"/usr/local/*bin/*" \
  --customize-hook='printf "revive (built %s) \\\\l\\n\\n" "$(date -u "+%F %H:%M UTC")" > "$1/etc/issue"' \
  --customize-hook="chroot \"\$1\" sh -ec '
      useradd -m -s /bin/bash -G sudo phone; echo phone:1234 | chpasswd
      rm -f /etc/ssh/ssh_host_*
      glib-compile-schemas /usr/share/glib-2.0/schemas
      systemctl enable systemd-networkd systemd-resolved iwd usb-gadget revive-shell ssh ModemManager ${SERVICES:-}
      systemctl disable getty@tty1'" \
  forky "$R" http://deb.debian.org/debian
"${KMAKE[@]}" modules_install INSTALL_MOD_PATH="$R/usr" INSTALL_MOD_STRIP=1

# Device firmware the phone's own partitions don't carry (e.g. ath10k's board/feature files).
if [ -n "${FIRMWARE_URL:-}" ]; then
  curl -fsSL "$FIRMWARE_URL" -o "$W/firmware.tar.gz"
  echo "$FIRMWARE_SHA512  $W/firmware.tar.gz" | sha512sum -c --quiet -
  tar -xzif "$W/firmware.tar.gz" -C "$R" --no-same-owner --wildcards 'usr/lib/firmware/*' \
    --transform 's,^usr/lib/firmware/postmarketos/,usr/lib/firmware/,' 2>/dev/null
fi
# Single files out of large firmware .debs (FIRMWARE_FROM_DEB="pkg:file,file ...").
for spec in ${FIRMWARE_FROM_DEB:-}; do
  pkg=${spec%%:*}
  (cd "$W" && rm -rf fwx "${pkg}"_*.deb && apt-get download -q "$pkg" && dpkg-deb -x "${pkg}"_*.deb fwx)
  for f in $(echo "${spec#*:}" | tr ',' ' '); do install -D -m 644 "$W/fwx/$f" "$R/$f"; done
done

echo "== revive-shell"
B=/work/sdk-forky
# Built under a temporary name and renamed only when complete, so an interrupted first build
# never leaves a broken cached chroot for later builds to reuse.
if [ ! -d "$B" ]; then
  rm -rf "$B.tmp"
  mmdebstrap --mode=root --arch=arm64 --variant=apt \
    --include=build-essential,cmake,qt6-base-dev,qt6-declarative-dev,qt6-wayland-dev,qt6-wayland-private-dev,libwayland-dev forky "$B.tmp" http://deb.debian.org/debian
  mv "$B.tmp" "$B"
fi
cp /etc/resolv.conf "$B/etc/resolv.conf"
mount -t proc proc "$B/proc"; trap 'umount "$B/proc" 2>/dev/null || true' EXIT
chroot "$B" sh -ec 'apt-get -q update && apt-get -qy dist-upgrade >/dev/null'  # same Qt as the rootfs
rm -rf "$B/src" "$B/out"; cp -r "$SRC/shell" "$B/src"
chroot "$B" sh -ec 'cmake -S /src -B /src/build -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build /src/build -j"$(nproc)"
  ctest --test-dir /src/build --output-on-failure
  DESTDIR=/out cmake --install /src/build --prefix /usr'
umount "$B/proc"; trap - EXIT
cp -a "$B/out/." "$R/"

echo "== initramfs"
# Mounts the rootfs by filesystem label, ignoring the root= the Android bootloader appends.
I=$W/initramfs; rm -rf "$I"; mkdir -p "$I"/{bin,dev,proc,sys,root}
cp "$R/usr/bin/busybox" "$I/bin/"
# The kernel opens /dev/console for init before devtmpfs exists; without it init has no output.
mknod -m 600 "$I/dev/console" c 5 1
cat > "$I/init" <<'INIT'
#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc proc /proc; mount -t sysfs sys /sys; mount -t devtmpfs dev /dev
exec </dev/console >/dev/console 2>&1
for i in $(seq 100); do ROOT=$(findfs LABEL=lp_root 2>/dev/null) && break; sleep 0.1; done
# On failure keep the message on screen: if init exits, the kernel panics and the phone reboots.
mount -o rw "$ROOT" /root || { echo "revive: rootfs (LABEL=lp_root) not found"; while :; do sleep 3600; done; }
umount /proc /sys; mount --move /dev /root/dev
exec switch_root /root /sbin/init
INIT
chmod +x "$I/init"
(cd "$I" && find . | cpio -o -H newc --quiet) > "$W/initramfs.cpio"

echo "== images"
KB=$W/kbuild/arch/arm64/boot
cat "$KB/Image.gz" "$KB/dts/$DTB.dtb" > "$W/Image.gz-dtb"
# shellcheck disable=SC2086
mkbootimg --kernel "$W/Image.gz-dtb" --ramdisk "$W/initramfs.cpio" --cmdline "$CMDLINE" \
  $BOOTIMG_ARGS -o "$OUT/boot.img"
rm -f "$W/rootfs.raw"
mkfs.ext4 -q -L lp_root -d "$R" "$W/rootfs.raw" 3G
img2simg "$W/rootfs.raw" "$OUT/rootfs.img"
# Empty AVB vbmeta with flags=2 (verification disabled), as `avbtool make_vbmeta_image --flags 2` makes.
python3 -c "import struct,sys; h=(b'AVB0'+struct.pack('>II',1,0)).ljust(120,b'\0')+struct.pack('>I',2); sys.stdout.buffer.write(h.ljust(4096,b'\0'))" > "$OUT/vbmeta.img"

for f in boot.img rootfs.img vbmeta.img; do [ -s "$OUT/$f" ] || { echo "missing $f" >&2; exit 1; }; done
ls -lh "$OUT"
