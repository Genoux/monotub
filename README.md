# monotub

Monotub grow chamber controller — one repo, four components wired together over MQTT.

| Package | Stack | Role |
|---------|-------|------|
| [`esp32/`](esp32/) | PlatformIO / Arduino | On-device firmware — sensors, relays, MQTT client |
| [`server/`](server/) | Bun / Express | MQTT bridge and HTTP API |
| [`dashboard/`](dashboard/) | Next.js | Web UI |
| [`tui/`](tui/) | Rust / ratatui | Terminal UI |

## How it fits together

```
esp32  ──MQTT──▶  server  ◀──HTTP──  dashboard
                    ▲
                    └──HTTP──  tui
```

The ESP32 publishes telemetry and receives commands over MQTT. The server subscribes, exposes REST endpoints, and keeps shared state. Both UIs talk to the server.

## Quick start

Each package has its own toolchain. From the repo root:

```bash
# MQTT bridge + API
cd server && cp .env.example .env && bun install && bun dev

# Web dashboard
cd dashboard && cp .env.example .env.local && bun install && bun dev

# Terminal UI
cd tui && cargo run

# Firmware (requires PlatformIO)
cd esp32 && pio run -t upload
```

Copy `esp32/include/secrets.h.example` to `esp32/include/secrets.h` and fill in WiFi/MQTT credentials before flashing.
