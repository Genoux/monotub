use tauri::menu::{CheckMenuItem, Menu, MenuItem};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{AppHandle, Manager, WindowEvent};
use tauri_plugin_autostart::ManagerExt;

fn show_window(app: &AppHandle) {
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.show();
        let _ = window.set_focus();
    }
}

// The checkbox always shows the autostart entry's real state, even when changing it fails.
fn toggle_start_at_login(app: &AppHandle, item: &CheckMenuItem<tauri::Wry>) {
    let autostart = app.autolaunch();
    let enabled = autostart.is_enabled().unwrap_or(false);
    let result = if enabled {
        autostart.disable()
    } else {
        autostart.enable()
    };
    if let Err(error) = result {
        eprintln!("monotub: could not change start at login: {error}");
    }
    let _ = item.set_checked(autostart.is_enabled().unwrap_or(false));
}

fn toggle_window(app: &AppHandle) {
    let Some(window) = app.get_webview_window("main") else {
        return;
    };
    if window.is_visible().unwrap_or(false) {
        let _ = window.hide();
    } else {
        show_window(app);
    }
}

fn main() {
    tauri::Builder::default()
        // Registered first so a second launch (login autostart, the app launcher) hands over to the
        // running widget and exits, instead of adding another tray icon.
        .plugin(tauri_plugin_single_instance::init(|app, _args, _cwd| {
            show_window(app)
        }))
        // Writes ~/.config/autostart/monotub.desktop, which the session's XDG autostart runs at login.
        .plugin(tauri_plugin_autostart::Builder::new().build())
        .setup(|app| {
            let toggle = MenuItem::with_id(app, "toggle", "Show / hide", true, None::<&str>)?;
            let start_at_login = CheckMenuItem::with_id(
                app,
                "start-at-login",
                "Start at login",
                true,
                app.autolaunch().is_enabled().unwrap_or(false),
                None::<&str>,
            )?;
            let quit = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
            let menu = Menu::with_items(app, &[&toggle, &start_at_login, &quit])?;
            TrayIconBuilder::new()
                .icon(tauri::include_image!("icons/tray.png"))
                .tooltip("Monotub")
                .menu(&menu)
                .show_menu_on_left_click(false)
                .on_menu_event(move |app, event| match event.id().as_ref() {
                    "toggle" => toggle_window(app),
                    "start-at-login" => toggle_start_at_login(app, &start_at_login),
                    "quit" => app.exit(0),
                    _ => {}
                })
                .on_tray_icon_event(|tray, event| {
                    if let TrayIconEvent::Click {
                        button: MouseButton::Left,
                        button_state: MouseButtonState::Up,
                        ..
                    } = event
                    {
                        toggle_window(tray.app_handle());
                    }
                })
                .build(app)?;
            Ok(())
        })
        .on_window_event(|window, event| {
            // Closing the widget only hides it; the tray's Quit ends the app.
            if let WindowEvent::CloseRequested { api, .. } = event {
                api.prevent_close();
                let _ = window.hide();
            }
        })
        .run(tauri::generate_context!())
        .expect("failed to start monotub desktop");
}
