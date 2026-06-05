use ratatui::crossterm::event::{KeyCode, KeyEvent};
use ratatui::layout::{Alignment, Constraint, Layout, Rect};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Clear, Paragraph, Wrap};
use ratatui::Frame;

use crate::app::{App, FocusedSection};
use crate::commands::{AppCommand, EditField};
use crate::sync::{display_bool, format_schedule};
use crate::telemetry::status_stale_warning;
use crate::theme;
use crate::time::{format_duration, seconds_since, transition_progress};

pub fn render(frame: &mut Frame, app: &App, area: Rect) {
    if app.show_help {
        render_help_overlay(frame, area);
        return;
    }

    if app.api_error.is_some() || app.device_state.is_none() {
        render_center_status(frame, area, app);
        return;
    }

    let (body_area, log_area) = if app.show_logs_strip {
        let [body, log] = Layout::vertical([Constraint::Fill(1), Constraint::Fill(1)]).areas(area);
        (body, Some(log))
    } else {
        (area, None)
    };

    let [mode_area, controls_area] =
        Layout::vertical([Constraint::Length(1), Constraint::Fill(1)]).areas(body_area);

    let [fan_area, mist_area, sensor_area, auto_area, device_area] = Layout::vertical([
        Constraint::Fill(1),
        Constraint::Fill(1),
        Constraint::Fill(1),
        Constraint::Fill(1),
        Constraint::Fill(1),
    ])
    .areas(controls_area);

    render_mode_bar(frame, mode_area, app);
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

fn render_center_status(frame: &mut Frame, area: Rect, app: &App) {
    let center = centered_area(area, 3);
    let lines = match app.api_error.as_deref() {
        Some(_) => vec![
            Line::from(Span::styled("( ͡° ͜ʖ ͡°)", theme::error())),
            Line::from(""),
            Line::from(Span::styled("Server unreachable", theme::error())),
            Line::from(Span::styled(
                "waiting for http://localhost:8001",
                theme::muted(),
            )),
        ],
        None => vec![Line::from(Span::styled(
            "waiting for server…",
            theme::muted(),
        ))],
    };

    frame.render_widget(
        Paragraph::new(lines)
            .alignment(Alignment::Center)
            .wrap(Wrap { trim: true }),
        center,
    );
}

fn centered_area(area: Rect, height: u16) -> Rect {
    let height = height.min(area.height);
    Rect {
        x: area.x,
        y: area.y + area.height.saturating_sub(height) / 2,
        width: area.width,
        height,
    }
}

fn render_mode_bar(frame: &mut Frame, area: Rect, app: &App) {
    let automation_on = app
        .device_state
        .as_ref()
        .map(automation_enabled)
        .unwrap_or(false);
    let (mode, detail, style) = if automation_on {
        ("AUTO MODE", "humidity + routine", theme::on())
    } else {
        ("MANUAL MODE", "service controls", theme::accent_bright())
    };

    frame.render_widget(
        Paragraph::new(Line::from(vec![
            Span::styled(mode, style),
            Span::styled("  ", theme::muted()),
            Span::styled(detail, theme::muted()),
        ])),
        area,
    );
}

// ── Fan ───────────────────────────────────────────────────────────────────────

fn render_fan(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Fan;
    let automation_on = app
        .device_state
        .as_ref()
        .map(automation_enabled)
        .unwrap_or(false);
    let block = section_block("Fan", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let Some(state) = app.device_state.as_ref() else {
        frame.render_widget(
            Paragraph::new(Span::styled("waiting for server…", theme::muted())),
            inner,
        );
        return;
    };

    let lines = if automation_on {
        fan_auto_lines(state, app, inner.width)
    } else {
        fan_manual_lines(state, app)
    };

    frame.render_widget(Paragraph::new(lines), inner);
}

fn fan_auto_lines<'a>(
    state: &'a crate::api::DeviceState,
    app: &'a App,
    inner_width: u16,
) -> Vec<Line<'a>> {
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

    if let Some(progress) = transition_progress(state) {
        lines.push(progress_bar_line(
            progress.ratio,
            progress.remaining_seconds,
            progress.next_label,
            inner_width.saturating_sub(2),
        ));
    }

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

    append_fan_status_lines(&mut lines, state, app);
    lines
}

