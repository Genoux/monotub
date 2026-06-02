# monotub rules

This project controls a monotub with one humidity sensor, one fan relay, and one mist relay.

## Quick Rule Chart

```mermaid
flowchart TD
  A[Automation state] -->|OFF| B[Manual mode]
  B --> B1[Fan: x toggles ON/OFF]
  B --> B2[Mist: m toggles ON/OFF]
  B --> B3[Routine paused]
  B --> B4[Humidity rules inactive]

  A -->|ON| C[Automation mode]
  C --> D{Humidity}

  D -->|Below 85%| E[Too dry]
  E --> E1[Fan: routine paused]
  E --> E2[Mist: ON]

  D -->|85-86%| F[Recovering from dry]
  F --> F1[Fan: routine paused]
  F --> F2[Mist: keep previous state]

  D -->|86-88%| G[Healthy band]
  G --> G1[Fan: routine allowed]
  G --> G2[Mist: keep previous state]

  D -->|88-90%| H[Getting wet]
  H --> H1[Fan: routine paused]
  H --> H2[Mist: OFF]

  D -->|Above 90%| I[Too wet]
  I --> I1[Fan: routine paused]
  I --> I2[Mist: OFF]
  I --> I3{Above 90% for 180s?}
  I3 -->|No| I4[Wait]
  I3 -->|Yes| I5[Fan: 15s corrective pulse]
  I5 --> I6[Fan cooldown: 180s]

  S[Simulation ON] --> S1[Use simulated RH instead of sensor RH]
  S1 --> C

  R[Relay resync] --> R1[ESP32 force-applies current desired relay state]
```

## Who Controls What

The ESP32 is the source of truth for the relays. The server and TUI only send commands and display the latest ESP32 status.

When automation is ON, the fan has two owners:

- Routine fan cycle: normal fresh-air exchange.
- Humidity correction: emergency venting when humidity stays too high.

When automation is OFF:

- Manual fan toggle controls fan.
- Manual mist toggle controls mist.

The mist has two modes:

- Automation ON: humidity rules control mist.
- Automation OFF: manual mist toggle controls mist.

## Humidity Bands

Default humidity targets:

- Hard low: `85%`
- Soft low: `86%`
- Soft high: `88%`
- Hard high: `90%`

Meaning:

- Below `85%`: too dry.
- `85-86%`: recovering from dry.
- `86-88%`: healthy working band.
- `88-90%`: getting wet, mist should stay off.
- Above `90%`: too wet.

## Mist Rules

When automation is ON:

- If humidity is below `85%`, mist turns ON.
- If humidity is above `88%`, mist turns OFF.
- Between `85%` and `88%`, mist keeps its previous state.

That middle zone is intentional. It prevents the mist relay from flickering on and off around one exact number.

When automation is OFF:

- The `m` key manually toggles mist ON/OFF.
- Humidity does not control mist.

When automation is ON, the `m` key is blocked because automation owns mist.

## Fan Routine Rules

The routine fan cycle is for normal fresh-air exchange.

Default routine:

- Fan ON for `30s`
- Fan OFF for `1200s`

The routine only runs inside the soft humidity band:

- Routine allowed: `86-88%`
- Routine paused below `86%`
- Routine paused above `88%`
- Routine paused when automation is OFF

If the routine is paused, the TUI shows the pause reason:

- `humidity low`
- `humidity high`

When the routine resumes, it waits in the OFF phase before the next ON phase.

## High-Humidity Fan Correction

High-humidity correction is separate from the normal routine.

Default correction rules:

- If humidity stays above `90%` for `180s`, run the fan.
- Fan correction pulse length is `15s`.
- After a correction pulse, wait `180s` before another correction pulse can start.

So at `92%`, the fan does not run immediately. The system waits to confirm humidity is truly high, then runs a short fan pulse.

## Automation OFF Rules

When automation is OFF:

- Humidity rules do not control fan or mist.
- Routine is paused.
- Fan is manual with the `x` key.
- Mist is manual.
- Humidity band and routine settings are informational only.

## Simulation Rules

Simulation mode lets you test the rules without changing the real chamber humidity.

In the TUI Sensor section:

- `s` toggles simulation ON/OFF.
- `+` increases simulated humidity.
- `-` decreases simulated humidity.

When simulation is ON:

- The ESP32 uses the simulated humidity instead of the SHT31 humidity.
- The TUI shows `SIM`.
- Automation reacts exactly as if the real sensor had that humidity.

When simulation is OFF:

- The ESP32 goes back to using the real SHT31 sensor.

## Relay Sync Rules

On ESP32 boot:

- Relays are held OFF for `5s`.
- After boot delay, the ESP32 force-applies the desired fan and mist relay states.

In the TUI Device section:

- `y` forces relay resync.

Relay resync is for making the physical relay outputs match the ESP32's current software state.

## TUI Key Rules

Fan section:

- `x` toggles manual fan only when automation is OFF.
- `r` toggles routine ON/OFF.
- `o` edits fan ON seconds.
- `f` edits fan OFF seconds.

Mist section:

- `m` toggles manual mist only when automation is OFF.

Sensor section:

- `s` toggles simulation.
- `+` increases simulated humidity.
- `-` decreases simulated humidity.

Automation section:

- `a` toggles automation ON/OFF.

Device section:

- `y` resyncs relays.

Global:

- `Tab` changes section.
- `l` shows logs.
- `?` shows help.
- `q` quits.
