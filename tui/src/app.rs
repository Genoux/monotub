use std::time::{Duration, Instant};

use anyhow::Result;
use chrono::Local;
use ratatui::crossterm::event::{Event, KeyCode, KeyEvent, KeyEventKind};
use tokio::sync::mpsc;

use crate::api::{ApiClient, DeviceState};
use crate::commands::AppCommand;
use crate::tabs;

const POLL_INTERVAL: Duration = Duration::from_secs(2);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum FocusedSection {
    #[default]
    Fan,
    Mist,
    Automation,
    Sensor,
    Device,
}

impl FocusedSection {
    pub fn next(self) -> Self {
        match self {
            Self::Fan => Self::Mist,
            Self::Mist => Self::Sensor,
            Self::Sensor => Self::Automation,
            Self::Automation => Self::Device,
            Self::Device => Self::Fan,
        }
    }

    pub fn prev(self) -> Self {
        match self {
            Self::Fan => Self::Device,
            Self::Mist => Self::Fan,
            Self::Sensor => Self::Mist,
            Self::Automation => Self::Sensor,
            Self::Device => Self::Automation,
        }
    }

    pub fn commands(self) -> &'static str {
        match self {
            Self::Fan => "x manual fan",
            Self::Mist => "m manual mist",
            Self::Automation => "a toggle auto",
            Self::Device => "y resync relays",
            Self::Sensor => "s sim  +/- sim RH",
        }
    }
}

/// Local deadlines derived from the most recent server status, updated every poll.
/// Used by the renderer to interpolate smooth countdowns every 100ms frame instead
/// of snapping to server values every 2s.
pub struct LocalDeadlines {
    /// When the current band hold expires (None = settled, no hold pending).
    pub band_hold: Option<Instant>,
    /// Wall-clock instant we last received a valid status from the device.
    pub last_device_contact: Option<Instant>,
}

impl LocalDeadlines {
    fn new() -> Self {
        Self {
            band_hold: None,
            last_device_contact: None,
        }
    }

    fn update_from_status(&mut self, status: &crate::api::FanStatus) {
        let now = Instant::now();
        self.last_device_contact = Some(now);

        self.band_hold = status
            .band_hold_remaining_s
            .filter(|&s| s > 0)
            .map(|s| now + Duration::from_secs(s as u64));
    }
}

pub struct App {
    pub api: ApiClient,
    pub device_state: Option<DeviceState>,
    pub deadlines: LocalDeadlines,
    pub api_error: Option<String>,
    pub last_poll_at: Option<String>,
    pub last_poll_latency_ms: Option<u128>,
    pub logs: Vec<String>,
    pub should_quit: bool,
    pub show_logs_strip: bool,
    pub show_help: bool,
    pub focused_section: FocusedSection,
    last_esp32_log_at: Option<String>,
    state_rx: mpsc::UnboundedReceiver<StateUpdate>,
    state_tx: mpsc::UnboundedSender<StateUpdate>,
}

enum StateUpdate {
    Ok {
        state: DeviceState,
        latency_ms: u128,
    },
    Err {
        message: String,
        latency_ms: u128,
    },
}

enum AppAction {
    ToggleFan,
    ToggleMist,
    SetAutomation(bool),
    ResyncRelays,
    SetSimulation(bool),
    SetSimulatedHumidity(f32),
}

impl App {
    pub fn new(api: ApiClient) -> Self {
        let (state_tx, state_rx) = mpsc::unbounded_channel();
        Self {
            api,
            device_state: None,
            deadlines: LocalDeadlines::new(),
            api_error: None,
            last_poll_at: None,
            last_poll_latency_ms: None,
            logs: vec![format_log("TUI started")],
            should_quit: false,
            show_logs_strip: false,
            show_help: false,
            focused_section: FocusedSection::default(),
            last_esp32_log_at: None,
            state_rx,
            state_tx,
        }
    }

    pub fn is_offline(&self) -> bool {
        if self.api_error.is_some() || self.device_state.is_none() {
            return true;
        }
        if self
            .device_state
            .as_ref()
            .and_then(|s| s.connected.as_deref())
            .map(|c| c == "offline")
            .unwrap_or(false)
        {
            return true;
        }
        if self
            .deadlines
            .last_device_contact
            .map(|t| t.elapsed().as_secs() > 10)
            .unwrap_or(false)
        {
            return true;
        }
        false
    }

