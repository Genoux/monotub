use crate::api::{DeviceState, FanStatus};

pub fn format_humidity_band(status: Option<&FanStatus>) -> String {
    match (
        status.and_then(|s| s.humidity_low_threshold),
        status.and_then(|s| s.humidity_soft_low_threshold),
        status.and_then(|s| s.humidity_soft_high_threshold),
        status.and_then(|s| s.humidity_hard_high_threshold),
    ) {
        (Some(hard_low), Some(soft_low), Some(soft_high), Some(hard_high)) => {
            format!("soft {soft_low:.0}–{soft_high:.0}% hard {hard_low:.0}–{hard_high:.0}%")
        }
        (Some(low), None, Some(high), None) => format!("band {low:.0}–{high:.0}%"),
        _ => "band —".into(),
    }
}

pub fn format_automation_targets(status: Option<&FanStatus>) -> String {
    let high_persist = status
        .and_then(|s| s.humidity_high_persist_seconds)
        .map(|s| format!("mist off >soft high · fan after {s}s hard high"))
        .unwrap_or_else(|| "mist/fan high —".into());
    let fan_pulse = status
        .and_then(|s| s.corrective_fan_seconds)
        .map(|s| format!("fan {s}s"))
        .unwrap_or_else(|| "fan —".into());
    format!("{high_persist}  ·  {fan_pulse}")
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
