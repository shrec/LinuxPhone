# Stage 3: revive shell and SDK

## Goal
A graphical shell that is entirely revive's own: home screen, status bar, app switching, gestures,
notifications, lock screen. Any Linux app (Qt, GTK, SDL, Flutter, …) runs inside it with full GPU
acceleration. Alongside it, an SDK so other developers can write apps that look and behave like revive.
The visual style is the terminal look of the boot screen (green on near-black, JetBrains Mono), carried
into a modern touch UI.

## What the spike on the Mi A2 showed (2026-11-28)
- A QML Wayland compositor on eglfs/KMS renders a steady **60 fps** on the Adreno 512 (freedreno,
  GLES 3.1). The compositor uses ~180 MB.
- Debian's Qt Wayland Compositor cannot take GPU buffers from apps. Mesa 26 dropped
  `EGL_WL_bind_wayland_display`, and Debian doesn't build Qt's `linux-dmabuf-unstable-v1` compositor plugin.
  Apps fall back to CPU (shm) rendering. Even then a long list scrolls naturally, but browsers, video
  and games need the GPU.
- The plugin exists in Qt 6.11's source (`src/plugins/hardwareintegration/compositor/linux-dmabuf-unstable-v1`).
  Debian ships the private headers it needs (`qt6-wayland-private-dev`).
- The alternative, phoc (wlroots) plus our shell as layer-shell clients, gives apps the GPU but costs
  ~40% more RAM and limits control over gestures and animations. It stays as the fallback.

## Decisions
- **Own compositor and shell in one process: `revive-shell`.** C++ core plus QML UI on Qt 6
  Wayland Compositor, running directly on KMS (eglfs) with libinput touch.
- **Full GPU for apps:** build and ship Qt's `linux-dmabuf-unstable-v1` compositor plugin.
- **Apps are ordinary Wayland clients.** We don't invent an app runtime. The SDK is a QML UI kit, a
  system API and tooling on top of standard Qt.
- Same split as Android, in one process: the compositor plays SurfaceFlinger plus WindowManager, and
  the shell UI plays SystemUI plus Launcher.

## Architecture

```
┌──────────────────────── revive-shell (one process, user "phone") ────────────────────────┐
│ QML shell UI: status bar · home/launcher · app switcher · shade/quick settings ·         │
│               lock screen · on-screen keyboard · notifications                            │
│ ─────────────────────────────────────────────────────────────────────────────────────────│
│ C++ core:  compositor (QWaylandCompositor, xdg-shell, text-input, output scale)           │
│            app manager (.desktop discovery, launch, lifecycle)                            │
│            system bridge (D-Bus: UPower, iwd, logind, notifications server)               │
│ ─────────────────────────────────────────────────────────────────────────────────────────│
│ Qt eglfs/KMS + libinput        GPU client buffers: linux-dmabuf-unstable-v1 plugin        │
└───────────────────────────────────────────────────────────────────────────────────────────┘
        ▲ Wayland (xdg-shell, linux-dmabuf, text-input)
        │
   apps: revive SDK apps (QML + `import revive`) · any Qt/GTK/SDL app · terminal
```

### Compositor core (C++)
- `QWaylandCompositor` with xdg-shell (one output, the panel), `linux-dmabuf-unstable-v1` for GPU
  buffers with a shm fallback, and text-input for the on-screen keyboard.
- Output scale comes from the device: a new `SCALE` in `device.conf`. The Mi A2 is 1080×2160 at about
  400 dpi, so scale 3 gives a 360×720 logical screen (`wl_output` scale is an integer).
- Touch: libinput through eglfs. Edge swipes are claimed by the shell, and everything else goes to the
  focused app.

### Shell UI (QML)
- **Status bar:** time, battery (UPower), Wi-Fi (iwd), charging state.
- **Home / launcher:** our own launcher (layout, search, folders, motion), a separate QML component
  inside the shell. It reads the list of installed apps from freedesktop `.desktop` files
  (`/usr/share/applications`), which are data that every Linux app installs. It can later be split
  into a replaceable launcher app, as on Android, without changing the shell.
- **App model (phone):** one app fullscreen at a time. Swipe up from the bottom edge goes home.
  Swipe up and hold opens the switcher, which shows live thumbnails (each app's surface as a
  `ShellSurfaceItem`). Flicking a card away closes that app.
- **Shade / quick settings:** swipe down shows notifications, Wi-Fi, brightness (backlight sysfs via
  logind) and rotation (later, when sensors work).
- **Notifications:** `revive-shell` implements `org.freedesktop.Notifications`, so every Linux app's
  notifications appear.
- **Lock screen:** on the power key and at boot. It replaces getty login on the screen. The PIN or
  password is checked through PAM.
- **On-screen keyboard:** Qt Virtual Keyboard inside the shell, delivering text to apps over the
  Wayland text-input protocol.

