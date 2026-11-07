use std::cell::Cell;
use std::time::{Duration, Instant};

use anyhow::Result;
use chrono::Local;
use ratatui::crossterm::event::{Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use tokio::sync::{mpsc, watch};

use crate::api::{normalize_server_url, ApiClient, DeviceState, FanStatus};
use crate::commands::AppCommand;
use crate::tabs;

const POLL_INTERVAL: Duration = Duration::from_secs(2);
const LOG_CAPACITY: usize = 500;
const LOG_PAGE: usize = 10;

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
    api_url_tx: watch::Sender<String>,
    pub device_state: Option<DeviceState>,
    pub deadlines: LocalDeadlines,
    pub api_error: Option<String>,
    pub server_url_buffer: String,
    pub server_connecting: bool,
    pub last_poll_at: Option<String>,
    pub last_poll_latency_ms: Option<u128>,
    pub logs: Vec<String>,
    pub log_scroll: usize,
    /// Rows the event strip showed on the last frame; set by the renderer.
    pub log_view_height: Cell<usize>,
    pub should_quit: bool,
    pub show_logs_strip: bool,
    pub show_help: bool,
    pub focused_section: FocusedSection,
    last_esp32_log_at: Option<String>,
    state_rx: mpsc::UnboundedReceiver<TaggedUpdate>,
    state_tx: mpsc::UnboundedSender<TaggedUpdate>,
}

/// The server URL a response came from, so a request still in flight when the
/// URL changes cannot overwrite state fetched from the new server.
struct TaggedUpdate {
    url: String,
    update: StateUpdate,
}

enum StateUpdate {
    Ok {
        state: Box<DeviceState>,
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
    pub fn new(api: ApiClient, api_url_tx: watch::Sender<String>) -> Self {
        let server_url_buffer = api.base_url();
        let (state_tx, state_rx) = mpsc::unbounded_channel();
        Self {
            api,
            api_url_tx,
            device_state: None,
            deadlines: LocalDeadlines::new(),
            api_error: None,
            server_url_buffer,
            server_connecting: false,
            last_poll_at: None,
            last_poll_latency_ms: None,
            logs: vec![format_log("TUI started")],
            log_scroll: 0,
            log_view_height: Cell::new(1),
            should_quit: false,
            show_logs_strip: false,
            show_help: false,
            focused_section: FocusedSection::default(),
            last_esp32_log_at: None,
            state_rx,
            state_tx,
        }
    }

    pub fn needs_server_setup(&self) -> bool {
        self.api_error.is_some() || self.device_state.is_none()
    }

    pub fn is_offline(&self) -> bool {
        if self.needs_server_setup() {
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
            let mut first_poll = true;
            loop {
                let update = poll_state(&api, first_poll).await;
                first_poll = false;
                if tx.send(update).is_err() {
                    break;
                }
                tokio::time::sleep(POLL_INTERVAL).await;
            }
        });
    }

