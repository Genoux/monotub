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

## Layout

- `scripts/check` — format, lint, types, tests. CI runs exactly this.

## Decisions

- The panel is a PWA served by the server, not a native app. The owner has an iPhone (no native iOS app), and the panel must stay on the tailnet (no public hosting). Same origin as the API means no CORS, and the Tailscale certificate gives the HTTPS iOS needs to install it. The desktop floating widget (not built yet) is planned as a thin Tauri shell loading the same URL, pinned with a Hyprland window rule (`float, pin`).
- The humidity graph keeps its history in the page (last 2 min of polls); the server stores no history.
- Automation is the linear band model: four RH edges (`rh_dry_below`, `rh_band_low`, `rh_band_high`, `rh_wet_above`) split five bands, and the firmware maps each band to one action. The panel lights the band from `current_band` (the band engaged after the hold time, so it matches the relays) and computes it from the edges only when the firmware does not report one.

<!-- - YYYY-MM-DD: decision. Measured: number, how. Guards against: mistake. -->

## Project rules

<!-- Rules that apply only here, or a deliberate exception to RULES.md with the reason. -->
