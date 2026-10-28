# Desktop visual review — 2026-10-28

This record covers the Tauri visual review only. It does not validate an installer,
updater, real Office diagnostics or an installed add-in payload.

## Artifact

- Executable: `artifacts/tauri-ui-review/openOfficetab-UI-Review.exe`
- SHA-256: `31E9D993F2A943085DF4F074C42B6C35A5B737805DE2B866578690DB02279EBC`
- Size: 3,255,808 bytes (about 3.1 MiB); the shared system WebView2 runtime is separate.
- Tauri 2.12.5, Rust 1.99.1, x86_64-pc-windows-msvc, release with embedded local assets.
- Frontend bundle: 16,252 bytes. Dependency versions are pinned by npm/Cargo lockfiles.
- The old rejected WinForms source and binaries are retained under ignored
  `artifacts/rejected-winforms-source` and `artifacts/rejected-winforms-0.2.0`.

## Observed in the actual desktop EXE

The review EXE was launched and inspected with Windows Computer Use and its
accessibility tree, serving `http://tauri.localhost/` from embedded assets.

- Installer, overview, appearance, diagnostics and about/update pages rendered;
  their principal controls and footers were visible at the default 1040 × 850 size.
- Sidebar, cards, typography, spacing, blue selection treatment and three-color
  brand icon were visually inspected. This is not a numerical pixel-difference score.
- `src/design.css` and the PNG bytes exactly match the approved design hashes in
  `design-provenance.json`. The outer review toolbar and native frame controls are
  desktop adaptations; the toolbar is a review aid.
- Appearance preview switched from `会议纪要.docx` to `项目计划.docx`; the selected
  visual tab and accessible tab-panel title both changed. This is a mock document,
  **not** evidence for real Word/Excel/PowerPoint activation.
- The update button displayed its demonstration result; no release was fetched.
- Real maximize and restore worked through the Tauri window API, confirmed by the
  native system title-bar actions changing between Maximize and Restore.
- Author links in the installer, sidebar and about footer target
  `https://github.com/xlennart`; repository, release, issue and Star links are present.
  They were inspected without navigating or starring a repository.
- The window was left open on the overview page for user review.

Actual window screenshots are under `artifacts/tauri-ui-review/screenshots/`.
The user still needs to judge whether this implementation meets the visual bar.
Other display scales, machines and dark desktop appearance were not revalidated
in this native EXE. Native minimize/close/drag and external-link invocation were
not exercised in this pass.

## Resource measurement

`process-measurement.json` records one local sample at 13:29:48 +08:00: seven
processes (the application and its WebView2 descendants), totaling 315,191,296
bytes, about 301 MiB, of private commit. This is not resident RAM, a peak, or a
cross-machine guarantee. EXE size alone must not be used as a memory claim.

## Boundaries

No Office installation, COM registration, host settings, update download, scheduled
task, repair or uninstall operation was performed by this review build. Simulated
preferences remain in memory. The existing native DLL candidate is separate and
its earlier Office test results do not apply to this review EXE or prove a new
Office payload installed. No Git commit, push or GitHub Release was made.

The repository remains MIT licensed. Functional integration starts after the
user accepts the actual desktop presentation.
