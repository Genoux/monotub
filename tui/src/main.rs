mod api;
mod app;
mod commands;
mod sync;
mod tabs;
mod telemetry;
mod theme;
mod time;
mod ui;

use std::time::Duration;

use anyhow::Result;
use clap::Parser;
use ratatui::crossterm::event::{self, Event};
use ratatui::DefaultTerminal;

use crate::api::ApiClient;
use crate::app::App;

#[derive(Parser, Debug)]
#[command(name = "monotub-tui", about = "Terminal UI for monotub fan control")]
struct Cli {
    #[arg(
        long,
        env = "MONOTUB_API_URL",
        default_value = "https://homeserver.tail2e56f0.ts.net:8001"
    )]
    api_url: String,
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    let rt = tokio::runtime::Runtime::new()?;
    let _guard = rt.enter();

    let (api, api_url_tx) = ApiClient::new(&cli.api_url);
    let mut app = App::new(api, api_url_tx);
    app.spawn_poller();

    let mut terminal = ratatui::init();
    let result = run(&mut terminal, &mut app);
    ratatui::restore();
    result
}

fn run(terminal: &mut DefaultTerminal, app: &mut App) -> Result<()> {
    loop {
        app.drain_state_updates();
        terminal.draw(|frame| ui::render(frame, app))?;

        if app.should_quit {
            break;
        }

        if event::poll(Duration::from_millis(100))? {
            let event = event::read()?;
            if matches!(event, Event::Resize(_, _)) {
                continue;
            }
            app.handle_event(event)?;
        }
    }
    Ok(())
}
