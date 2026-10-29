# 32-bit and 64-bit Office: 0.2.2

[简体中文](office-x86.md)

Version 0.2.2 adds 32-bit Office support on **64-bit Windows**. The management center and EXE remain x64; the installer includes native x86 and x64 DLLs and selects the matching Office payload automatically. A 32-bit Windows system and native ARM64 Office are outside this release's target.

Both architectures have real desktop evidence for Word, Excel and PowerPoint. See the [versioned validation record](validation-v0.2.2.md) for actual coverage, payload identities and remaining limits. Offline checks are reported separately.

32-bit Office is tested in VMware on Windows 11 Enterprise LTSC 26100 (64-bit), using 32-bit Microsoft 365 Apps 16.0.20527.20112. 64-bit Office is tested on the host. This environment does not cover every Office build. Report problems through the bug report template with reproduction steps, the Office version/architecture, an exported redacted diagnostic log, and a video when useful.

## Architecture policy

- Inspect existing Word, Excel and PowerPoint executable PE headers using Click-to-Run and MSI/App Paths installation evidence from the appropriate registry views.
- Do not infer Office bitness from Windows bitness or a folder name. Missing, unreadable, unsupported or conflicting evidence prevents installation.
- `Install.cmd` defaults to automatic selection. An explicit `install.ps1 -Architecture x86` or `x64` must still match detected Office.
- Use the matching registration tool and COM registry view. Keep both payloads in the installed center for later repair, including an Office architecture change.
- Preserve shared settings, host enable/disable preferences and opt-in update behavior. Close Office before maintenance; setup never closes documents.
- The existing signed update protocol and x64 EXE filename describe the Windows installer. Older x64 clients can receive the unified installer without a protocol downgrade. RSA verification, installer hash checks and bounded HTTPS routing remain mandatory.

## Build and offline verification

```powershell
./scripts/build.ps1 -Architecture x86
./scripts/build.ps1 -Architecture x64
./scripts/test-native.ps1 -Architecture x86
./scripts/test-native.ps1 -Architecture x64
./tests/native-payload-test.ps1
./scripts/build-desktop.ps1 -SkipNative
powershell.exe -NoProfile -File ./tests/desktop-service-test.ps1
./scripts/package.ps1 -SkipBuild -OutputDirectory ./artifacts/candidate-v0.2.2
./tests/desktop-package-test.ps1 -DeliveryDirectory ./artifacts/candidate-v0.2.2
```

The DLL verifier runs separate x86/x64 processes, loads each DLL, checks all five undecorated COM exports, and exercises class factory and unload lifetimes. It does not register the add-in or start Office. `--verify` retains the original x64 hash output; `--verify-payloads <report.json>` verifies both embedded DLL identities.

Native logs include numeric `bits` for their emitting process; service logs identify the service process width. Exported diagnostics separately report detected Office architecture. These fields must not be confused with proof that a host actually loaded the DLL.

## Runtime validation and future updates

Test 32-bit Office in the VMware desktop and 64-bit Office on the host. The 0.2.2 record covers matching registration and loaded DLL hashes, tab lifecycle, grouping and cross-window drop commands, native cancellation events, layout recovery, language synchronization, redacted logs, update-window OK/X and maintenance round trips. Large Word and Excel fixtures were additionally exercised in the x86 VM.

Command-driven drop tests do not measure manual drag smoothness; cancellation events do not establish every real save-dialog button combination. Office architecture migration, 32-bit Windows, ARM64 and other Office builds are not all runtime-tested.

For future releases, build both DLLs, retain both in the unified installer, and repeat both desktop suites. Shut down the VM normally, then synchronize the same source commit, tag and assets to GitHub and CNB and verify anonymous downloads, hashes and update signatures. Keep reusable stress fixtures; remove temporary copies.
