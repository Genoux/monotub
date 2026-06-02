use ratatui::crossterm::event::{KeyCode, KeyEvent};
use ratatui::layout::{Constraint, Layout, Rect};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Clear, Paragraph, Wrap};
use ratatui::Frame;

use crate::app::{App, FocusedSection};
use crate::commands::{AppCommand, EditField};
use crate::sync::{display_bool, format_schedule};
use crate::telemetry::{format_automation_targets, format_humidity_band, status_stale_warning};
use crate::theme;
use crate::time::{format_duration, seconds_since, transition_progress};

pub fn render(frame: &mut Frame, app: &App, area: Rect) {
    if app.show_help {
        render_help_overlay(frame, area);
        return;
    }

    let (body_area, log_area) = if app.show_logs_strip {
        let [body, log] = Layout::vertical([Constraint::Fill(1), Constraint::Fill(1)]).areas(area);
        (body, Some(log))
    } else {
        (area, None)
    };

    let [fan_area, mist_area, sensor_area, auto_area, device_area] = Layout::vertical([
        Constraint::Fill(1),
        Constraint::Fill(1),
        Constraint::Fill(1),
        Constraint::Fill(1),
        Constraint::Fill(1),
    ])
    .areas(body_area);

    render_fan(frame, fan_area, app);
    render_mist(frame, mist_area, app);
    render_sensor(frame, sensor_area, app);
    render_automation(frame, auto_area, app);
    render_device(frame, device_area, app);

    if let Some(log) = log_area {
        render_log_strip(frame, log, app);
    }
}

pub fn handle_key(key: KeyEvent, app: &App) -> Option<AppCommand> {
    if app.edit_field.is_some() {
        return handle_edit_key(key);
    }

    match app.focused_section {
        FocusedSection::Fan => match key.code {
            KeyCode::Char('x') => Some(AppCommand::ToggleFan),
            KeyCode::Char('r') => Some(AppCommand::ToggleRoutine),
            KeyCode::Char('o') => Some(AppCommand::StartEdit(EditField::OnSeconds)),
            KeyCode::Char('f') => Some(AppCommand::StartEdit(EditField::OffSeconds)),
            _ => None,
        },
        FocusedSection::Mist => match key.code {
            KeyCode::Char('m') => Some(AppCommand::ToggleMist),
            _ => None,
        },
        FocusedSection::Automation => match key.code {
            KeyCode::Char('a') => Some(AppCommand::ToggleAutomation),
            _ => None,
        },
        FocusedSection::Device => match key.code {
            KeyCode::Char('y') => Some(AppCommand::ResyncRelays),
            _ => None,
        },
        FocusedSection::Sensor => match key.code {
            KeyCode::Char('s') => Some(AppCommand::ToggleSimulation),
            KeyCode::Char('+') | KeyCode::Char('=') => Some(AppCommand::AdjustSimulatedHumidity(1)),
            KeyCode::Char('-') => Some(AppCommand::AdjustSimulatedHumidity(-1)),
            _ => None,
        },
    }
}

// ── section blocks ────────────────────────────────────────────────────────────

