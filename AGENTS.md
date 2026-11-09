# AGENTS.md

Notes on why monotub is built the way it is. Record what was measured or
debugged, and the mistake each decision guards against. The shared rules
(git, commits, releases, local-only demos) are in jb-toolkit's `RULES.md` and
are not repeated here; anything below that departs from them says why.

Read `PRODUCT.md` first.

## Stack

- `esp32/` — firmware, C++ on PlatformIO. Owns the automation; everything else only sets and reads it over MQTT.
- `server/` — Bun + Express. MQTT bridge and HTTP API, runs on the homeserver behind Tailscale.
- `server/public/` — the web panel: one static HTML file, no build step, served by the same Express app.
- `tui/` — Rust + ratatui terminal client of the same HTTP API.
- `desktop/` — Tauri 2 shell: a fixed 560×220, non-resizable frameless window plus a tray icon (left click shows/hides, close hides). Its only bundled page is `desktop/ui/index.html`, a "connecting" screen that retries the panel every 3 s and then navigates to `https://monotub.tail2e56f0.ts.net/?widget`; loading the remote URL directly showed WebKit's error page whenever the widget started without the tailnet. The panel is one reflective-LCD design on every platform (band labels, readouts, graph, AUTO/FAN/MIST as switches on the glass); `?widget` only cuts it down to fit 560×220 (no plate or graph labels). A translucent window was tried on 2026-11-08 and dropped: over a wallpaper the panel and background could not be balanced (pane vs background 1.37:1, unlit labels 1.6:1). Icons share that look: `server/public/icon.svg` is the source; `apple-touch-icon.png` (180 px, square, iOS rounds it) and `desktop/icons/icon.png` (512 px, rounded, larger fan so it reads at 22 px in the tray) are rendered from it with `rsvg-convert`. The tray needs the `ksni` feature of `tray-icon`: the default AppIndicator backend never emits left-click events.

## Layout

- `scripts/check` — format, lint, types, tests. CI runs exactly this.

## Decisions

- The panel is a PWA served by the server, not a native app. The owner has an iPhone (no native iOS app), and the panel must stay on the tailnet (no public hosting). Same origin as the API means no CORS, and the Tailscale certificate gives the HTTPS iOS needs to install it. The desktop floating widget is a thin Tauri shell loading the same URL; window class `monotub`, floated and centered by the `monotub-widget` rule in the dotfiles' Hyprland `windowrules.lua`. The window sets `url` explicitly: with only `build.frontendDist` set to the remote URL, Tauri 2 looked for a bundled `index.html` ("asset not found").
- The server ships as `ghcr.io/genoux/monotub-server` and `compose.yaml` deploys it (R8). Mosquitto runs the image's built-in `/mosquitto-no-auth.conf` (anonymous listener on 1883, tailnet-only host) instead of a custom config and bind mount. It keeps no persistent state: the ESP32 republishes its retained state when it reconnects.
- The humidity graph keeps its history in the page (last 2 min of polls); the server stores no history.
- Automation is the linear band model: four RH edges (`rh_dry_below`, `rh_band_low`, `rh_band_high`, `rh_wet_above`) split five bands, and the firmware maps each band to one action. The panel lights the band from `current_band` (the band engaged after the hold time, so it matches the relays) and computes it from the edges only when the firmware does not report one.

<!-- - YYYY-MM-DD: decision. Measured: number, how. Guards against: mistake. -->
- The ESP32 finds the MQTT broker by mDNS name, `homeserver.local` (default `MQTT_HOST`; change it on the board with the serial `mqtt host[:port]` command, saved in NVS). Measured 2026-11-09: the board resolved `ZimaOS.local` and then `homeserver.local` and connected, also after a reset. Guards against: the broker moving to a new IP or router and every board needing a reflash; a replacement server just takes the hostname `homeserver`. The ESP32 is not on Tailscale, so the panel's tailnet URL cannot serve this.
- Wi-Fi is set from a setup hotspot (WiFiManager, `monotub-setup`, WPA2 with `OTA_PASSWORD`) instead of compiled-in credentials only: moving the box to another network needed a USB reflash. It runs non-blocking so the control loop keeps regulating while it is open, and closes after 180 s; the saved network is not retried while it is open, so a router that is slow after a power cut is rejoined within ~3 min. `secrets.h` only seeds a fresh board. Measured 2026-11-09 on the board: with the saved network wiped, the hotspot appeared (psk), 6 sensor readings logged while it was open, credentials sent through the page joined `jb-cabine` and MQTT reconnected; the normal build then booted on the saved network. Firmware with it: 954,185 bytes, 72.8% of an OTA slot. Accepted limits (review 2026-11-09): saving credentials blocks the loop ~18 s with the relays held in their current state, and WiFiManager registers `/update` with no HTTP auth (the menu link is hidden; the route needs the hotspot passphrase, which is `OTA_PASSWORD` anyway) and that upload skips the relays-off guard of ArduinoOTA.
- Firmware updates go over Wi-Fi (ArduinoOTA, hostname `monotub`, password `OTA_PASSWORD` in `secrets.h`), after one USB flash; see README › Flashing the ESP32. The esp32dev default partition table already has two OTA slots (`app0`/`app1`, 0x140000 bytes each), so no custom table. Measured 2026-11-08 with `pio run -e esp32dev`: 840,621 bytes, 64.1% of a slot. First Wi-Fi upload that completed, 2026-11-09: espota pins `--host_port=3233` because the board connects back to the host and ufw dropped the random port (auth succeeded, then "No response from device"). Guards against: `ArduinoOTA.handle()` blocks for the whole transfer, so the control loop cannot switch relays mid-update; `onStart` turns both off (mist would otherwise keep running unattended) and `onError` hands them back via `controllerForceApplyOutputs()`.

## Project rules

<!-- Rules that apply only here, or a deliberate exception to RULES.md with the reason. -->

Every agent follows the shared rules in `.toolkit/RULES.md` (managed by jb-toolkit); deliberate exceptions are in `.jb-exceptions`.
