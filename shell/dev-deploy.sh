#!/bin/sh
# Build revive-shell on the phone, run its tests, install it and restart it.
#   ./shell/dev-deploy.sh [user@host]      (default phone@172.16.42.1; asks for the sudo password)
set -e
H=${1:-phone@172.16.42.1}
cd "$(dirname "$0")"
tar -cf - CMakeLists.txt src qml tests 2>/dev/null | ssh "$H" 'rm -rf ~/revive-shell-src && mkdir -p ~/revive-shell-src && tar -xf - -C ~/revive-shell-src'
ssh -t "$H" 'set -e; cd ~/revive-shell-src
  cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
  cmake --build build -j"$(nproc)"
  ctest --test-dir build --output-on-failure
  if [ -f build/revive-shell ]; then sudo cmake --install build --prefix /usr && sudo systemctl restart revive-shell 2>/dev/null || true; fi'