fn section_block(title: &'static str, focused: bool) -> Block<'static> {
    if focused {
        Block::bordered()
            .title(title)
            .border_style(theme::accent_bright())
            .title_style(theme::accent_bright())
    } else {
        Block::bordered()
            .title(title)
            .border_style(theme::muted())
            .title_style(theme::muted())
    }
}

// ── Fan ───────────────────────────────────────────────────────────────────────

fn render_fan(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Fan;
    let block = section_block("Fan Relay", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let Some(state) = app.device_state.as_ref() else {
        frame.render_widget(
            Paragraph::new(Span::styled("waiting for server…", theme::muted())),
            inner,
        );
        return;
    };

    let status = state.status.as_ref();
    let fan_label = state.fan.as_deref().unwrap_or("?");
    let fan_reason = status
        .and_then(|s| s.fan_reason.as_deref())
        .filter(|r| *r != "off")
        .map(|r| format!("  ·  {r}"))
        .unwrap_or_default();

    let routine = display_bool(
        status.map(|s| s.routine),
        state.desired.routine,
        "ON",
        "OFF",
    );
    let schedule = format_schedule(
        status.map(|s| s.on_seconds),
        status.map(|s| s.off_seconds),
        state.desired.on_seconds,
        state.desired.off_seconds,
    );
    let routine_on = status
        .map(|s| s.routine)
        .or(state.desired.routine)
        .unwrap_or(false);
    let automation_owns_fan = status.and_then(|s| s.automation_enabled).unwrap_or(false);
    let manual_fan = display_bool(
        status.and_then(|s| s.manual_fan),
        if automation_owns_fan {
            None
        } else {
            state.desired.manual_fan
        },
        "ON",
        "OFF",
    );
    let routine_paused = status.and_then(|s| s.routine_paused).unwrap_or(false);
    let pause_reason = status
        .and_then(|s| s.routine_pause_reason.as_deref())
        .map(format_pause_reason)
        .unwrap_or("—");

    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("Relay"), theme::muted()),
            Span::styled(fan_label, theme::fan(state.fan.as_deref())),
            Span::styled(fan_reason, theme::muted()),
        ]),
        Line::from(vec![
            Span::styled(label("Routine"), theme::muted()),
            Span::styled(routine.text.clone(), routine.style()),
        ]),
    ];

    if routine_on && routine_paused {
        lines.push(Line::from(vec![
            Span::styled(label("Paused"), theme::muted()),
            Span::styled(pause_reason, theme::warn()),
        ]));
    } else if routine_on {
        let phase_on = status
            .and_then(|s| s.scheduled_fae_on)
            .unwrap_or(matches!(state.fan.as_deref(), Some("ON")));
        let phase = if phase_on { "ON phase" } else { "OFF phase" };
        lines.push(Line::from(vec![
            Span::styled(label("Phase"), theme::muted()),
            Span::styled(phase, theme::accent_bright()),
        ]));
    }

    if automation_owns_fan {
        lines.push(Line::from(vec![
            Span::styled(label("Manual"), theme::muted()),
            Span::styled("disabled while automation is ON", theme::muted()),
        ]));
    } else {
        lines.push(Line::from(vec![
            Span::styled(label("Manual"), theme::muted()),
            Span::styled(manual_fan.text.clone(), manual_fan.style()),
        ]));
    }

    // Cycle progress (only when routine is on and we have transition data)
    if let Some(progress) = transition_progress(state) {
        lines.push(progress_bar_line(
            progress.ratio,
            progress.remaining_seconds,
            progress.next_label,
            inner.width.saturating_sub(2),
        ));
    }

    // Schedule row or edit hint
    match app.edit_field {
        Some(EditField::OnSeconds) => {
            lines.push(Line::from(vec![
                Span::styled(label("ON sec"), theme::muted()),
                Span::styled(app.edit_buffer.clone(), theme::on()),
                Span::styled("_", theme::accent()),
                Span::styled("  Enter save  Esc cancel", theme::muted()),
            ]));
        }
        Some(EditField::OffSeconds) => {
            lines.push(Line::from(vec![
                Span::styled(label("OFF sec"), theme::muted()),
                Span::styled(app.edit_buffer.clone(), theme::off()),
                Span::styled("_", theme::accent()),
                Span::styled("  Enter save  Esc cancel", theme::muted()),
            ]));
        }
        None => {
            lines.push(Line::from(vec![
                Span::styled(label("Schedule"), theme::muted()),
                Span::styled(schedule.text.clone(), schedule.style()),
            ]));
        }
    }

    if let Some(warn) = status_stale_warning(state) {
        lines.push(Line::from(Span::styled(warn, theme::warn())));
    }

    if let Some(err) = app.api_error.as_deref() {
        lines.push(Line::from(Span::styled(err, theme::error())));
    }

    frame.render_widget(Paragraph::new(lines), inner);
}

// ── Mist ──────────────────────────────────────────────────────────────────────

