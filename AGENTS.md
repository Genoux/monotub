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
- `desktop/` — Tauri 2 shell: a fixed 720×300, non-resizable frameless window on `https://monotub.tail2e56f0.ts.net` plus a tray icon (left click shows/hides, close hides). No frontend of its own: the panel has a landscape mode (`min-width: 561px` and `max-height: 420px`) that fills that window edge to edge with overflow hidden; the size was picked from screenshots at 640×280 and 720×300 (2026-11-08), where 720×300 fits the readout, temperature and keys on one row.

## Layout

- `scripts/check` — format, lint, types, tests. CI runs exactly this.

## Decisions

- The panel is a PWA served by the server, not a native app. The owner has an iPhone (no native iOS app), and the panel must stay on the tailnet (no public hosting). Same origin as the API means no CORS, and the Tailscale certificate gives the HTTPS iOS needs to install it. The desktop floating widget is a thin Tauri shell loading the same URL; window class `monotub`, floated and pinned by the `monotub-widget` rule in the dotfiles' Hyprland `windowrules.lua`. The window sets `url` explicitly: with only `build.frontendDist` set to the remote URL, Tauri 2 looked for a bundled `index.html` ("asset not found").
- The humidity graph keeps its history in the page (last 2 min of polls); the server stores no history.
- Automation is the linear band model: four RH edges (`rh_dry_below`, `rh_band_low`, `rh_band_high`, `rh_wet_above`) split five bands, and the firmware maps each band to one action. The panel lights the band from `current_band` (the band engaged after the hold time, so it matches the relays) and computes it from the edges only when the firmware does not report one.

<!-- - YYYY-MM-DD: decision. Measured: number, how. Guards against: mistake. -->
- Firmware updates go over Wi-Fi (ArduinoOTA, hostname `monotub`, password `OTA_PASSWORD` in `secrets.h`), after one USB flash; see README › Flashing the ESP32. The esp32dev default partition table already has two OTA slots (`app0`/`app1`, 0x140000 bytes each), so no custom table. Measured 2026-11-08 with `pio run -e esp32dev`: 840,621 bytes, 64.1% of a slot. Guards against: `ArduinoOTA.handle()` blocks for the whole transfer, so the control loop cannot switch relays mid-update; `onStart` turns both off (mist would otherwise keep running unattended) and `onError` hands them back via `controllerForceApplyOutputs()`.

## Project rules

<!-- Rules that apply only here, or a deliberate exception to RULES.md with the reason. -->
