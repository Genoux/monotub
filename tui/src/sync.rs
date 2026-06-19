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

pub fn display_bool(
    reported: Option<bool>,
    desired: Option<bool>,
    on: &str,
    off: &str,
) -> SyncDisplay {
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