    pub fn drain_state_updates(&mut self) {
        while let Ok(TaggedUpdate { url, update }) = self.state_rx.try_recv() {
            if url != self.api.base_url() {
                continue;
            }
            self.server_connecting = false;
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
                    self.device_state = Some(*state);
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

            if key.modifiers.contains(KeyModifiers::CONTROL) && key.code == KeyCode::Char('c') {
                self.should_quit = true;
                return Ok(());
            }

            // The URL field takes every printable key, so q, l, ?, j, k and Tab
            // must not reach the global shortcuts while it is shown.
            if self.needs_server_setup() {
                if self.show_help {
                    self.show_help = false;
                } else {
                    self.handle_server_setup_key(key);
                }
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

    fn handle_server_setup_key(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Enter => self.commit_server_url(),
            KeyCode::Esc => self.server_url_buffer = self.api.base_url(),
            KeyCode::Backspace => {
                self.server_url_buffer.pop();
            }
            KeyCode::Char(c)
                if !c.is_control() && !key.modifiers.contains(KeyModifiers::CONTROL) =>
            {
                self.server_url_buffer.push(c)
            }
            _ => {}
        }
    }

    fn commit_server_url(&mut self) {
        if self.server_connecting {
            return;
        }

        let normalized = normalize_server_url(&self.server_url_buffer);
        self.server_url_buffer = normalized.clone();
        let _ = self.api_url_tx.send(normalized.clone());
        self.push_log(format!("Connecting to {normalized}"));
        self.server_connecting = true;

        let api = self.api.clone();
        let tx = self.state_tx.clone();
        tokio::spawn(async move {
            let _ = tx.send(poll_state(&api, true).await);
        });
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
                self.log_scroll = 0;
                true
            }
            KeyCode::Up | KeyCode::Char('k') if self.show_logs_strip => {
                self.scroll_logs(1);
                true
            }
            KeyCode::Down | KeyCode::Char('j') if self.show_logs_strip => {
                self.scroll_logs(-1);
                true
            }
            KeyCode::PageUp if self.show_logs_strip => {
                self.scroll_logs(LOG_PAGE as isize);
                true
            }
            KeyCode::PageDown if self.show_logs_strip => {
                self.scroll_logs(-(LOG_PAGE as isize));
                true
            }
            KeyCode::End if self.show_logs_strip => {
                self.log_scroll = 0;
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
        if self.logs.len() > LOG_CAPACITY {
            self.logs.remove(0);
        }
        // Scrolled-back readers keep their place instead of being dragged along.
        if self.log_scroll > 0 {
            self.scroll_logs(1);
        }
    }

    pub fn scroll_logs(&mut self, delta: isize) {
        self.log_scroll = self.log_scroll.saturating_add_signed(delta);
        self.log_scroll = self.log_scroll_offset();
    }

    /// Newest-first offset into the log, clamped so the strip never scrolls
    /// past a full page of the oldest entries (re-clamped when it resizes).
    pub fn log_scroll_offset(&self) -> usize {
        let max_scroll = self.logs.len().saturating_sub(self.log_view_height.get());
        self.log_scroll.min(max_scroll)
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
            let why =
                transition_reason(&previous, state, state.fan.as_deref() == Some("ON"), |s| {
                    s.fan_reason.as_deref()
                });
            self.push_log(format!(
                "Fan relay {} → {}{why}{}",
                previous.fan.as_deref().unwrap_or("?"),
                state.fan.as_deref().unwrap_or("?"),
                humidity_note(state.status.as_ref())
            ));
        }
        if previous.mist != state.mist {
            let why =
                transition_reason(&previous, state, state.mist.as_deref() == Some("ON"), |s| {
                    s.mist_reason.as_deref()
                });
            self.push_log(format!(
                "Mist {} → {}{why}{}",
                previous.mist.as_deref().unwrap_or("?"),
                state.mist.as_deref().unwrap_or("?"),
                humidity_note(state.status.as_ref())
            ));
        }
    }

    fn run_action(&self, action: AppAction) {
        let api = self.api.clone();
        let tx = self.state_tx.clone();
        tokio::spawn(async move {
            let url = api.base_url();
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

            let update = match mutation {
                Err(err) => TaggedUpdate {
                    url,
                    update: StateUpdate::Err {
                        message: err.to_string(),
                        latency_ms: 0,
                    },
                },
                Ok(()) => poll_state(&api, true).await,
            };
            let _ = tx.send(update);
        });
    }
}

// When a relay stops, the reason it was *running* explains the event; the new
// reason is just "off".
fn transition_reason(
    previous: &DeviceState,
    next: &DeviceState,
    turning_on: bool,
    reason: fn(&FanStatus) -> Option<&str>,
) -> String {
    let source = if turning_on { next } else { previous };
    let label = match source.status.as_ref().and_then(reason) {
        Some("manual") => "manual",
        Some("vent") => "vent cycle",
        Some("humidity_automation") => "RH automation",
        _ => return String::new(),
    };
    if turning_on {
        format!("  ·  {label}")
    } else {
        format!("  ·  {label} ended")
    }
}

fn humidity_note(status: Option<&FanStatus>) -> String {
    status
        .and_then(|s| s.smoothed_humidity_percent.or(s.humidity_percent))
        .map(|rh| format!("  ·  RH {rh:.1}%"))
        .unwrap_or_default()
}

fn format_log(message: impl Into<String>) -> String {
    format!("[{}] {}", Local::now().format("%H:%M:%S"), message.into())
}

async fn poll_state(api: &ApiClient, sync_first: bool) -> TaggedUpdate {
    let url = api.base_url();
    let started_at = Instant::now();
    let result = if sync_first {
        request_fresh_state(api).await
    } else {
        api.fetch_state().await
    };
    let latency_ms = started_at.elapsed().as_millis();
    let update = match result {
        Ok(state) => StateUpdate::Ok {
            state: Box::new(state),
            latency_ms,
        },
        Err(err) => StateUpdate::Err {
            message: err.to_string(),
            latency_ms,
        },
    };
    TaggedUpdate { url, update }
}

async fn request_fresh_state(api: &ApiClient) -> Result<DeviceState> {
    api.request_sync().await?;
    tokio::time::sleep(Duration::from_millis(300)).await;
    api.fetch_state().await
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api::DesiredFanState;

    fn test_app() -> App {
        let (api, tx) = ApiClient::new("http://localhost:8001");
        App::new(api, tx)
    }

    fn sample_state(status: Option<FanStatus>) -> DeviceState {
        DeviceState {
            connected: Some("online".into()),
            fan: Some("OFF".into()),
            mist: Some("OFF".into()),
            status,
            desired: DesiredFanState {
                manual_fan: None,
                mist: None,
                automation_enabled: None,
                updated_at: None,
            },
            logs: Vec::new(),
            last_seen_at: None,
            last_transition_at: None,
            next_transition_at: None,
        }
    }

    fn press(app: &mut App, code: KeyCode, modifiers: KeyModifiers) {
        app.handle_event(Event::Key(KeyEvent::new(code, modifiers)))
            .expect("key event");
    }

    #[test]
    fn scroll_clamps_to_viewport_and_stays_anchored_when_new_logs_arrive() {
        let mut app = test_app();
        app.log_view_height.set(3);
        for i in 0..5 {
            app.push_log(format!("entry {i}"));
        }
        assert_eq!(app.log_scroll, 0);

        app.scroll_logs(2);
        assert_eq!(app.log_scroll, 2);

        let anchored = app.logs[app.logs.len() - 1 - app.log_scroll].clone();
        app.push_log("entry 5");
        assert_eq!(app.log_scroll, 3);
        assert_eq!(app.logs[app.logs.len() - 1 - app.log_scroll], anchored);

        app.scroll_logs(999);
        assert_eq!(app.log_scroll, app.logs.len() - 3);
        assert_eq!(app.log_scroll_offset(), app.log_scroll);

        app.log_view_height.set(5);
        assert_eq!(app.log_scroll_offset(), app.logs.len() - 5);

        app.scroll_logs(-999);
        assert_eq!(app.log_scroll, 0);
    }

    #[test]
    fn server_url_field_takes_shortcut_keys_and_ctrl_c_quits() {
        let mut app = test_app();
        app.server_url_buffer.clear();
        assert!(app.needs_server_setup());

        for c in ['q', 'l', '?', 'j', 'k'] {
            press(&mut app, KeyCode::Char(c), KeyModifiers::NONE);
        }
        press(&mut app, KeyCode::Tab, KeyModifiers::NONE);
        press(&mut app, KeyCode::BackTab, KeyModifiers::SHIFT);

        assert_eq!(app.server_url_buffer, "ql?jk");
        assert!(!app.should_quit);
        assert!(!app.show_logs_strip);
        assert!(!app.show_help);
        assert_eq!(app.focused_section, FocusedSection::default());

        press(&mut app, KeyCode::Char('c'), KeyModifiers::CONTROL);
        assert!(app.should_quit);
        assert_eq!(app.server_url_buffer, "ql?jk");
    }

    #[test]
    fn updates_from_a_previous_server_url_are_dropped() {
        let mut app = test_app();
        let _ = app.api_url_tx.send("http://new-server:8001".into());

        let stale = TaggedUpdate {
            url: "http://localhost:8001".into(),
            update: StateUpdate::Err {
                message: "old server down".into(),
                latency_ms: 1,
            },
        };
        app.state_tx.send(stale).expect("send stale");
        app.drain_state_updates();
        assert!(app.api_error.is_none());
        assert!(app.device_state.is_none());

        let current = TaggedUpdate {
            url: app.api.base_url(),
            update: StateUpdate::Ok {
                state: Box::new(sample_state(None)),
                latency_ms: 1,
            },
        };
        app.state_tx.send(current).expect("send current");
        app.drain_state_updates();
        assert!(app.device_state.is_some());
    }

    #[test]
    fn transition_logs_carry_reason_and_humidity() {
        let mut app = test_app();
        let status: FanStatus = serde_json::from_str(
            r#"{"fan":true,"uptime_s":10,"rssi":-50,"fan_reason":"vent",
                "smoothed_humidity_percent":87.4}"#,
        )
        .expect("status JSON");
        let previous = sample_state(Some(status));
        let mut next = previous.clone();
        next.fan = Some("ON".into());

        app.device_state = Some(previous);
        app.log_state_update(&next);

        let line = app.logs.last().expect("log line");
        assert!(line.contains("Fan relay OFF → ON"), "{line}");
        assert!(line.contains("vent cycle"), "{line}");
        assert!(line.contains("RH 87.4%"), "{line}");
    }
}
