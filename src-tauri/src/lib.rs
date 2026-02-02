use tauri::{Manager, State};
use enigo::{
    Enigo, Settings,
    agent::{Agent, Token},
};
use std::sync::Mutex;
use std::path::PathBuf;
use ron::de::from_str;

const SETTINGS_FILENAME: &str = "user.settings.json";
const PROFILES_FILENAME: &str = "user.profiles.json";

struct AppState {
    enigo_lock: Mutex<Enigo>,
    settings_lock: Mutex<()>,
    profiles_lock: Mutex<()>,
}

fn get_settings_path(app: &tauri::AppHandle, filename: &str) -> PathBuf {
    app.path()
        .app_local_data_dir()
        .expect("failed to get app local data dir")
        .join(filename)
}

fn load_file(app: &tauri::AppHandle, filename: &str) -> String {
    let path = get_settings_path(app, filename);
    if path.exists() {
        std::fs::read_to_string(&path)
            .unwrap_or_else(|_| "{}".to_string())
    } else {
        "{}".to_string()
    }
}

fn save_file(app: &tauri::AppHandle, filename: &str, data: &str) {
    let path = get_settings_path(app, filename);
    std::fs::write(&path, data)
        .expect("failed to write file");
}

#[tauri::command]
fn enigo_execute_token(state: State<AppState>, action: &str) -> Result<(), String> {
    let token: Token = from_str(action)
        .map_err(|e| format!("deserialization error: {}", e))?;
    let mut enigo = state.enigo_lock.lock().unwrap();
    enigo.execute(&token).map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
fn get_settings(app: tauri::AppHandle, state: State<AppState>) -> String {
    let _guard = state.settings_lock.lock().unwrap();
    load_file(&app, SETTINGS_FILENAME)
}

#[tauri::command]
fn set_settings(app: tauri::AppHandle, state: State<AppState>, settings: String) {
    let _guard = state.settings_lock.lock().unwrap();
    save_file(&app, SETTINGS_FILENAME, &settings);
}

#[tauri::command]
fn get_profiles(app: tauri::AppHandle, state: State<AppState>) -> String {
    let _guard = state.profiles_lock.lock().unwrap();
    load_file(&app, PROFILES_FILENAME)
}

#[tauri::command]
fn set_profiles(app: tauri::AppHandle, state: State<AppState>, profiles: String) {
    let _guard = state.profiles_lock.lock().unwrap();
    save_file(&app, PROFILES_FILENAME, &profiles);
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_fs::init())
        .manage(AppState {
            enigo_lock: Mutex::new(Enigo::new(&Settings::default()).unwrap()),
            settings_lock: Mutex::new(()),
            profiles_lock: Mutex::new(()),
        })
        .invoke_handler(tauri::generate_handler![
            enigo_execute_token,
            get_settings,
            set_settings,
            get_profiles,
            set_profiles
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}