fn fan_manual_lines<'a>(state: &'a crate::api::DeviceState, app: &'a App) -> Vec<Line<'a>> {
    let fan_label = state.fan.as_deref().unwrap_or("?");

    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("State"), theme::muted()),
            Span::styled(fan_label, theme::fan(state.fan.as_deref())),
        ]),
    ];
    append_fan_status_lines(&mut lines, state, app);
    lines
}

fn append_fan_status_lines<'a>(
    lines: &mut Vec<Line<'a>>,
    state: &'a crate::api::DeviceState,
    app: &'a App,
) {
    if let Some(warn) = status_stale_warning(state) {
        lines.push(Line::from(Span::styled(warn, theme::warn())));
    }
    if let Some(err) = app.api_error.as_deref() {
        lines.push(Line::from(Span::styled(err.to_string(), theme::error())));
    }
}

// ── Mist ──────────────────────────────────────────────────────────────────────

fn render_mist(frame: &mut Frame, area: Rect, app: &App) {
    let focused = app.focused_section == FocusedSection::Mist;
    let automation_on = app
        .device_state
        .as_ref()
        .map(automation_enabled)
        .unwrap_or(false);
    let block = section_block("Mist", focused);
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let state = app.device_state.as_ref();
    let status = state.and_then(|s| s.status.as_ref());

    let reported_mist = status.and_then(|s| {
        if automation_on {
            s.automation_mist.or(s.mist)
        } else {
            s.manual_mist.or(s.mist)
        }
    });
    let desired_mist = if automation_on {
        None
    } else {
        state.and_then(|s| s.desired.mist)
    };
    let mist = display_bool(reported_mist, desired_mist, "ON", "OFF");
    let pulse = status
        .and_then(|s| s.mist_pulse_remaining_s)
        .filter(|&s| s > 0)
        .map(|s| format!("pulse {s}s left"))
        .unwrap_or_default();

    let mut lines = vec![Line::from(vec![
        Span::styled(label("State"), theme::muted()),
        Span::styled(mist.text.clone(), mist.style()),
    ])];

    if automation_on {
        if !pulse.is_empty() {
            lines.push(Line::from(vec![
                Span::styled(label("Pulse"), theme::muted()),
                Span::styled(pulse, theme::accent()),
            ]));
        }
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

    let automation_on = state.map(automation_enabled).unwrap_or(false);
    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("State"), theme::muted()),
            Span::styled(auto.text.clone(), auto.style()),
        ]),
    ];

    if automation_on {
        lines.extend([
            Line::from(vec![
                Span::styled(label("RH"), theme::muted()),
                Span::styled(format_rh_rule(status), theme::muted()),
            ]),
            Line::from(vec![
                Span::styled(label("Mist"), theme::muted()),
                Span::styled(format_mist_rule(status), theme::muted()),
            ]),
            Line::from(vec![
                Span::styled(label("Fan"), theme::muted()),
                Span::styled(format_fan_rule(status), theme::muted()),
            ]),
            Line::from(vec![
                Span::styled(label("Vent"), theme::muted()),
                Span::styled(format_vent_rule(status), theme::muted()),
            ]),
        ]);
    }

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
    let simulation_on = status.and_then(|s| s.simulation_enabled).unwrap_or(false);
    let sensor_ok = status.and_then(|s| s.environment_ok);

    let humidity = if simulation_on {
        status.and_then(|s| {
            s.humidity_percent
                .or(s.simulated_humidity_percent)
                .or(s.smoothed_humidity_percent)
        })
    } else {
        status.and_then(|s| s.humidity_percent)
    };
    let temperature = status.and_then(|s| s.temperature_c);
    let rh = humidity
        .map(|h| format!("{h:.1}%"))
        .unwrap_or_else(|| "—".into());
    let temp = temperature
        .map(|t| format!("{t:.1}°C"))
        .unwrap_or_else(|| "—".into());
    let smoothed = if simulation_on {
        humidity
            .map(|h| format!("sim {h:.1}%"))
            .unwrap_or_else(|| "sim —".into())
    } else {
        status
            .and_then(|s| s.smoothed_humidity_percent)
            .map(|h| format!("smoothed {h:.1}%"))
            .unwrap_or_else(|| "smoothed —".into())
    };
    let (status_label, status_style) = if simulation_on {
        ("SIM", theme::warn())
    } else {
        (
            sensor_status_label(sensor_ok, humidity, temperature),
            sensor_status_style(sensor_ok, humidity, temperature),
        )
    };

    let mut lines = vec![
        Line::from(vec![
            Span::styled(label("Humidity"), theme::muted()),
            Span::styled(rh, theme::accent_bright()),
            Span::styled("   ", theme::muted()),
            Span::styled(temp, theme::label()),
        ]),
        Line::from(vec![
            Span::styled(label("Status"), theme::muted()),
            Span::styled(status_label, status_style),
            Span::styled("   ", theme::muted()),
            Span::styled(smoothed, theme::muted()),
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
        Line::from(Span::styled("Automation (master)", theme::muted())),
        help_line("a", "ON = grow mode (RH mist, routine, fan pulses)"),
        help_line("", "OFF = manual fan x and mist m only"),
        Line::from(""),
        Line::from(Span::styled("Fan — Auto view (automation ON)", theme::muted())),
        help_line("o / f", "edit routine ON/OFF seconds"),
        Line::from(""),
        Line::from(Span::styled("Fan — Manual view (automation OFF)", theme::muted())),
        help_line("x", "toggle fan relay"),
        Line::from(""),
        Line::from(Span::styled("Mist — Manual view (automation OFF)", theme::muted())),
        help_line("m", "toggle mist relay"),
        Line::from(""),
        Line::from(Span::styled("Sensor section", theme::muted())),
        help_line("s", "toggle humidity simulation"),
        help_line("+ / -", "adjust simulated RH"),
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

fn automation_enabled(state: &crate::api::DeviceState) -> bool {
    state
        .desired
        .automation_enabled
        .or_else(|| state.status.as_ref().and_then(|s| s.automation_enabled))
        .unwrap_or(false)
}

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

fn format_rh_rule(status: Option<&crate::api::FanStatus>) -> String {
    match (
        status.and_then(|s| s.humidity_low_threshold),
        status.and_then(|s| s.humidity_high_threshold),
    ) {
        (Some(low), Some(high)) => format!("{low:.0}-{high:.0}%"),
        _ => "—".into(),
    }
}

fn format_mist_rule(status: Option<&crate::api::FanStatus>) -> String {
    match (
        status.and_then(|s| s.humidity_low_threshold),
        status.and_then(|s| s.humidity_soft_high_threshold),
    ) {
        (Some(low), Some(soft_high)) => format!("ON <{low:.0}%  ·  OFF >{soft_high:.0}%"),
        _ => "auto by RH".into(),
    }
}

fn format_fan_rule(status: Option<&crate::api::FanStatus>) -> String {
    match (
        status.and_then(|s| s.humidity_soft_low_threshold),
        status.and_then(|s| s.humidity_soft_high_threshold),
        status.and_then(|s| s.humidity_high_threshold),
    ) {
        (Some(soft_low), Some(soft_high), Some(high)) => {
            format!("routine {soft_low:.0}-{soft_high:.0}%  ·  vent >{high:.0}%")
        }
        _ => "routine + high RH vent".into(),
    }
}

fn format_vent_rule(status: Option<&crate::api::FanStatus>) -> String {
    let fan_pulse = status
        .and_then(|s| s.corrective_fan_seconds)
        .map(|s| format!("{s}s pulse"))
        .unwrap_or_else(|| "—".into());
    let fan_cd = status
        .and_then(|s| s.fan_cooldown_remaining_s)
        .filter(|&s| s > 0)
        .map(|s| format_duration(s as i64))
        .unwrap_or_else(|| "0s".into());
    let pulses = match (
        status.and_then(|s| s.corrective_fan_pulse_count),
        status.and_then(|s| s.corrective_fan_pulse_limit),
    ) {
        (Some(count), Some(limit)) => format!("{count}/{limit} used this hour"),
        _ => "— used this hour".into(),
    };
    format!("{fan_pulse}  ·  cooldown {fan_cd}  ·  {pulses}")
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