    pub fn spawn_poller(&self) {
        let api = self.api.clone();
        let tx = self.state_tx.clone();
        tokio::spawn(async move {
            loop {
                let started_at = Instant::now();
                let update = match request_fresh_state(&api).await {
                    Ok(state) => StateUpdate::Ok {
                        state,
                        latency_ms: started_at.elapsed().as_millis(),
                    },
                    Err(err) => StateUpdate::Err {
                        message: err.to_string(),
                        latency_ms: started_at.elapsed().as_millis(),
                    },
                };
                if tx.send(update).is_err() {
                    break;
                }
                tokio::time::sleep(POLL_INTERVAL).await;
            }
        });
    }

    pub fn drain_state_updates(&mut self) {
        while let Ok(update) = self.state_rx.try_recv() {
            self.last_poll_at = Some(Local::now().format("%H:%M:%S").to_string());
            match update {
                StateUpdate::Ok { state, latency_ms } => {
                    self.last_poll_latency_ms = Some(latency_ms);
                    if self.api_error.is_some() {
                        self.push_log("API connection recovered");
                    }
                    self.log_state_update(&state);
                    if let Some(status) = &state.status {
                        self.deadlines.update_from_status(status);
                    }
                    self.device_state = Some(state);
                    self.api_error = None;
                }
                StateUpdate::Err {
                    message,
                    latency_ms,
                } => {
                    self.last_poll_latency_ms = Some(latency_ms);
                    if self.api_error.as_deref() != Some(message.as_str()) {
                        self.push_log(format!("API error: {message}"));
                    }
                    self.api_error = Some(message);
                }
            }
        }
    }

    pub fn handle_event(&mut self, event: Event) -> Result<()> {
        if let Event::Key(key) = event {
            if key.kind != KeyEventKind::Press {
                return Ok(());
            }

            if self.api_error.is_some() {
                self.handle_global_key(key);
                return Ok(());
            }

            if self.handle_global_key(key) {
                return Ok(());
            }
            if let Some(command) = tabs::handle_key(key, self) {
                self.apply_command(command);
            }
        }
        Ok(())
    }

    fn apply_command(&mut self, command: AppCommand) {
        match command {
            AppCommand::ToggleFan => self.toggle_fan(),
            AppCommand::ToggleMist => self.toggle_mist(),
            AppCommand::ToggleAutomation => self.toggle_automation(),
            AppCommand::ResyncRelays => self.resync_relays(),
            AppCommand::ToggleSimulation => self.toggle_simulation(),
            AppCommand::AdjustSimulatedHumidity(delta) => self.adjust_simulated_humidity(delta),
        }
    }

    fn handle_global_key(&mut self, key: KeyEvent) -> bool {
        // Any key closes help overlay
        if self.show_help {
            self.show_help = false;
            return true;
        }

        match key.code {
            KeyCode::Char('q') => {
                self.should_quit = true;
                true
            }
            KeyCode::Char('?') => {
                self.show_help = true;
                true
            }
            KeyCode::Char('l') => {
                self.show_logs_strip = !self.show_logs_strip;
                true
            }
            KeyCode::Tab => {
                self.focused_section = self.focused_section.next();
                true
            }
            KeyCode::BackTab => {
                self.focused_section = self.focused_section.prev();
                true
            }
            _ => false,
        }
    }

    pub fn toggle_fan(&mut self) {
        let automation_enabled = self
            .device_state
            .as_ref()
            .and_then(|s| {
                s.desired.automation_enabled.or_else(|| {
                    s.status
                        .as_ref()
                        .and_then(|status| status.automation_enabled)
                })
            })
            .unwrap_or(false);
        if automation_enabled {
            self.push_log("Grow mode: fan is automatic — turn automation OFF, then use x");
            return;
        }
        self.push_log("Toggle fan");
        self.run_action(AppAction::ToggleFan);
    }

    pub fn toggle_mist(&mut self) {
        let automation_enabled = self
            .device_state
            .as_ref()
            .and_then(|s| {
                s.desired.automation_enabled.or_else(|| {
                    s.status
                        .as_ref()
                        .and_then(|status| status.automation_enabled)
                })
            })
            .unwrap_or(false);
        if automation_enabled {
            self.push_log("Grow mode: mist is automatic — turn automation OFF, then use m");
            return;
        }
        self.push_log("Toggle mist");
        self.run_action(AppAction::ToggleMist);
    }

    pub fn toggle_automation(&mut self) {
        let enabled = self
            .device_state
            .as_ref()
            .and_then(|s| {
                s.desired.automation_enabled.or_else(|| {
                    s.status
                        .as_ref()
                        .and_then(|status| status.automation_enabled)
                })
            })
            .map(|automation| !automation)
            .unwrap_or(true);
        self.push_log(format!(
            "Set automation {}",
            if enabled { "ON" } else { "OFF" }
        ));
        self.run_action(AppAction::SetAutomation(enabled));
    }

