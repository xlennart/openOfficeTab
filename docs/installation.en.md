# openOfficetab 0.2.0 · Windows x64

[简体中文](installation.md)

Native document tabs for Word, Excel, and PowerPoint. The native add-in supports English and Simplified Chinese. The management center currently uses Simplified Chinese.

## Install

1. Save your work and fully exit Word, Excel, and PowerPoint, including background instances.
2. Run the EXE, select the Office components and click **安装 (Install)**.
3. For the ZIP package, extract all files and run `Install.cmd`.
4. Reopen an Office document. The tab bar appears below the ribbon.

Installation is per user and needs no administrator rights. The installer does not close your documents or change Office security policy. Windows x64 with 64-bit Office is required; other Office versions and display setups have not all been verified. The installer is unsigned; compare the download with `SHA256SUMS.txt`.

## Interface language

Open **… → Language / 语言**. Choose **Follow Windows (default)**, **简体中文**, or **English**. Automatic mode uses Chinese for Chinese Windows display languages and English for other languages. The choice persists across restarts and syncs across running Office instances. Menus and ribbon labels refresh without rebuilding document panes. Native Office save prompts and Windows file/color dialogs keep their own application's or system language. File names, paths, and document content are never translated.

The management center uses Simplified Chinese and does not change the native add-in language. Existing settings are preserved by updates.

## Use

- Click a tab to switch; drag to reorder; middle-click or use `×` to close.
- Drag a tab's right edge to resize it (80–360 DIP).
- Right-click to save, close other tabs, lock, or move to an independent window.
- `+` creates a document; the down arrow lists documents; `…` opens settings.
- Single-taskbar mode is enabled by default for new configurations. It coordinates windows in the current group; independent windows and other groups are excluded.
- Closing from the window title bar offers current / group / cancel with a remember option. Office's own save prompts remain active.
- Recently closed files and per-file tab width / independent-window preferences are stored locally. Unsaved content is not backed up by this add-in.

Settings and logs live under `%LOCALAPPDATA%\openOfficetab`. File history stores encoded, unencrypted paths. Avoid posting those files publicly; use sanitized diagnostics instead.

## Uninstall and recovery

Save and fully exit Office before selecting **诊断与修复 → 卸载** in the management center or running `Uninstall.cmd`. Settings are kept by default. To remove them, run `powershell -NoProfile -File .\scripts\uninstall.ps1 -RemoveSettings` from the extracted ZIP.

If tabs are hidden, use **Recover tabs and windows** on the ribbon. If the add-in does not load, open **诊断与修复** in the management center; save and exit Office before **修复 (Repair)**. Do not disable Office security prompts.

See the repository's validation record for the tested environment and limits.

MIT License · https://github.com/xlennart/openOfficetab

## Management center and logs

Use the ribbon management-center entry or Start menu shortcut. Export a JSON report from **诊断与修复 → 导出诊断与日志** and attach it to the original issue with the steps and approximate time. Structured logs exclude document names, paths, content, cell values, accounts and tokens by default; nothing is uploaded automatically. Update checks select available routes automatically, verify signed metadata and installer hashes, and preserve settings. Automatic download/install are opt-in; installation waits until Office exits.