### App launching and lifecycle
- Launch: `systemd-run --user --scope` per app, so each app gets its own cgroup, is killable as a
  unit and shows up in `systemd-cgls`. Environment: `WAYLAND_DISPLAY`, `QT_QPA_PLATFORM=wayland`,
  `GDK_BACKEND=wayland`.
- Background apps keep running at first. Later, freeze them with the cgroup freezer to save battery.
- Closing an app sends xdg `close` first and kills its scope after a timeout.

### Session and boot
- `revive-shell.service`: a systemd user service for `phone` that starts on boot through
  autologin on tty1 and replaces getty and buffyboard on screen. The lock screen is the gate.
  SSH and a terminal app keep shell access.
- If `revive-shell` crashes, systemd restarts it. After repeated failures it falls back to the
  text console, so the phone is never left with a dead screen.

## SDK
1. **UI kit:** the QML module `revive` (`import revive`). Theme tokens (colours, type, spacing,
   motion) and components: Page, Header, Button, ListItem, Switch, TextField, Sheet, Toast.
   The shell is built with the same kit.
2. **System API:** the QML module `revive.system`, thin wrappers over freedesktop D-Bus:
   Notifications, Battery, Network, Vibration, and later Telephony and Permissions.
   Ordinary apps can use the same D-Bus APIs directly.
3. **Template and tool:** `revive-sdk new <app>` creates a CMake + QML project.
   `revive-sdk run` builds it for arm64 in the Docker build environment and runs it on a phone
   over SSH. The same project runs on a desktop Linux or WSLg window for quick iteration.
4. **Packaging:** apps ship as `.deb` packages from a revive apt repository. The app market
   (stage 5) builds on that repository. Flatpak can be reconsidered later.

## Default apps
Apps are not rebuilt: Debian already ships them for arm64, and they run in the shell as Wayland
clients. revive ships a curated set that fits a phone screen and touch:

| Role | First choice | Notes |
|---|---|---|
| Terminal | foot (MIT) | fast, Wayland-native. A revive-styled terminal app may replace it later. |
| Browser | Firefox ESR | needs the GPU (phase 3b) |
| Files, calculator, text editor, clock | GNOME adaptive (libadwaita) or KDE Kirigami apps | pick per app by testing on the phone |
| Music and video player | to be picked by testing | video needs the GPU and the venus decoder |
| Hacker kit | cool-retro-term (retro CRT terminal), btop, fastfetch, cmatrix, ranger, nmap, tmux | fits revive's terminal look. cool-retro-term needs the GPU. |
| Messengers | **Signal** (Axolotl or Flare), **Telegram** (telegram-desktop or Paper Plane), **Discord** (Dissent) | all open-source clients. Axolotl/signal-cli can register Signal as the primary device, no Android phone needed. Dissent is an open client for the closed Discord service. |
| Settings | **revive's own**, built with the SDK | first SDK app |
| Phone and SMS | **revive's own**, built with the SDK | once ModemManager works |

Licences: revive's own code (shell, SDK, our apps) is MIT and contains no GPL code. Third-party apps
keep their own licences and ship as separate Debian packages, which is how every Linux distribution
distributes GPL software.

## Repository layout (additions)
```
shell/          revive-shell: CMakeLists.txt, src/*.cpp, qml/*.qml
sdk/            qml/revive (UI kit), qml/revive/system, template/, revive-sdk (tool)
packaging/      Debian packaging for revive-shell, the SDK modules and the Qt dmabuf plugin
```
`build.sh` builds these `.deb` packages in Docker (arm64) and installs them into the rootfs.
During development we deploy to a running phone over SSH, with no reflash.

## Phases (each one testable on the phone)
- **3a. Skeleton:** compositor, status bar, launcher from `.desktop` files, launch an app fullscreen,
  swipe-up home. Boot into it through the user service. The terminal (foot) is the first app.
- **3b. GPU:** build and ship the `linux-dmabuf-unstable-v1` plugin. Verify an app reports
  `RENDERER: FD512` and a browser plays video smoothly.
- **3c. Switcher and system:** app switcher, notifications server, shade with Wi-Fi, battery and brightness.
- **3d. Input:** on-screen keyboard and text-input for Qt and GTK apps.
- **3e. Lock and power:** lock screen (PAM), power key, screen blank, suspend if mainline allows.
- **3f. SDK v0:** UI kit, `revive.system`, template and tool. Rebuild one shell screen and one
  sample app with it.

## Testing
- C++ logic (desktop-entry parsing, app lifecycle) gets QtTest unit tests that run in the build container.
- Each phase ends with a check on the Mi A2: FPS logged by the shell, RAM per process, and the phase's
  feature exercised by hand. Results go in the README.

## Risks and fallbacks
- The dmabuf plugin doesn't build or doesn't work with freedreno: fall back to phoc (wlroots) as the
  compositor, keeping all QML shell code. Only the compositor core is replaced.
- GTK apps use text-input-v3: if Qt's compositor doesn't support it, implement it in the C++ core.
- Suspend on mainline SDM660 may not be stable: start with screen blank only.
