use crate::api::DeviceState;

pub fn status_stale_warning(state: &DeviceState) -> Option<&'static str> {
    if state.status.is_some() {
        return None;
    }
    match state.connected.as_deref() {
        Some("online") => Some("device online — waiting for status telemetry"),
        _ => None,
    }
}
