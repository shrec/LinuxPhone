#!/bin/bash
# Update a running phone without touching its buttons:
#   ./flash.sh <device> [--host user@ip] [--no-userdata] [--boot-only]
# Reboots the phone into fastboot over SSH, flashes boot (+ userdata), then `fastboot boot`s it.
#   --boot-only    test-boot boot.img, write nothing
#   --no-userdata  keep the phone's data, only update the kernel
set -euo pipefail

usage() { echo "usage: ./flash.sh <device> [--host user@ip] [--no-userdata] [--boot-only]" >&2; exit 1; }

DEVICE=""
HOST="phone@172.16.42.1"
NO_USERDATA=0
BOOT_ONLY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --host) [ $# -ge 2 ] || usage; HOST=$2; shift 2 ;;
    --no-userdata) NO_USERDATA=1; shift ;;
    --boot-only) BOOT_ONLY=1; shift ;;
    -h|--help) usage ;;
    -*) echo "unknown option: $1" >&2; usage ;;
    *) [ -z "$DEVICE" ] || usage; DEVICE=$1; shift ;;
  esac
done
[ -n "$DEVICE" ] || usage

SRC=$(cd "$(dirname "$0")" && pwd)
OUT=$SRC/out/$DEVICE
BOOT=$OUT/boot.img
ROOTFS=$OUT/rootfs.img
VBMETA=$OUT/vbmeta.img

command -v fastboot > /dev/null || { echo "fastboot not found: install Android platform-tools" >&2; exit 1; }
for f in "$BOOT" "$ROOTFS" "$VBMETA"; do
  [ -f "$f" ] || { echo "missing $f: run ./build.sh $DEVICE first" >&2; exit 1; }
done

# On Windows a running adb server holds the USB device and breaks fastboot.
if command -v adb > /dev/null; then
  echo "== adb kill-server"
  adb kill-server > /dev/null 2>&1 || true
fi

fastboot_up() { [ -n "$(fastboot devices 2> /dev/null)" ]; }

# SSH answers: a login or an authentication refusal both mean the phone is up.
ssh_up() {
  local msg
  msg=$(ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new "$HOST" true 2>&1) && return 0
  case "$msg" in
    *"Permission denied"*|*"Host key verification failed"*) return 0 ;;
  esac
  return 1
}

if fastboot_up; then
  echo "== phone already in fastboot"
else
  if ssh_up; then
    echo "== rebooting $HOST into fastboot (sudo may ask for the phone password)"
    ssh -t "$HOST" sudo systemctl reboot --reboot-argument=bootloader || true
  else
    echo "== $HOST does not answer SSH"
    echo "   Put the phone into fastboot by hand: hold Power + Volume Down."
  fi
  echo "== waiting for fastboot"
  waited=0
  while ! fastboot_up; do
    sleep 2
    waited=$((waited + 2))
    if [ "$waited" -eq 90 ]; then
      echo "   still no fastboot after 90 s: hold Power + Volume Down on the phone"
    fi
  done
  echo "== fastboot is up"
fi

if [ "$BOOT_ONLY" -eq 1 ]; then
  echo "== test boot (nothing is written)"
  fastboot boot "$BOOT"
else
  echo "== flash boot (all slots)"
  fastboot flash --slot=all boot "$BOOT"
  if [ "$NO_USERDATA" -eq 1 ]; then
    echo "== skipping userdata (--no-userdata)"
  else
    echo "== flash userdata"
    fastboot flash userdata "$ROOTFS"
  fi
  echo "== boot the flashed kernel"
  fastboot boot "$BOOT"
fi

echo "== waiting for the phone to answer SSH (up to 120 s)"
# Wall-clock deadline: each ssh_up can itself take up to its 5 s connect timeout.
deadline=$((SECONDS + 120))
while ! ssh_up; do
  if [ "$SECONDS" -ge "$deadline" ]; then
    echo "no SSH after 120 s: check the USB cable and 'usbinfo' on the phone; hold Power to retry"
    exit 1
  fi
  sleep 2
done
echo "booted: ssh $HOST"
