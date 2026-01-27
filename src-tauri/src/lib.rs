use tauri::Manager;
// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
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
        .invoke_handler(tauri::generate_handler![greet])
        .setup(|app| {
            let window = app.get_webview_window("main").unwrap();
            setup_window(window);
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
