use ratatui::layout::{Constraint, Layout};
use ratatui::text::{Line, Span};
use ratatui::widgets::Paragraph;
use ratatui::Frame;

use crate::app::App;
use crate::tabs;
use crate::theme;

pub fn render(frame: &mut Frame, app: &App) {
    let [content_area, footer_area] =
        Layout::vertical([Constraint::Fill(1), Constraint::Length(1)]).areas(frame.area());

    tabs::render(frame, app, content_area);
    render_footer(frame, footer_area, app);
}

fn render_footer(frame: &mut Frame, area: ratatui::layout::Rect, app: &App) {
    if app.show_help {
        frame.render_widget(
            Paragraph::new(Span::styled("any key to close", theme::muted())),
            area,
        );
        return;
    }

    let section_cmds = if app.edit_field.is_some() {
        "Enter save  Esc cancel"
    } else {
        app.focused_section.commands()
    };

    let logs_label = if app.show_logs_strip {
        "l logs*"
    } else {
        "l logs"
    };

    frame.render_widget(
        Paragraph::new(Line::from(vec![
            Span::styled(section_cmds, theme::label()),
            Span::raw("   "),
            Span::styled("Tab", theme::accent()),
            Span::raw(" nav  "),
            Span::styled(logs_label, theme::accent()),
            Span::raw("  "),
            Span::styled("?", theme::accent()),
            Span::raw(" help  "),
            Span::styled("q", theme::accent()),
            Span::raw(" quit"),
        ])),
        area,
    );
}
