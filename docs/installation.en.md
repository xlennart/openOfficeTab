# openOfficeTab · Windows x64

[简体中文](installation.md)

Native document tabs for Word, Excel, and PowerPoint. The native add-in supports English and Simplified Chinese. The installer and center share this language setting.

Version 0.2.2 selects native 32-bit or 64-bit Office support automatically on **64-bit Windows**. Both payloads are in the same EXE and ZIP; the center remains x64. See the [architecture and test notes](office-x86.en.md).

## Install

1. Save your work and fully exit Word, Excel, and PowerPoint, including background instances.
2. Run the EXE, select the Office components and click **Install**.
3. For the ZIP package, extract all files and run `Install.cmd`.
4. Reopen an Office document. The tab bar appears below the ribbon.

Installation is per user and needs no administrator rights. The installer does not close your documents or change Office security policy. Windows x64 with 32-bit or 64-bit Office is required. Tests cover host Office 16.0.20527.20088 x64 and VMware Office 16.0.20527.20112 x86; other builds and display setups have not all been verified. The installer is unsigned; compare the download with `SHA256SUMS.txt`.

## Interface language

Open **… → Language**. Choose **Follow Windows (default)**, **Simplified Chinese**, or **English**. Automatic mode uses Chinese for Chinese Windows display languages and English for other languages. The choice persists across restarts and syncs across running Office instances. Menus and ribbon labels refresh without rebuilding document panes. Native Office save prompts and Windows file/color dialogs keep their own application's or system language. File names, paths, and document content are never translated.

In the installer and center, use the language selector in the sidebar or setup content area. It saves the same preference as the native add-in; center windows refresh on focus after a native-language change. Updates preserve existing settings. The Chinese option retains its native name in the picker so users can identify it.

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

Save and fully exit Office before selecting **Diagnostics & repair → Uninstall** in the management center or running `Uninstall.cmd`. Settings are kept by default. To remove them, run `powershell -NoProfile -File .\scripts\uninstall.ps1 -RemoveSettings` from the extracted ZIP.

If tabs are hidden, use **Recover tabs and windows** on the ribbon. If the add-in does not load, open **Diagnostics & repair** in the management center; save and exit Office before **Repair**. Do not disable Office security prompts.

See the repository's validation record for the tested environment and limits.

MIT License · https://github.com/xlennart/openOfficeTab

## Management center and logs

Version 0.2.2 opens a separate window from **Check for updates**. It shows the new version and release notes, or **You're up to date**. Choose **Update now** to proceed. Save your work and exit Office; existing settings are kept. If Office is running, keep this window open to continue after Office exits.

Use the ribbon management-center entry or Start menu shortcut. Export a JSON report from **Diagnostics & repair → Export diagnostics and logs** and attach it to the original issue with the steps and approximate time. Structured logs exclude document names, paths, content, cell values, accounts and tokens by default; nothing is uploaded automatically. Update checks select available routes automatically, verify signed metadata and installer hashes, and preserve settings. Automatic download/install are opt-in; installation waits until Office exits.

## Beta update channel

From 0.2.3-beta.1, choose **About & updates → Update channel → Stable / Beta**. Stable is the default. Manual and automatic checks share this preference. Switching channels never downgrades. Beta numbers compare numerically; a matching stable version supersedes its Beta. Version 0.2.2 and older remain on stable updates; download the prerelease setup to try Beta initially.