fn render_mist(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Mist;
    let block = section_block("Mist", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let state = app.device_state.as_ref();
    let status = state.and_then(|s| s.status.as_ref());

    let automation_owns_mist = status.and_then(|s| s.automation_enabled).unwrap_or(false);
    let reported_mist = status.and_then(|s| {
        if automation_owns_mist {
            s.automation_mist.or(s.mist)
        } else {
            s.manual_mist.or(s.mist)
        }
    });
    let desired_mist = if automation_owns_mist {
        None
    } else {
        state.and_then(|s| s.desired.mist)
    };
    let mist = display_bool(reported_mist, desired_mist, "ON", "OFF");
    let mist_reason = status
        .and_then(|s| s.mist_reason.as_deref())
        .filter(|r| *r != "off")
        .map(|r| format!("  ·  {r}"))
        .unwrap_or_default();

    let pulse = status
        .and_then(|s| s.mist_pulse_remaining_s)
        .filter(|&s| s > 0)
        .map(|s| format!("pulse {s}s left"))
        .unwrap_or_default();

    let mut lines = vec![Line::from(vec![
        Span::styled(label("State"), theme::muted()),
        Span::styled(mist.text.clone(), mist.style()),
        Span::styled(mist_reason, theme::muted()),
    ])];

    if !pulse.is_empty() {
        lines.push(Line::from(vec![
            Span::styled(label("Pulse"), theme::muted()),
            Span::styled(pulse, theme::accent()),
        ]));
    }

    if automation_owns_mist {
        lines.push(Line::from(vec![
            Span::styled(label("Manual"), theme::muted()),
            Span::styled("disabled while automation is ON", theme::muted()),
        ]));
    }

    if let Some(state) = state {
        if let Some(warn) = status_stale_warning(state) {
            lines.push(Line::from(Span::styled(warn, theme::warn())));
        }
    }

    frame.render_widget(Paragraph::new(lines), inner);
}

// ── Automation ────────────────────────────────────────────────────────────────

fn render_automation(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Automation;
    let block = section_block("Automation", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let state = app.device_state.as_ref();
    let status = state.and_then(|s| s.status.as_ref());

    let auto = display_bool(
        status.and_then(|s| s.automation_enabled),
        state.and_then(|s| s.desired.automation_enabled),
        "ON",
        "OFF",
    );

    let cooldowns = {
        let high = status
            .and_then(|s| s.humidity_high_persist_remaining_s)
            .filter(|&s| s > 0)
            .map(|s| format!("fan in {}", format_duration(s as i64)));
        let fan_cd = status
            .and_then(|s| s.fan_cooldown_remaining_s)
            .filter(|&s| s > 0)
            .map(|s| format!("fan CD {}", format_duration(s as i64)))
            .unwrap_or_else(|| "fan CD 0s".into());
        high.map(|high| format!("{high}  ·  {fan_cd}"))
            .unwrap_or(fan_cd)
    };

    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("State"), theme::muted()),
            Span::styled(auto.text.clone(), auto.style()),
            Span::raw("  "),
            Span::styled(format_humidity_band(status), theme::muted()),
        ]),
        Line::from(vec![
            Span::styled(label("Targets"), theme::muted()),
            Span::styled(format_automation_targets(status), theme::muted()),
        ]),
        Line::from(vec![
            Span::styled(label("Cooldown"), theme::muted()),
            Span::styled(cooldowns, theme::muted()),
        ]),
    ];

    if let Some(state) = state {
        if let Some(warn) = status_stale_warning(state) {
            lines.push(Line::from(Span::styled(warn, theme::warn())));
        }
    }

    frame.render_widget(Paragraph::new(lines), inner);
}

// ── Sensor ────────────────────────────────────────────────────────────────────

