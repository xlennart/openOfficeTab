# openOfficetab desktop management center

Tauri 2 hosts the approved design and a local .NET Framework service. The native Office add-in remains C++. Build with `powershell.exe -NoProfile -File scripts/build-desktop.ps1`; package with `scripts/package.ps1 -SkipBuild`.

The installer registers selected x64 Office hosts for the current user, installs a hash-isolated management center and preserves shared INI preferences. Maintenance refuses running Office processes and never closes documents. The Ribbon and native settings menu open the installed center. Uninstall retains settings unless explicitly selected.

Updates use embedded RSA public-key verification of release metadata and SHA-256 verification of installers. A manual check probes available routes automatically; the interface shows only progress and release results. Downloads retry routes after transport or checksum failure. CNB latest metadata is resolved through its public API. Credentials never ship in the app. Automatic download and install are opt-in; installation waits until Office exits.

`src/design.css` and the original PNG retain approved bytes. `shell.css` adds fixed navigation, independent content scrolling, right-aligned actions and the original 14px base typography. No remote frontend or CDN is needed. MIT and bundled icon notices accompany distribution.

See `VALIDATION.md` for historical visual review and `../docs/validation.md` for functional acceptance and limitations. Build and isolated tests do not prove all desktop or Office flows.

Structured runtime logs and diagnostic export are described in [diagnostic-logging.md](../docs/diagnostic-logging.md).
