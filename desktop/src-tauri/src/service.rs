use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{fs, io::Write, os::windows::{fs::MetadataExt, process::CommandExt}, path::PathBuf, process::{Command, Stdio}, sync::{atomic::{AtomicBool, Ordering}, Arc, Mutex}};
macro_rules! asset { ($path:literal) => { include_bytes!(concat!(env!("CARGO_MANIFEST_DIR"), "/../../", $path)) as &[u8] }; }
const FILES: &[(&str, &[u8])] = &[
    ("dist/x64/openOfficetab.dll", asset!("dist/x64/openOfficetab.dll")),
    ("service/openOfficetab.Service.dll", asset!("build/desktop-service/openOfficetab.Service.dll")),
    ("LICENSE", asset!("LICENSE")),
    ("scripts/install.ps1", asset!("scripts/install.ps1")),
    ("scripts/uninstall.ps1", asset!("scripts/uninstall.ps1")),
    ("scripts/desktop-install.ps1", asset!("scripts/desktop-install.ps1")),
    ("scripts/desktop-service.ps1", asset!("scripts/desktop-service.ps1")),
    ("scripts/diagnose.ps1", asset!("scripts/diagnose.ps1")),
    ("scripts/desktop-context.ps1", asset!("scripts/desktop-context.ps1")),
    ("scripts/localization.ps1", asset!("scripts/localization.ps1")),
];
fn hex(bytes: &[u8]) -> String { bytes.iter().map(|b| format!("{b:02x}")).collect() }
pub fn verify(path: &str) -> std::io::Result<()> { fs::write(path, hex(&Sha256::digest(FILES[0].1))) }
fn payload() -> Result<PathBuf, String> {
    let mut digest = Sha256::new();
    for (name, bytes) in FILES { digest.update(name.as_bytes()); digest.update(bytes); }
    let identity = hex(&digest.finalize());
    let local = PathBuf::from(std::env::var_os("LOCALAPPDATA").ok_or("LocalAppData is unavailable.")?);
    let root = local.join("openOfficetab/cache").join(&identity[..16]);
    for directory in [local.join("openOfficetab"), local.join("openOfficetab/cache"), root.clone()] {
        if let Ok(metadata) = fs::symlink_metadata(&directory) { if metadata.file_attributes() & 0x400 != 0 { return Err("Payload cache contains a reparse point.".into()); } }
        fs::create_dir_all(&directory).map_err(|e| e.to_string())?;
    }
    for (name, bytes) in FILES {
        let target = root.join(name);
        let parent = target.parent().ok_or("Invalid payload path.")?;
        if let Ok(metadata) = fs::symlink_metadata(parent) { if metadata.file_attributes() & 0x400 != 0 { return Err("Payload directory is redirected.".into()); } }
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        if let Ok(metadata) = fs::symlink_metadata(&target) { if metadata.file_attributes() & 0x400 != 0 { return Err("Payload file is redirected.".into()); } }
        if target.exists() {
            if fs::read(&target).map_err(|e| e.to_string())? != *bytes { return Err("Payload integrity check failed; cache was not overwritten.".into()); }
        } else {
            let temp = target.with_extension(format!("{}.tmp", std::process::id()));
            fs::write(&temp, bytes).map_err(|e| e.to_string())?;
            if let Err(error) = fs::rename(&temp, &target) { let _ = fs::remove_file(&temp); if fs::read(&target).ok().as_deref() != Some(*bytes) { return Err(error.to_string()); } }
        }
    }
    Ok(root)
}
pub fn backend(action: &str, mut request: Value) -> Result<Value, String> {
    let root = payload()?;
    request["exePath"] = json!(std::env::current_exe().map_err(|e| e.to_string())?);
    request["callerPid"] = json!(std::process::id());
    let windows = PathBuf::from(std::env::var_os("WINDIR").ok_or("Windows directory is unavailable.")?);
    let mut child = Command::new(windows.join("System32/WindowsPowerShell/v1.0/powershell.exe"))
        .args(["-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File"])
        .arg(root.join("scripts/desktop-service.ps1")).args(["-Action", action])
        // A PowerShell 7 caller can pass Core-only module paths through this
        // executable. Let Windows PowerShell construct its own module search
        // path, so built-in commands such as Get-FileHash remain available.
        .env_remove("PSModulePath")
        .creation_flags(0x08000000).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped())
        .spawn().map_err(|e| e.to_string())?;
    child.stdin.take().ok_or("Service input is unavailable.")?.write_all(request.to_string().as_bytes()).map_err(|e| e.to_string())?;
    let output = child.wait_with_output().map_err(|e| e.to_string())?;
    let response: Value = serde_json::from_slice(&output.stdout).map_err(|_| "本地服务未返回有效结果，请检查安装文件是否完整。".to_string())?;
    if output.status.success() && response["ok"] == true { Ok(response["data"].clone()) } else { Err(response["error"].as_str().unwrap_or("操作未完成。").into()) }
}
pub struct Runtime { serial: Mutex<()>, pub critical: AtomicBool }
impl Default for Runtime { fn default() -> Self { Self { serial: Mutex::new(()), critical: AtomicBool::new(false) } } }
#[tauri::command]
pub async fn service(action: String, mut request: Value, state: tauri::State<'_, Arc<Runtime>>) -> Result<Value, String> {
    if !request.is_object() { return Err("Invalid request.".into()); }
    if !["state","language","appearance","hosts","install","repair","diagnostics","preferences","check-update","probe-routes","download-update","apply-update","uninstall"].contains(&action.as_str()) { return Err("Unsupported action.".into()); }
    request["preserveHosts"] = json!(false);
    let runtime = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let critical = ["language","appearance","hosts","install","repair","preferences","apply-update","uninstall"].contains(&action.as_str());
        let _lock = if critical {Some(runtime.serial.lock().map_err(|_| "Service lock failed.".to_string())?)} else {None};
        if critical {runtime.critical.store(true, Ordering::SeqCst);}
        let result = backend(&action, request);
        if critical {runtime.critical.store(false, Ordering::SeqCst);}
        result
    }).await.map_err(|e| e.to_string())?
}
