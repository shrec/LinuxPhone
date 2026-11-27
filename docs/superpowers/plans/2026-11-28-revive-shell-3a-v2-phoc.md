# revive-shell 3a v2: phoc compositor + revive shell as layer-shell clients

> Replaces Tasks 4-6 of `2026-11-28-revive-shell-3a-skeleton.md` where they assume Qt's compositor.
> Tasks 1-3 (desktop-entry parsing, launch args, AppModel, Theme) and Task 5 (session service) are reused.

**Why:** tested on the Mi A2 (2026-11-28), Qt Wayland Compositor 6.11 exposes `wl_seat` v4 and
`wl_data_device_manager` v1, so foot refuses to start ("need interface version 5 / 3"). It also has
no GPU buffer path on Mesa 26 (no `EGL_WL_bind_wayland_display`, and Debian ships no dmabuf plugin).
wlroots implements the current protocols and dmabuf. This is the fallback the spec names: phoc
(wlroots, built for phones) is the compositor, and revive's QML shell runs on it as layer-shell
clients, the way Android's SystemUI and Launcher are apps on top of SurfaceFlinger.

## Architecture
```
phoc (wlroots: xdg-shell, layer-shell, foreign-toplevel, linux-dmabuf, current wl_seat/data-device)
 ├─ revive-shell (Qt 6 Wayland client, QT_WAYLAND_SHELL_INTEGRATION=layer-shell)
 │    ├─ StatusBar window   layer=top,        anchored top, exclusive zone 28
 │    ├─ Launcher window    layer=background, full screen (apps cover it)
 │    └─ HomeStrip window   layer=overlay,    anchored bottom, 28 px; swipe up = minimize active app
 └─ apps (foot, GTK, Qt, Firefox, SDK apps) via xdg-shell
```
- Scale comes from phoc (`/etc/phoc/phoc.ini`, `[output:DSI-1] scale = 3`). The shell gets it from
  `wl_output` like any client, so `REVIVE_SCALE`/`QT_SCALE_FACTOR` go away.
- App switching uses `zwlr_foreign_toplevel_management_v1` (phoc implements it). A `ToplevelModel` lists
  windows and can activate, minimize or close them. Tapping a running app activates it instead of
  launching a second copy.

## Tasks
**V1. Shell as layer-shell client** (`shell/src/main.cpp`, `shell/qml/*.qml`, `shell/CMakeLists.txt`, `shell/src/appmodel.cpp`)
- main.cpp: default `QT_QPA_PLATFORM=wayland` and `QT_WAYLAND_SHELL_INTEGRATION=layer-shell`. Drop eglfs,
  `QT_SCALE_FACTOR`, `REVIVE_SCALE` and the `outputScale` context property. Keep the icon provider.
- `Main.qml` becomes a non-visual root (`QtObject`) holding three `Window`s built with `import org.kde.layershell 1.0 as LayerShell`.
  The windows are StatusBar, Launcher and HomeStrip, with the layers, anchors, exclusive zone and
  keyboard interactivity from the architecture above. Reuse `StatusBar.qml`, `Launcher.qml` and `Theme.qml` unchanged.
- AppModel.launch: inherit `WAYLAND_DISPLAY` from the shell's environment (phoc sets it). Don't hardcode `wayland-0`.
- CMake: the `revive-shell` target builds only `if(UNIX AND NOT APPLE)`. Windows keeps building `revive_core` + tests.
- Remove the `qt6-wayland-compositor` QML imports and code entirely.

**V2. ToplevelModel (foreign-toplevel)** (`shell/src/toplevelmodel.{h,cpp}`, `shell/protocols/wlr-foreign-toplevel-management-unstable-v1.xml`, `shell/CMakeLists.txt`, `shell/qml/Main.qml`, `shell/qml/Launcher.qml`)
- Vendor the protocol XML (MIT, from wlr-protocols) and generate client code with `qt6_generate_wayland_protocol_client_sources`. Use `QWaylandClientExtensionTemplate`.
- QML singleton `ToplevelModel`: roles `appId`, `title`, `activated`. `Q_INVOKABLE bool activate(QString desktopId)`
  (matches with `AppModel.matches`), `minimizeActive()`, `closeActive()`.
- Launcher tap: `if (!ToplevelModel.activate(appId)) AppModel.launch(appId)`. A HomeStrip swipe up by more than 60 px calls `ToplevelModel.minimizeActive()`.

**V3. Session and image** (`rootfs/etc/systemd/system/revive-shell.service`, `devices/xiaomi-jasmine/rootfs/etc/phoc/phoc.ini`, `build.sh`, `devices/xiaomi-jasmine/device.conf`)
- Service: `ExecStart=/usr/bin/phoc -C /etc/phoc/phoc.ini -E /usr/bin/revive-shell`. Everything else stays as is (logind session, fallback to console).
- `phoc.ini` per device: `[output:DSI-1]` `scale = 3`. Replaces `SCALE` and `/etc/revive/shell.env`; remove both.
- Packages: add `phoc,layer-shell-qt,qml6-module-org-kde-layershell,qt6-wayland`; drop `qml6-module-qtwayland-compositor`. The SDK chroot adds `qt6-wayland-dev,qt6-wayland-private-dev,libwayland-dev`.

## Verification (manager, on the phone)
- Build the shell on the phone. Run `phoc -C phoc.ini -E revive-shell` in a `phone` logind session (`systemd-run -p User=phone -p PAMName=login -p TTYPath=/dev/tty1`).
- The owner checks: the bar shows on top, the launcher grid is visible, tapping Foot opens a working terminal,
  swiping up goes home, tapping Foot again brings back the same window (`pgrep -c -x foot` = 1),
  and killing foot returns to the launcher.
