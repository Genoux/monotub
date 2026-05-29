use ratatui::style::Style;

use crate::theme;

#[derive(Debug, Clone)]
pub struct SyncDisplay {
    pub text: String,
    pub pending: bool,
}

impl SyncDisplay {
    pub fn style(&self) -> Style {
        if self.pending {
            theme::warn()
        } else {
            theme::accent_bright()
        }
    }
}

pub fn display_bool(reported: Option<bool>, desired: Option<bool>, on: &str, off: &str) -> SyncDisplay {
    match desired.or(reported) {
        Some(value) => SyncDisplay {
            text: if value { on } else { off }.to_string(),
            pending: false,
        },
        None => SyncDisplay {
            text: "unknown".into(),
            pending: false,
        },
    }
}

pub fn display_u32(reported: Option<u32>, desired: Option<u32>) -> SyncDisplay {
    match desired.or(reported) {
        Some(value) => SyncDisplay {
            text: format!("{value}s"),
            pending: false,
        },
        None => SyncDisplay {
            text: "unknown".into(),
            pending: false,
        },
    }
}

pub fn format_schedule(
    reported_on: Option<u32>,
    reported_off: Option<u32>,
    desired_on: Option<u32>,
    desired_off: Option<u32>,
) -> SyncDisplay {
    let on = display_u32(reported_on, desired_on);
    let off = display_u32(reported_off, desired_off);
    SyncDisplay {
        text: format!("{} on / {} off", on.text, off.text),
        pending: on.pending || off.pending,
    }
}
