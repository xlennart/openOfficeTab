# openOfficetab 0.1.9 · Windows x64

[简体中文](installation.md)

Native document tabs for Word, Excel, and PowerPoint. This package supports English and Simplified Chinese.

## Install

1. Save your work and fully exit Word, Excel, and PowerPoint, including background instances.
2. Run the EXE and select **Install / Update**. The installer follows the Windows display language by default; its language selector also offers Chinese and English.
3. For the ZIP package, extract all files and run `Install.cmd`.
4. Reopen an Office document. The tab bar appears below the ribbon.

Installation is per user and needs no administrator rights. The installer does not close your documents or change Office security policy. Windows x64 with 64-bit Office is required; other Office versions and display setups have not all been verified. The installer is unsigned; compare the download with `SHA256SUMS.txt`.

## Interface language

Open **… → Language / 语言** or **openOfficetab → Tab settings → Language / 语言**. Choose **Follow Windows (default)**, **简体中文**, or **English**. Automatic mode uses Chinese for Chinese Windows display languages and English for other languages. The choice persists across restarts and syncs across running Office instances. Menus and ribbon labels refresh without rebuilding document panes. Native Office save prompts and Windows file/color dialogs keep their own application's or system language. File names, paths, and document content are never translated.

The installer's selector changes only the installer language; use the add-in setting above to override the add-in language. Existing add-in settings are preserved by updates.

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

Save and fully exit Office before selecting **Uninstall** in the EXE or running `Uninstall.cmd`. Settings are kept by default. To remove them, run `powershell -NoProfile -File .\scripts\uninstall.ps1 -RemoveSettings` from the extracted ZIP.

If tabs are hidden, use **Recover tabs and windows** on the ribbon. If the add-in does not load, use installer **Diagnostics**; save and exit Office before **Repair registration**. Do not disable Office security prompts.

See the repository's validation record for the tested environment and limits.

MIT License · https://github.com/xlennart/openOfficetab
