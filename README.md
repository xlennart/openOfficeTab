<div align="center">

<img src="assets/branding/openofficetab.png" alt="openOfficetab icon" width="128" height="128">

# openOfficetab

**Browser-style document tabs for Word, Excel, and PowerPoint.**

Free · Open source · Native C++ · Windows desktop Office

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Windows x64](https://img.shields.io/badge/platform-Windows%20x64-0078D4.svg)](#compatibility)
[![Build](https://github.com/xlennart/openOfficetab/actions/workflows/build.yml/badge.svg)](https://github.com/xlennart/openOfficetab/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/xlennart/openOfficetab)](https://github.com/xlennart/openOfficetab/releases/latest)

**English** · [简体中文](README.zh-CN.md)

[Download](https://github.com/xlennart/openOfficetab/releases/latest) · [User guide](#user-guide) · [Release history](#release-history) · [Report an issue](https://github.com/xlennart/openOfficetab/issues) · [Development](docs/development.md)

</div>

---

openOfficetab adds a document tab bar below the Office ribbon. Switch documents with a click, drag tabs to reorder them, manage files from the context menu, or move a document into an independent window. It keeps Office's existing documents and save prompts, with no file conversion required.

**Current release: 0.2.1.** Completes English and Chinese setup/management pages, shares language settings with the native add-in, and fixes long labels and smaller-window layouts. Structured runtime logs and the initial tab/document selection improvements from 0.2.0 remain available. See the [validation record](docs/validation.md) for tested behavior and remaining limits.

The add-in supports English and Simplified Chinese through **… → Language**. The installer and management center share the same saved language setting, with a language selector in the sidebar or setup content area and Windows display-language detection by default. The screenshots below show the English interface; [English installation instructions](docs/installation.en.md) are also available. Most supporting technical documents are in Chinese.

Downloads: [GitHub Releases](https://github.com/xlennart/openOfficetab/releases/latest) · [CNB mirror](https://cnb.cool/xlennart/openofficetab/-/releases). Both provide the same release assets and checksums.

## Screenshots

English Office and openOfficetab, with blank example files. Both tab appearances are shown below: **blended open-edge tabs** and a **separate rounded panel**. Expand each comparison to see the alternative.

### Word · Blue accent

Blended open-edge tabs, the add-in ribbon, and the English settings menu:

![Word with blended open-edge tabs and the English settings menu](docs/images/en/word-settings.png)

<details>
<summary>Compare: separate rounded panel in Word</summary>

![Word with a separate rounded panel and a closed rounded selected tab](docs/images/en/word-panel.png)

</details>

### Excel · Green accent

Blended open-edge tabs with a green accent:

![Excel with blended open-edge workbook tabs and a green accent](docs/images/en/excel-blended.png)

<details>
<summary>Compare: separate rounded panel in Excel</summary>

![Excel with a separate rounded panel and a closed rounded selected tab](docs/images/en/excel-panel.png)

</details>

### PowerPoint · Orange-red accent

Blended open-edge tabs with an orange-red accent:

![PowerPoint with blended open-edge presentation tabs and an orange-red accent](docs/images/en/powerpoint-blended.png)

<details>
<summary>Compare: separate rounded panel in PowerPoint</summary>

![PowerPoint with a separate rounded panel and a closed rounded selected tab](docs/images/en/powerpoint-panel.png)

</details>

These user-provided screenshots show version 0.1.7. Choose either appearance under **… → Tab appearance**; the default light blended background is `#F0F0F0`. The [Chinese README](README.zh-CN.md#看看效果) provides matching Chinese-interface screenshots.

## Features

| Feature | What it does |
| :--- | :--- |
| Document tabs | Click to switch, drag to reorder, and see `*` on unsaved documents. |
| New documents and document list | Use `+` to create a document, the down arrow to list documents, and `…` for settings. |
| Quick closing | Close with `×` or the middle mouse button; optional double-click closing. Closing the final document exits the application when no other documents remain. Cancelling keeps the document open. |
| Context menu | Save, Save As, close other / left / right / all tabs, and open the containing folder. |
| Independent windows | Move a document out of its tab group while keeping its original window, file, and unsaved edits. |
| Multiple tab groups | Drag a tab out to create a group, drag it onto another tab bar to merge, or choose a target group from the menu. |
| Resizable tabs | Drag an individual tab's right edge to resize it from 80–360 DIP. Choose fixed, title-based, or compressed default widths. |
| Recently closed | Reopen up to 20 saved files per application; an already-open file is activated instead of duplicated. Clear the list at any time. |
| File preferences | Remember each saved file's manual tab width and independent-window preference across restarts, with reset options. |
| Interface language | English, Simplified Chinese, or automatic selection from the Windows display language; saved across restarts. |
| Settings sync | Sync language, single-taskbar mode, theme, window-close preference, appearance, color, and default width across running Office instances. |
| Diagnostics and recovery | Inspect loading, export sanitized diagnostics, and recover tabs and windows. The installer can inspect and repair registration. |
| Tab locking | Prevent accidental closing from the tab bar. |
| Window coordination | Keep the current size and position when switching within a group, including split-screen layouts. Single-taskbar mode also coordinates moving, resizing, maximizing, minimizing, and restoring. Independent windows and other groups are excluded. |
| Appearance | Blended open-edge tabs or a separate rounded white panel; top, bottom, or no accent line; application-specific accents and Office / Windows theme following. |

Save All and bulk closing apply only to the current tab group. They exclude independent windows and other groups. Closing the final visible tab does not exit Office if other documents, hidden workbooks, or Protected View windows remain.

## Installation

1. Download `openOfficetab-0.2.1-setup-x64.exe` from [Releases](https://github.com/xlennart/openOfficetab/releases/latest).
2. Save your work and fully exit Word, Excel, and PowerPoint, including any old background instances.
3. Run the installer and select the components and click **Install**.
4. Open an Office document again. The tab bar appears below the ribbon.

Installation is per Windows user and does not require administrator privileges. The installer does not close Office windows for you.

Alternatively, download `openOfficetab-0.2.1-win-x64.zip`, extract the entire package, and run `Install.cmd`. The ZIP and EXE install the same add-in.

Releases include `SHA256SUMS.txt`. The installer is not code-signed; you can verify its checksum or build from source.

```powershell
Get-FileHash .\openOfficetab-0.2.1-setup-x64.exe -Algorithm SHA256
```

### Uninstall

Save your work and exit all three Office applications. Open the management center and select **Diagnostics & repair → Uninstall**, or run `Uninstall.cmd` from the extracted ZIP. Settings and logs are kept by default.

To remove settings as well, run this from the extracted package directory:

```powershell
powershell -NoProfile -File .\scripts\uninstall.ps1 -RemoveSettings
```

## Management center and diagnostics

Open **openOfficetab → Settings and recovery → Management center** from the ribbon, or use the Start menu shortcut. The center provides component status, appearance, diagnostics/repair and updates. The center includes full English and Chinese interface text.

A manual update check probes available routes automatically. Signed metadata and installer SHA-256 are verified; failed downloads can try another available route. Automatic downloads/installation are opt-in, and installation waits until Office exits.

If a problem recurs, choose **Diagnostics & repair → Export diagnostics and logs** to export a JSON report, then attach it to the original issue with the steps and approximate time. The bounded timeline records operations, Office events, process-local window/tab identities and error codes. Document names, paths, content, cell values, accounts and tokens are excluded by default; nothing is uploaded automatically. See [diagnostic logging](docs/diagnostic-logging.md). Related issues remain open for follow-up.

## User guide

The instructions below use the English interface labels.

### Choose the interface language

Open **… → Language**. Select **Follow Windows (default)**, **Simplified Chinese**, or **English**. Automatic mode uses Chinese for Chinese Windows display languages and English for other languages. Your choice survives restarts and syncs across running Office instances. Ribbon labels, menus, and subsequent add-in dialogs update without recreating document panes. File names and contents stay unchanged; Office save prompts and native Windows dialogs retain their own language. In the installer and center, the language selector in the sidebar or setup content area uses this same saved setting. Center windows refresh on focus after a native-language change. Static labels, runtime messages and sample content are translated; user file names, paths and content retain their original text.

### Use an independent window

Right-click a tab and select **Show in an independent window**. The window hides its tab bar, stops following the group, and regains its own taskbar entry. The document is neither closed nor reopened.

To rejoin, select **Join / move to tab group** on the **openOfficetab** ribbon and choose a group. The ribbon and tab menu also offer **New independent document**.

Saved local files remember this choice across restarts. Unsaved documents keep it in memory until their first save. If a file has multiple editing views, the selected view is detached; the last explicit choice becomes the default when that file is reopened. Group membership itself lasts only for the current session.

### Split and merge tab groups

Drag a tab outside the tab bar to create a new group window with its own tab bar. Drag it onto another window's tab bar to join that group. The context menu also provides **Move to a new tab group window** and **Move to tab group…**.

Groups operate within one instance of one Office application. Cross-process moves and persistent group membership are not supported. An independent window has no tab bar and must rejoin through the ribbon.

### Resize individual tabs

Move the pointer to a tab's right edge and drag when the horizontal resize cursor appears. This changes only that tab's width without switching, closing, or moving it. Press Esc to cancel.

Manual widths are saved locally for saved files and restored when reopened. An unsaved document inherits its current width on first save, and Save As carries preferences to the new path. **Default tab width…** affects tabs without a manual override.

Use **Reset this file's tab settings** to clear its width and independent-window preference. **Clear remembered file settings for this application** clears all file preferences for the current application and resets current tab widths. Existing independent windows stay in place; reopening uses the default grouping behavior.

### Reopen recently closed files

Open **Recently closed…** from the ribbon, a tab's context menu, or `…`. Each application remembers up to 20 saved local files, including accessible UNC paths. An already-open file is activated; moved or deleted files remain listed with an error when opening fails.

Cancelled closes, hidden documents that remain open, and other views of an open file are not treated as closed files. Unsaved documents require Office's own document recovery. **Clear recently closed files** clears the list. This history stores paths, not document content or unsaved edits, and is not a backup.

### Switch tabs in split-screen layouts

When you place an Office window on one side of the screen, switching from its tab bar transfers that window's current size and position to the target document. In single-taskbar mode, moving, resizing, maximizing, minimizing, and restoring also follow within the group. Independent windows and other groups keep their own layouts.

Office retains its native document windows. During moving and resizing, the add-in temporarily masks background windows in the same group, then aligns them before restoring their appearance. Updates are coalesced outside Office's layout callbacks. **Restore window arrangement** restores layouts changed by the add-in while preserving subsequent manual layout changes.

Single-taskbar mode is enabled by default for new configurations, persists across restarts, and syncs across running instances; an explicit choice to disable it is retained. Each group keeps the active document's taskbar entry, while independent windows keep their own entries. Hiding the tab bar restores other entries. Recovery does not disable this setting. Coordination is limited to the current process; it does not merge Office processes or change the Windows Alt+Tab list.

### Close from the window title bar

With single-taskbar mode enabled and multiple tabs open, the window's `×` or Alt+F4 asks whether to **Close current**, **Close group**, or **Cancel**. You can select **Remember my choice**, and reset it to asking each time under **Tab settings → When clicking the window close button**.

Group closing excludes independent windows and other groups. Office's own save prompts remain active; cancelling a save prompt stops the remaining closes. Closing the final document exits the application only when no other documents or Protected View windows remain. A tab's own `×` closes only that tab.

### Customize appearance

Open **… → Tab appearance**:

- **Blend with application (open-edge tabs)**: a light `#F0F0F0` background with no separate white panel or panel shadow. The outer frame retains rounded corners and 8 DIP side insets aligned with the ribbon. The selected tab opens toward the document area, including when docked at the bottom. A background overlay softens the native ribbon seam only in recognized layouts.
- **Separate tab bar (rounded panel)**: a rounded white panel with a closed gray selected tab.
- **Accent line**: top, bottom, or none. Word uses blue, Excel green, and PowerPoint orange-red.

The default is the blended style with a top accent. Theme selection follows Office by default and follows Windows when Office uses the system setting. **Theme** also offers always light, always dark, or Windows-only following. Black Office themes use `#262626` for the blended bar and top seam; Dark Gray uses a matching darker palette. Dark blended layouts align with the dark ribbon and retain the bottom separator.

Choose **Tab bar color…** to enter `#RRGGBB` or select a palette color. Custom colors also work with the separate panel. **Restore default color** restores `#F0F0F0` for blended mode or white for the separate panel. Dark mode and Windows high contrast take precedence over custom colors. Cancelling leaves settings unchanged.

Appearance changes redraw the add-in controls without rebuilding Office panes. Other running instances normally receive changes within a second, or later if Office is busy or showing a modal dialog.

### Set a default width

Use **… → Default tab width…** or **Tab width** on the ribbon to set 80–360 DIP. Widths scale with the display; title-based and compressed modes use the value as their upper limit. Cancelling leaves settings unchanged.

### Diagnose and recover

The ribbon offers **Loading diagnostics**, **Export sanitized diagnostics**, and **Recover tabs and windows**. Recovery rebuilds the add-in's own bar, shows tabs, and disables single-tab auto-hide while preserving documents, edits, group membership, order, and the single-taskbar preference.

If the add-in does not load at all, open **Diagnostics & repair** in the management center. Save and exit Office before selecting **Repair**. Sanitized diagnostics exclude file names, paths, document content, accounts, and raw logs. Repair does not change Office security policy.

Settings are available from `…` or **Tab settings** on the ribbon. Tab locking only guards against closing from the tab bar; an explicit title-bar group close still goes through Office's normal save flow.

## Compatibility

### Development environment

The current development machine uses the following environment (checked on October 18, 2026):

| Component | Version |
| :--- | :--- |
| Operating system | Windows 11 Enterprise 26H2, 64-bit; OS build **26300.9711**. |
| Office | Microsoft Office Home 2024, Click-to-Run, **64-bit**; version **16.0.20430.20182**. |
| Host applications | Word, Excel, and PowerPoint from the Office installation above. |

These versions describe the development environment, rather than minimum requirements or a guarantee of compatibility with every Office version. The earlier runtime test record below used Office **16.0.20430.20146**; the development installation has since been updated.

### Compatibility and validation scope

| Item | Current status |
| :--- | :--- |
| Earlier runtime-tested environment | Windows 11; Office Home 2024 x64; Word / Excel / PowerPoint 16.0.20430.20146. |
| Tested display scale | 200%. This Office installation imposes a minimum horizontal pane height of 80 physical pixels, approximately 40 DIP. |
| Release architecture | x64. Match **Office's bitness**, not just Windows bitness. |
| x86 / other Office versions | The build script has an x86 option, but runtime behavior is unverified and no x86 installer is provided. |
| Office for the web / macOS | Unsupported. This is a native Windows COM add-in. |

This is an early usable release, with no claim of feature parity with mature commercial tab add-ins. Configurable global shortcuts and complete accessibility support are not yet available. See the [validation record](docs/validation.md) for methods and limits.

## Frequently asked questions

**Why is there no tab bar after double-clicking a file?**

Save and exit Office, reinstall, and reopen the document. Check whether `openOfficetab` is loaded in Office's COM Add-ins list, then check tab visibility and single-tab auto-hide. Safe Mode, Protected View, or organizational policy can restrict add-ins; check in a normal editing window without changing Office security settings.

When installation runs from the Codex development environment, the script delegates registration to a separate desktop process to avoid isolated registry and AppData views.

**Where are settings and logs stored?**

Under `%LOCALAPPDATA%\openOfficetab`. `settings.ini` stores global settings; `documents.ini` stores per-application file preferences and recently closed paths. Paths are encoded, not encrypted. These files stay local and are not uploaded; sanitized diagnostics exclude path history. Avoid posting private file names and paths in issue reports.

**Why did another window not pick up my settings?**

Language, appearance, color, default width, single-taskbar mode, theme, and title-bar closing preferences normally sync within a second. Busy applications and native dialogs can delay updates. Per-file widths override the default. Visibility and docking position are not live-synced. A new DLL still requires saving and restarting Office.

## Release history

| Version | Date | Highlights |
| :--- | :--- | :--- |
| [0.2.1](https://github.com/xlennart/openOfficetab/releases/tag/v0.2.1) | 2026-10-29 | English and Chinese setup/management center, shared language settings, responsive layout fixes and English documentation labels. |
| [0.2.0](https://github.com/xlennart/openOfficetab/releases/tag/v0.2.0) | 2026-10-28 | Management center, sanitized operation timelines, automatic update route selection and initial tab/document alignment improvements. |
| [0.1.9](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.9) | 2026-10-23 | Preserve Excel formula entry when switching tabs to select cells from other workbooks; defer window coordination until editing ends. |
| [0.1.8](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.8) | 2026-10-19 | Queue native new-document commands; defer pane creation and window coordination until ribbon content is ready; strengthen regression checks for actual ribbon content. |
| [0.1.7](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.7) | 2026-10-18 | English and Chinese UI; saved language selection with Windows display-language detection; live Ribbon translation; bilingual installer and package instructions. |
| [0.1.6](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.6) | 2026-10-17 | Persist single-taskbar mode and enable it by default for new configurations; Office / Windows theme following; coordinated window operations and close choices; fixes for resizing and PowerPoint activation. |
| [0.1.5](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.5) | 2026-10-15 | Recently closed files, remembered per-file widths and independent windows, live appearance sync, and ribbon seam refinements. |
| [0.1.4](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.4) | 2026-10-13 | Split-screen tab switching across all three applications, ribbon ownership fixes, and custom tab-bar colors with a `#F0F0F0` default. |
| [0.1.3](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.3) | 2026-10-11 | Blended open-edge tabs, three accent-line positions, and individual tab-edge resizing. |
| [0.1.2](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.2) | 2026-10-11 | Exit after the final document closes, independent-window workflows, multiple groups, width policies, diagnostics, and recovery. |
| [0.1.1](https://github.com/xlennart/openOfficetab/releases/tag/v0.1.1) | 2026-10-10 | First public release: native tabs for Word, Excel, and PowerPoint, document management, and x64 EXE / ZIP packages. |

Dates follow the project's release notes in China Standard Time (UTC+8). For detailed changes, see [CHANGELOG.md](CHANGELOG.md) (Chinese); downloads and release-specific notes are on [Releases](https://github.com/xlennart/openOfficetab/releases).

## Development and contributing

Install Visual Studio Build Tools with C++ tools and the Windows SDK, then run:

```powershell
pwsh -File .\scripts\build.ps1 -Architecture x64
pwsh -File .\scripts\test-native.ps1
pwsh -File .\scripts\package.ps1
```

The build produces `dist/x64/openOfficetab.dll`; packaging produces an EXE, a ZIP, and checksums. The desktop build uses Node.js, Rust/MSVC and Windows .NET Framework. Its Tauri/WebView2 management center runs separately; the Office tab bar remains native C++. Packaging also emits signed update metadata. GitHub Actions builds the add-in and verifies tests and installer payloads; actual Office runtime tests require a local desktop Office installation.

See the [development guide](docs/development.md) for source, testing, and release workflows, and the [feature comparison](docs/comparison.md) for differences from Office Tab and possible next steps. Reproducible bug reports and suggestions are welcome through [Issues](https://github.com/xlennart/openOfficetab/issues). Include Windows and Office versions, Office bitness, and display scale.

## License

[MIT](LICENSE). openOfficetab is an independent implementation and includes no source code or binaries from the reference product. It is not affiliated with Microsoft or ExtendOffice. Office and other product names belong to their respective owners.
