use crate::api::{DeviceState, FanStatus};

pub fn format_humidity_band(status: Option<&FanStatus>) -> String {
    match (
        status.and_then(|s| s.humidity_low_threshold),
        status.and_then(|s| s.humidity_high_threshold),
    ) {
        (Some(low), Some(high)) => format!("band {low:.0}–{high:.0}%"),
        _ => "band —".into(),
    }
}

pub fn format_automation_targets(status: Option<&FanStatus>) -> String {
    let low_persist = status
        .and_then(|s| s.humidity_low_persist_seconds)
        .map(|s| format!("low {s}s"))
        .unwrap_or_else(|| "low —".into());
    let high_persist = status
        .and_then(|s| s.humidity_high_persist_seconds)
        .map(|s| format!("high {s}s"))
        .unwrap_or_else(|| "high —".into());
    let mist_pulse = status
        .and_then(|s| s.mist_pulse_seconds)
        .map(|s| format!("mist {s}s"))
        .unwrap_or_else(|| "mist —".into());
    let fan_pulse = status
        .and_then(|s| s.corrective_fan_seconds)
        .map(|s| format!("fan {s}s"))
        .unwrap_or_else(|| "fan —".into());
    format!("{low_persist}  ·  {high_persist}  ·  {mist_pulse}  ·  {fan_pulse}")
}

pub fn status_stale_warning(state: &DeviceState) -> Option<&'static str> {
    if state.status.is_some() {
        return None;
    }
    match state.connected.as_deref() {
        Some("online") => Some("device online — waiting for status telemetry"),
        _ => None,
    }
}