fn render_sensor(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Sensor;
    let block = section_block("Sensor", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let status = app.device_state.as_ref().and_then(|s| s.status.as_ref());
    let sensor_ok = status.and_then(|s| s.environment_ok);

    let humidity = status.and_then(|s| s.humidity_percent);
    let temperature = status.and_then(|s| s.temperature_c);
    let rh = humidity
        .map(|h| format!("{h:.1}%"))
        .unwrap_or_else(|| "—".into());
    let temp = temperature
        .map(|t| format!("{t:.1}°C"))
        .unwrap_or_else(|| "—".into());
    let smoothed = status
        .and_then(|s| s.smoothed_humidity_percent)
        .map(|h| format!("smoothed {h:.1}%"))
        .unwrap_or_else(|| "smoothed —".into());
    let simulation = status
        .and_then(|s| s.simulation_enabled)
        .filter(|enabled| *enabled)
        .map(|_| {
            status
                .and_then(|s| s.simulated_humidity_percent)
                .map(|h| format!("SIM {h:.0}%"))
                .unwrap_or_else(|| "SIM".into())
        })
        .unwrap_or_default();

    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("Humidity"), theme::muted()),
            Span::styled(rh, theme::accent_bright()),
            Span::styled("   ", theme::muted()),
            Span::styled(temp, theme::label()),
        ]),
        Line::from(vec![
            Span::styled(label("Status"), theme::muted()),
            Span::styled(
                sensor_status_label(sensor_ok, humidity, temperature),
                sensor_status_style(sensor_ok, humidity, temperature),
            ),
            Span::styled("   ", theme::muted()),
            Span::styled(smoothed, theme::muted()),
            Span::styled("   ", theme::muted()),
            Span::styled(simulation, theme::warn()),
        ]),
    ];

    if status.is_none() {
        if let Some(state) = app.device_state.as_ref() {
            if let Some(warn) = status_stale_warning(state) {
                lines.push(Line::from(Span::styled(warn, theme::warn())));
            }
        }
    }

    frame.render_widget(Paragraph::new(lines), inner);
}

// ── Device ────────────────────────────────────────────────────────────────────

fn render_device(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Device;
    let block = section_block("Device", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let state = app.device_state.as_ref();
    let status = state.and_then(|s| s.status.as_ref());

    let connection = state
        .and_then(|s| s.connected.as_deref())
        .unwrap_or("unknown");
    let ip = status.and_then(|s| s.ip.as_deref()).unwrap_or("—");
    let rssi = status
        .map(|s| format!("{} dBm", s.rssi))
        .unwrap_or_else(|| "—".into());
    let uptime = status
        .map(|s| format!("up {}", format_duration(s.uptime_s as i64)))
        .unwrap_or_else(|| "—".into());
    let latency = app
        .last_poll_latency_ms
        .map(|ms| format!("{ms}ms"))
        .unwrap_or_else(|| "—".into());
    let last_seen = state
        .and_then(|s| s.last_seen_at.as_deref())
        .and_then(|t| seconds_since(Some(t)))
        .map(|s| format!("seen {s}s ago"))
        .unwrap_or_else(|| "—".into());

    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("Network"), theme::muted()),
            Span::styled(
                connection,
                theme::connection(state.and_then(|s| s.connected.as_deref())),
            ),
            Span::styled("   ", theme::muted()),
            Span::styled(ip, theme::label()),
            Span::styled("   ", theme::muted()),
            Span::styled(rssi, theme::muted()),
        ]),
        Line::from(vec![
            Span::styled(label("System"), theme::muted()),
            Span::styled(uptime, theme::muted()),
            Span::styled("   ", theme::muted()),
            Span::styled(last_seen, theme::muted()),
            Span::styled("   ", theme::muted()),
            Span::styled(latency, theme::muted()),
        ]),
    ];

    if let Some(state) = state {
        if let Some(warn) = status_stale_warning(state) {
            lines.push(Line::from(Span::styled(warn, theme::warn())));
        }
    }

    frame.render_widget(Paragraph::new(lines), inner);
}

// ── Log strip ────────────────────────────────────────────────────────────────

fn render_log_strip(frame: &mut Frame, area: Rect, app: &App) {
    let block = section_block("Events", false);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let lines: Vec<Line> = app
        .logs
        .iter()
        .rev()
        .take(inner.height as usize)
        .map(|line| styled_log_line(line))
        .collect();

    frame.render_widget(Paragraph::new(lines), inner);
}

// ── Help overlay ──────────────────────────────────────────────────────────────