    pub fn resync_relays(&mut self) {
        self.push_log("Resync relays");
        self.run_action(AppAction::ResyncRelays);
    }

    pub fn toggle_simulation(&mut self) {
        let enabled = self
            .device_state
            .as_ref()
            .and_then(|s| {
                s.status
                    .as_ref()
                    .and_then(|status| status.simulation_enabled)
            })
            .map(|enabled| !enabled)
            .unwrap_or(true);
        self.push_log(format!(
            "Set simulation {}",
            if enabled { "ON" } else { "OFF" }
        ));
        if self.focused_section != FocusedSection::Sensor {
            self.push_log("Tip: Tab to Sensor section before pressing s");
        }
        self.run_action(AppAction::SetSimulation(enabled));
    }

    pub fn adjust_simulated_humidity(&mut self, delta: i8) {
        let current = self
            .device_state
            .as_ref()
            .and_then(|s| {
                s.status
                    .as_ref()
                    .and_then(|status| status.simulated_humidity_percent)
                    .or_else(|| s.status.as_ref().and_then(|status| status.humidity_percent))
            })
            .unwrap_or(88.0);
        let next = (current + f32::from(delta)).clamp(1.0, 100.0);
        self.push_log(format!("Set simulated RH to {next:.0}%"));
        self.run_action(AppAction::SetSimulatedHumidity(next));
    }

    pub fn push_log(&mut self, message: impl Into<String>) {
        self.logs.push(format_log(message));
        if self.logs.len() > 100 {
            self.logs.remove(0);
        }
    }

    fn log_state_update(&mut self, state: &DeviceState) {
        // Push any ESP32 log entries we haven't seen yet (server prepends newest first).
        let new_entries: Vec<&str> = state
            .logs
            .iter()
            .take_while(|e| Some(e.received_at.as_str()) != self.last_esp32_log_at.as_deref())
            .map(|e| e.message.as_str())
            .collect();
        if let Some(newest) = state.logs.first() {
            self.last_esp32_log_at = Some(newest.received_at.clone());
        }
        for msg in new_entries.into_iter().rev() {
            self.push_log(format!("esp32: {msg}"));
        }

        let Some(previous) = self.device_state.clone() else {
            self.push_log(format!(
                "Synced: fan relay {} mist {}",
                state.fan.as_deref().unwrap_or("?"),
                state.mist.as_deref().unwrap_or("?")
            ));
            return;
        };

        if previous.connected != state.connected {
            self.push_log(format!(
                "Device {} → {}",
                previous.connected.as_deref().unwrap_or("?"),
                state.connected.as_deref().unwrap_or("?")
            ));
        }
        if previous.fan != state.fan {
            self.push_log(format!(
                "Fan relay {} → {}",
                previous.fan.as_deref().unwrap_or("?"),
                state.fan.as_deref().unwrap_or("?")
            ));
        }
        if previous.mist != state.mist {
            self.push_log(format!(
                "Mist {} → {}",
                previous.mist.as_deref().unwrap_or("?"),
                state.mist.as_deref().unwrap_or("?")
            ));
        }
    }

    fn run_action(&self, action: AppAction) {
        let api = self.api.clone();
        let tx = self.state_tx.clone();
        tokio::spawn(async move {
            let mutation = match action {
                AppAction::ToggleFan => api.toggle_fan().await,
                AppAction::ToggleMist => api.toggle_mist().await,
                AppAction::SetAutomation(enabled) => api.set_automation(enabled).await,
                AppAction::ResyncRelays => api.resync_relays().await,
                AppAction::SetSimulation(enabled) => api.set_simulation(enabled).await,
                AppAction::SetSimulatedHumidity(humidity) => {
                    api.set_simulated_humidity(humidity).await
                }
            };

            if let Err(err) = mutation {
                let _ = tx.send(StateUpdate::Err {
                    message: err.to_string(),
                    latency_ms: 0,
                });
                return;
            }

            let started_at = Instant::now();
            match request_fresh_state(&api).await {
                Ok(state) => {
                    let _ = tx.send(StateUpdate::Ok {
                        state,
                        latency_ms: started_at.elapsed().as_millis(),
                    });
                }
                Err(err) => {
                    let _ = tx.send(StateUpdate::Err {
                        message: err.to_string(),
                        latency_ms: started_at.elapsed().as_millis(),
                    });
                }
            }
        });
    }
}

fn format_log(message: impl Into<String>) -> String {
    format!("[{}] {}", Local::now().format("%H:%M:%S"), message.into())
}

async fn request_fresh_state(api: &ApiClient) -> Result<DeviceState> {
    api.request_sync().await?;
    tokio::time::sleep(Duration::from_millis(300)).await;
    api.fetch_state().await
}
