# revive: vision

A light, private, general-purpose computing platform for phones that the user fully controls.
revive is built on mainline Linux and Debian, so the OS infrastructure and most applications already
exist. The work is organizing them for mobile devices.

## Principles

| Area | revive |
|---|---|
| OS | Mainline Linux + Debian |
| Resource use | Minimal base services |
| Accounts | Works without any cloud account |
| Telemetry | Off by default, none in the base system |
| Data | Stored locally |
| Apps | An independent App Market plus ordinary Debian packages |
| Updates | Independent of the phone's manufacturer |
| System access | The user has full control (root, SSH, a real shell) |
| Hardware | Modular device profiles (`devices/<codename>/`) |

## App Market

One catalog, four kinds of apps:

- **Native Linux**: arm64 Debian packages, as they are today.
- **Mobile native**: apps built with the revive SDK.
- **Web apps**: PWAs.
- **Sandboxed apps**: isolated execution.

Before an app is installed, the market shows what it asks for: network, camera, microphone,
location, contacts and files. A per-app firewall controls each app's network traffic. Turning
telemetry off is not enough for privacy: apps need real isolation, signed packages and secure
updates.

## The phone as a computer

With a monitor, keyboard and mouse attached (on devices with video out), the same system becomes a
full Linux desktop that runs compilers, IDEs, databases and servers. It is one OS with two
interfaces, not two systems.

## Many devices

Supporting and maintaining many phones is the most expensive part of any mobile Linux project. The
plan is a standardized device porting pipeline: for a new phone, agents study the hardware and
prepare the device tree, kernel config, driver and firmware packaging, the install image and tests.
Physical testing and closed vendor firmware stay the hard part.

## Local AI

If resource use allows, the phone runs its own assistant on the device, with no user data sent to
outside servers.

## Where we are (2026-11-28)

| Goal | Status |
|---|---|
| Mainline kernel + Debian, boot, USB SSH, Wi-Fi, GPU | done on the Mi A2 |
| Own shell: status bar, launcher, dock, home gesture, app switching | done (revive-shell on phoc) |
| Debian apps on the phone (terminal, Calls, Chats, Contacts) | running; calls and SMS wait on the modem |
| On-screen keyboard | phosh-osk-stevia, shown on text input |
| Modem (calls, SMS, data) | modem firmware crashes every ~18 s; diag-router is the lead |
| Lock screen, notifications, quick settings | next |
| SDK v0 | after the shell basics |
| App Market, permissions, per-app firewall, sandboxing | later |
| Desktop convergence | later, on phones with video out |
| More devices, porting pipeline | after the Mi A2 is complete |