fn render_help_overlay(frame: &mut Frame, area: Rect) {
    frame.render_widget(Clear, area);
    let lines = vec![
        Line::from(Span::styled("monotub controls", theme::accent_bright())),
        Line::from(""),
        Line::from(Span::styled("Fan section", theme::muted())),
        help_line("x", "toggle manual fan when automation is OFF"),
        help_line("r", "toggle routine"),
        help_line("o", "edit fan ON seconds"),
        help_line("f", "edit fan OFF seconds"),
        Line::from(""),
        Line::from(Span::styled("Mist section", theme::muted())),
        help_line("m", "toggle mist"),
        Line::from(""),
        Line::from(Span::styled("Sensor section", theme::muted())),
        help_line("s", "toggle humidity simulation"),
        help_line("+ / -", "adjust simulated RH"),
        Line::from(""),
        Line::from(Span::styled("Automation section", theme::muted())),
        help_line("a", "toggle automation"),
        Line::from(""),
        Line::from(Span::styled("Global", theme::muted())),
        help_line("Tab / Shift+Tab", "navigate sections"),
        help_line("y", "resync relays (Device section)"),
        help_line("l", "toggle event log"),
        help_line("?", "toggle this help"),
        help_line("q", "quit"),
        Line::from(""),
        Line::from(Span::styled("any key to close", theme::muted())),
    ];

    frame.render_widget(
        Paragraph::new(lines)
            .block(section_block("Help", true))
            .wrap(Wrap { trim: true }),
        area,
    );
}

// ── helpers ───────────────────────────────────────────────────────────────────

fn label(text: &str) -> String {
    format!("{:<10}", text)
}

fn sensor_status_label(
    sensor_ok: Option<bool>,
    humidity: Option<f32>,
    temperature: Option<f32>,
) -> &'static str {
    match (sensor_ok, humidity.is_some() || temperature.is_some()) {
        (Some(true), _) => "OK",
        (Some(false), false) => "NO READING",
        (Some(false), true) => "ERROR",
        (None, _) => "—",
    }
}

fn sensor_status_style(
    sensor_ok: Option<bool>,
    humidity: Option<f32>,
    temperature: Option<f32>,
) -> ratatui::style::Style {
    match (sensor_ok, humidity.is_some() || temperature.is_some()) {
        (Some(true), _) => theme::on(),
        (Some(false), false) => theme::warn(),
        (Some(false), true) => theme::error(),
        (None, _) => theme::muted(),
    }
}

fn format_pause_reason(reason: &str) -> &'static str {
    match reason {
        "automation_off" => "automation off",
        "humidity_low" => "humidity low",
        "humidity_high" => "humidity high",
        "unknown" => "sensor unknown",
        _ => "—",
    }
}

fn progress_bar_line(
    ratio: f64,
    remaining_seconds: i64,
    next_label: &str,
    width: u16,
) -> Line<'static> {
    let bar_width = (width as usize).saturating_sub(14).max(8);
    let filled = ((bar_width as f64) * ratio.clamp(0.0, 1.0)).round() as usize;
    let filled = if ratio > 0.0 { filled.max(1) } else { filled }.min(bar_width);

    Line::from(vec![
        Span::styled("━".repeat(filled), theme::progress_bar()),
        Span::styled(
            "─".repeat(bar_width.saturating_sub(filled)),
            theme::progress_empty(),
        ),
        Span::styled(
            format!("  {} in {}", next_label, format_duration(remaining_seconds)),
            theme::label(),
        ),
    ])
}

fn help_line(key: &str, action: &str) -> Line<'static> {
    Line::from(vec![
        Span::styled(format!("  {key:<18}"), theme::accent()),
        Span::styled(action.to_string(), theme::text()),
    ])
}

fn handle_edit_key(key: KeyEvent) -> Option<AppCommand> {
    match key.code {
        KeyCode::Esc => Some(AppCommand::CancelEdit),
        KeyCode::Enter => Some(AppCommand::CommitEdit),
        KeyCode::Char(c) if c.is_ascii_digit() => Some(AppCommand::PushEditChar(c)),
        KeyCode::Backspace => Some(AppCommand::BackspaceEdit),
        _ => None,
    }
}

fn styled_log_line(line: &str) -> Line<'_> {
    let lower = line.to_ascii_lowercase();
    let style = if lower.contains("error") || lower.contains("fail") {
        theme::error()
    } else if lower.contains("offline") {
        theme::off()
    } else if lower.contains("online") || lower.contains("recovered") {
        theme::on()
    } else if lower.contains("warn") || lower.contains("stale") {
        theme::warn()
    } else {
        theme::muted()
    };
    Line::from(Span::styled(line, style))
}
