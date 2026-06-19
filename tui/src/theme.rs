use ratatui::style::{Color, Modifier, Style};

const PRIMARY_HIGH: Color = Color::LightBlue;
const SECONDARY: Color = Color::LightGreen;
const TERTIARY: Color = Color::LightYellow;
const OFF_ACCENT: Color = Color::LightCyan;
const MUTED: Color = Color::White;
const MUTED_DARK: Color = Color::Gray;

pub fn accent() -> Style {
    accent_bright()
}

pub fn accent_bright() -> Style {
    Style::new().fg(PRIMARY_HIGH).add_modifier(Modifier::BOLD)
}

pub fn text() -> Style {
    Style::new()
}

pub fn muted() -> Style {
    Style::new().fg(MUTED_DARK)
}

pub fn label() -> Style {
    Style::new().fg(MUTED)
}

pub fn on() -> Style {
    Style::new().fg(SECONDARY).add_modifier(Modifier::BOLD)
}

pub fn off() -> Style {
    Style::new().fg(OFF_ACCENT).add_modifier(Modifier::BOLD)
}

pub fn warn() -> Style {
    Style::new().fg(TERTIARY).add_modifier(Modifier::BOLD)
}

pub fn error() -> Style {
    Style::new().fg(Color::Red).add_modifier(Modifier::BOLD)
}

pub fn connection(value: Option<&str>) -> Style {
    match value {
        Some("online") => on(),
        Some("offline") => off(),
        _ => muted(),
    }
}

pub fn fan(value: Option<&str>) -> Style {
    match value {
        Some("ON") => accent_bright(),
        Some("OFF") => off(),
        _ => muted(),
    }
}
