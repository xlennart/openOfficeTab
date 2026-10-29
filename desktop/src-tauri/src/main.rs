#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::os::windows::process::CommandExt;
use std::{fs, sync::{Arc, atomic::Ordering}};
use serde_json::{json, Value};
use tauri::{Emitter, Manager};
use tauri_plugin_dialog::DialogExt;
mod service;

// External navigation stays limited to the links visible in the approved UI.
#[tauri::command]
fn open_external(url: String) -> Result<(), String> {
    const LINKS: [&str; 4] = [
        "https://github.com/xlennart",
        "https://github.com/xlennart/openOfficetab",
        "https://github.com/xlennart/openOfficetab/releases",
        "https://github.com/xlennart/openOfficetab/issues",
    ];
    if !LINKS.contains(&url.as_str()) {
        return Err("This destination is not allowed.".into());
    }
    std::process::Command::new("rundll32.exe")
        .arg("url.dll,FileProtocolHandler")
        .arg(url)
        .creation_flags(0x08000000)
        .spawn()
        .map(|_| ())
        .map_err(|e| e.to_string())
}

#[tauri::command]
async fn export_diagnostics(app: tauri::AppHandle, language: Option<String>) -> Result<bool, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let report = service::backend("diagnostics", json!({"language":language}))?;
        if let Some(file) = app.dialog().file().set_file_name("openOfficetab-diagnostics.json").add_filter("JSON", &["json"]).blocking_save_file() {
            let path = file.into_path().map_err(|_| "Invalid export path.".to_string())?;
            fs::write(path, serde_json::to_vec_pretty(&report).map_err(|e| e.to_string())?).map_err(|e| e.to_string())?;
            Ok(true)
        } else { Ok(false) }
    }).await.map_err(|e| e.to_string())?
}
fn page(args: &[String]) -> String {
    if args.iter().any(|arg| arg == "--uninstall") { return "repair".into(); }
    if let Some(index) = args.iter().position(|arg| arg == "--page") { if let Some(value) = args.get(index + 1) { if ["installer","overview","appearance","repair","updates"].contains(&value.as_str()) { return value.clone(); } } }
    if args.first().and_then(|name| std::path::Path::new(name).file_name()).is_some_and(|name| name.to_string_lossy().to_lowercase().contains("setup")) { "installer".into() } else { "overview".into() }
}
fn language(args: &[String]) -> Option<String> {
    args.iter().position(|arg| arg=="--language").and_then(|index|args.get(index+1)).filter(|value|["auto","zh-CN","en"].contains(&value.as_str())).cloned()
}
#[tauri::command]
fn boot() -> Value {
    let args: Vec<String> = std::env::args().collect();
    let host = args.iter().position(|arg| arg=="--host").and_then(|index|args.get(index+1)).filter(|host|["Word","Excel","PowerPoint"].contains(&host.as_str())).map_or("Word",|host|host.as_str());
    json!({"page":page(&args),"host":host,"language":language(&args),"version":env!("CARGO_PKG_VERSION"),"uninstall":args.iter().any(|s|s=="--uninstall")})
}
#[tauri::command]
fn set_update_language(app: tauri::AppHandle, language: String) -> Result<(), String> {
    if !["zh-CN", "en"].contains(&language.as_str()) { return Err("Invalid interface language.".into()); }
    if let Some(window) = app.get_webview_window("update") { window.emit("update-language", &language).map_err(|e| e.to_string())?; }
    Ok(())
}
#[tauri::command]
async fn open_update_window(app: tauri::AppHandle, language: String) -> Result<(), String> {
    if !["zh-CN", "en"].contains(&language.as_str()) { return Err("Invalid interface language.".into()); }
    if let Some(window) = app.get_webview_window("update") {
        window.emit("check-update", &language).map_err(|e| e.to_string())?;
        window.unminimize().map_err(|e| e.to_string())?;
        return window.set_focus().map_err(|e| e.to_string());
    }
    let title = if language == "en" {"openOfficeTab Software update"} else {"openOfficeTab 软件更新"};
    let mut builder = tauri::WebviewWindowBuilder::new(&app, "update", tauri::WebviewUrl::App(format!("update.html?language={language}").into()))
        .title(title).inner_size(560.0, 530.0).min_inner_size(360.0, 400.0)
        .decorations(false).resizable(true).center();
    if let Some(owner) = app.get_webview_window("main") { builder = builder.owner(&owner).map_err(|e| e.to_string())?; }
    builder.build().map(|_| ()).map_err(|e| e.to_string())
}
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if let Some(index) = args.iter().position(|arg| arg == "--verify-payloads") {
        let ok = args.get(index + 1).is_some_and(|path| service::verify_payloads(path).is_ok());
        std::process::exit(if ok {0} else {1});
    }
    if let Some(index) = args.iter().position(|arg| arg == "--verify") {
        let ok = args.get(index + 1).is_some_and(|path| service::verify(path).is_ok());
        std::process::exit(if ok {0} else {1});
    }
    if args.iter().any(|arg| arg == "--silent" || arg == "--background-update") {
        let background = args.iter().any(|arg| arg == "--background-update");
        let result = service::backend(if background {"background-update"} else {"install"}, json!({"preserveHosts":true,"hosts":["Word","Excel","PowerPoint"],"shortcut":false,"language":language(&args)}));
        // Explicit automation output stays in the caller's chosen local file;
        // diagnostic bundles only include the separately sanitized timeline.
        if let Some(index) = args.iter().position(|arg| arg == "--result-file") {
            let report = match &result { Ok(value) => json!({"ok":true,"data":value}), Err(error) => json!({"ok":false,"error":error}) };
            let saved = args.get(index + 1).is_some_and(|path| fs::write(path, report.to_string()).is_ok());
            if !saved { std::process::exit(2); }
        }
        std::process::exit(if result.is_ok() {0} else {1});
    }
    let runtime = Arc::new(service::Runtime::default());
    let closing = runtime.clone();
    tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, args, _| { if let Some(window) = app.get_webview_window("main") { let _ = window.unminimize(); let _ = window.set_focus(); let _ = window.emit("open-page", json!({"page":page(&args),"language":language(&args)})); } }))
        .plugin(tauri_plugin_dialog::init())
        .setup(|app| {
            if let Some(window) = app.get_webview_window("main") {
                if let Some(monitor) = window.current_monitor()? {
                    let scale = monitor.scale_factor();
                    let width = (monitor.size().width as f64 / scale * 0.88).min(920.0);
                    let height = (monitor.size().height as f64 / scale * 0.86).min(700.0);
                    window.set_size(tauri::LogicalSize::new(width, height))?;
                    window.center()?;
                }
            }
            Ok(())
        })
        .manage(runtime)
        .on_window_event(move |_, event| { if let tauri::WindowEvent::CloseRequested { api, .. } = event { if closing.critical.load(Ordering::SeqCst) { api.prevent_close(); } } })
        .invoke_handler(tauri::generate_handler![boot, service::service, export_diagnostics, open_external, open_update_window, set_update_language])
        .run(tauri::generate_context!()).expect("Unable to launch openOfficetab");
}
