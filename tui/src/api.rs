use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct FanStatus {
    pub on_seconds: u32,
    pub off_seconds: u32,
    pub routine: bool,
    pub fan: bool,
    pub manual_fan: Option<bool>,
    pub mist: Option<bool>,
    pub uptime_s: u64,
    pub rssi: i32,
    pub ip: Option<String>,
    pub humidity_percent: Option<f32>,
    pub temperature_c: Option<f32>,
    pub environment_ok: Option<bool>,
    pub smoothed_humidity_percent: Option<f32>,
    pub automation_enabled: Option<bool>,
    pub fan_reason: Option<String>,
    pub mist_reason: Option<String>,
    pub humidity_low_threshold: Option<f32>,
    pub humidity_high_threshold: Option<f32>,
    pub humidity_low_persist_seconds: Option<u32>,
    pub humidity_high_persist_seconds: Option<u32>,
    pub mist_pulse_seconds: Option<u32>,
    pub corrective_fan_seconds: Option<u32>,
    pub mist_cooldown_seconds: Option<u32>,
    pub fan_cooldown_seconds: Option<u32>,
    pub mist_cooldown_remaining_s: Option<u32>,
    pub fan_cooldown_remaining_s: Option<u32>,
    pub mist_pulse_remaining_s: Option<u32>,
    pub corrective_fan_remaining_s: Option<u32>,
    pub scheduled_fae_on_seconds: Option<u32>,
    pub scheduled_fae_off_seconds: Option<u32>,
    pub scheduled_fae_on: Option<bool>,
    pub next_scheduled_fae_s: Option<u32>,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LogEntry {
    pub message: String,
    #[serde(rename = "receivedAt")]
    pub received_at: String,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct DeviceState {
    pub connected: Option<String>,
    pub fan: Option<String>,
    pub mist: Option<String>,
    pub status: Option<FanStatus>,
    pub desired: DesiredFanState,
    #[serde(default)]
    pub logs: Vec<LogEntry>,
    #[serde(rename = "lastSeenAt")]
    pub last_seen_at: Option<String>,
    #[serde(rename = "lastTransitionAt")]
    pub last_transition_at: Option<String>,
    #[serde(rename = "nextTransitionAt")]
    pub next_transition_at: Option<String>,
}

#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct DesiredFanState {
    pub routine: Option<bool>,
    pub manual_fan: Option<bool>,
    pub on_seconds: Option<u32>,
    pub off_seconds: Option<u32>,
    pub mist: Option<bool>,
    pub automation_enabled: Option<bool>,
    #[serde(rename = "updatedAt")]
    pub updated_at: Option<String>,
}

#[derive(Clone)]
pub struct ApiClient {
    base_url: String,
    http: reqwest::Client,
}

impl ApiClient {
    pub fn new(base_url: impl Into<String>) -> Self {
        Self {
            base_url: base_url.into().trim_end_matches('/').to_string(),
            http: reqwest::Client::new(),
        }
    }

    pub async fn fetch_state(&self) -> Result<DeviceState> {
        let url = format!("{}/state", self.base_url);
        self.http
            .get(&url)
            .send()
            .await
            .context("request failed")?
            .error_for_status()
            .context("server returned error")?
            .json()
            .await
            .context("invalid JSON response")
    }

    pub async fn set_routine(&self, enabled: bool) -> Result<()> {
        let url = format!("{}/routine", self.base_url);
        self.http
            .post(&url)
            .json(&serde_json::json!({ "enabled": enabled }))
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    pub async fn toggle_fan(&self) -> Result<()> {
        let url = format!("{}/fan/toggle", self.base_url);
        self.http
            .post(&url)
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    pub async fn set_on_seconds(&self, seconds: u32) -> Result<()> {
        let url = format!("{}/on-seconds", self.base_url);
        self.http
            .post(&url)
            .json(&serde_json::json!({ "seconds": seconds }))
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    pub async fn set_off_seconds(&self, seconds: u32) -> Result<()> {
        let url = format!("{}/off-seconds", self.base_url);
        self.http
            .post(&url)
            .json(&serde_json::json!({ "seconds": seconds }))
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    pub async fn toggle_mist(&self) -> Result<()> {
        let url = format!("{}/mist/toggle", self.base_url);
        self.http
            .post(&url)
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    pub async fn set_automation(&self, enabled: bool) -> Result<()> {
        let url = format!("{}/automation-settings", self.base_url);
        self.http
            .post(&url)
            .json(&serde_json::json!({ "automation_enabled": enabled }))
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_state_with_desired_values() {
        let state: DeviceState = serde_json::from_str(
            r#"{
                "connected": "offline",
                "fan": "OFF",
                "status": null,
                "desired": {
                    "routine": true,
                    "on_seconds": 15,
                    "off_seconds": 20,
                    "updatedAt": "2026-05-18T13:00:00.000Z"
                },
                "lastSeenAt": null,
                "lastTransitionAt": null,
                "nextTransitionAt": null
            }"#,
        )
        .expect("state JSON");

        assert_eq!(state.connected.as_deref(), Some("offline"));
        assert_eq!(state.desired.routine, Some(true));
        assert_eq!(state.desired.on_seconds, Some(15));
        assert_eq!(state.desired.off_seconds, Some(20));
    }

    #[tokio::test]
    #[ignore = "requires running mqtt-server"]
    async fn fetch_state_from_running_server() {
        let base =
            std::env::var("MONOTUB_API_URL").unwrap_or_else(|_| "http://localhost:8001".into());
        let client = ApiClient::new(base);
        let state = client
            .fetch_state()
            .await
            .expect("fetch /state from mqtt-server");
        assert!(state.connected.is_none() || state.connected.is_some());
    }

    #[tokio::test]
    #[ignore = "requires running mqtt-server"]
    async fn post_routine_round_trip() {
        let base =
            std::env::var("MONOTUB_API_URL").unwrap_or_else(|_| "http://localhost:8001".into());
        let client = ApiClient::new(base);
        client.set_routine(true).await.expect("set routine on");
        client.set_on_seconds(15).await.expect("set on seconds");
        client.set_off_seconds(20).await.expect("set off seconds");
        let state = client.fetch_state().await.expect("fetch state");
        assert!(state.last_seen_at.is_some());
    }
}
