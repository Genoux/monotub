use chrono::{DateTime, Utc};

use crate::api::DeviceState;

pub struct TransitionProgress {
    pub ratio: f64,
    pub remaining_seconds: i64,
    pub next_label: &'static str,
}

pub fn transition_progress(state: &DeviceState) -> Option<TransitionProgress> {
    let status = state.status.as_ref()?;
    if !status.routine {
        return None;
    }

    let phase_on = status
        .scheduled_fae_on
        .unwrap_or(matches!(state.fan.as_deref(), Some("ON")));
    let next_label = if phase_on { "OFF" } else { "ON" };

    // Server syncs these from ESP32 next_scheduled_fae_s on each status message.
    if let (Some(start), Some(end)) = (
        parse_iso(state.last_transition_at.as_deref()?),
        parse_iso(state.next_transition_at.as_deref()?),
    ) {
        let total_seconds = (end - start).num_seconds().max(1);
        let elapsed_seconds = (Utc::now() - start).num_seconds().clamp(0, total_seconds);
        let remaining_seconds = (end - Utc::now()).num_seconds().max(0);
        let ratio = elapsed_seconds as f64 / total_seconds as f64;
        return Some(TransitionProgress {
            ratio,
            remaining_seconds,
            next_label,
        });
    }

    if let Some(remaining) = status.next_scheduled_fae_s {
        let duration = if phase_on {
            status.on_seconds
        } else {
            status.off_seconds
        }
        .max(1) as i64;
        let remaining_seconds = i64::from(remaining);
        let elapsed = (duration - remaining_seconds).max(0);
        let ratio = (elapsed as f64 / duration as f64).clamp(0.0, 1.0);
        return Some(TransitionProgress {
            ratio,
            remaining_seconds,
            next_label,
        });
    }

    None
}

pub fn seconds_since(iso: Option<&str>) -> Option<i64> {
    parse_iso(iso?).map(|time| (Utc::now() - time).num_seconds().max(0))
}

pub fn format_duration(seconds: i64) -> String {
    if seconds < 60 {
        return format!("{seconds}s");
    }

    let minutes = seconds / 60;
    let seconds = seconds % 60;
    if minutes < 60 {
        return format!("{minutes}:{seconds:02}");
    }

    let hours = minutes / 60;
    let minutes = minutes % 60;
    format!("{hours}h {minutes:02}m")
}

fn parse_iso(value: &str) -> Option<DateTime<Utc>> {
    DateTime::parse_from_rfc3339(value)
        .ok()
        .map(|time| time.with_timezone(&Utc))
}
