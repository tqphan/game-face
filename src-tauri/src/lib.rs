use tauri::Manager;
use enigo::{
    Enigo, Settings,
    agent::{Agent, Token},
};
use std::{
    sync::{LazyLock, Mutex},
};
use ron::de::from_str;

static ENIGO: LazyLock<Mutex<Enigo>> =
    LazyLock::new(|| Mutex::new(Enigo::new(&Settings::default()).unwrap()));

#[tauri::command]
fn enigo_execute_token(action: &str) -> Result<(), String> {
    let token: Token = from_str(&action).map_err(|e| format!("Deserialization error: {}", e))?;
    ENIGO.lock().unwrap().execute(&token).map_err(|e| e.to_string())?;
    Ok(())
}

fn setup_window(window: tauri::WebviewWindow) {
    if let Ok(Some(monitor)) = window.primary_monitor() {
        let size = monitor.size();
        let scale_factor = monitor.scale_factor();
        
        // Calculate 75% of screen size
        let width = (size.width as f64 * 0.75 / scale_factor) as u32;
        let height = (size.height as f64 * 0.75 / scale_factor) as u32;
        
        let _ = window.set_size(tauri::Size::Physical(tauri::PhysicalSize {
            width,
            height,
        }));
        
        let _ = window.center();
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![enigo_execute_token])
        .setup(|app| {
            let window = app.get_webview_window("main").unwrap();
            setup_window(window);
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
