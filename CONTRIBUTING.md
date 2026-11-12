# Contributing to openOfficeTab

[简体中文](CONTRIBUTING.zh-CN.md)

## Open a pull request

1. Fork the repository and start from the current `main`. Keep the PR focused on one change and name any overlapping or dependent PRs.
2. Describe the problem, the change users will see, and the checks you actually ran. Link the relevant issue. Use draft for unfinished implementation; describe missing runtime environments and agree on maintainer testing when needed.
3. Use synthetic Office files for tests and screenshots. Keep personal paths, documents, credentials, raw logs, and local workspace files out of commits and public attachments.

## Provide verification evidence

- For **native code**, run `pwsh -File scripts/test-native.ps1 -Architecture x64` and repeat with `-Architecture x86`. The [Build workflow](.github/workflows/build.yml) builds both Office payloads and checks the unified package.
- For **UI changes**, follow the [interface design guide](design.md#english) and include before/after captures of the affected surface. Update Simplified Chinese and English text, accessible names, and affected documentation. Check Follow Windows, keyboard navigation, and relevant light, dark, and high-contrast states. For window layout changes, include default and minimum sizes and the display scale.
- For **Office behavior**, use disposable fixtures to cover the changed paths and their failure and cancellation behavior. Changes that save, rename, overwrite, or remove files must also cover an existing destination and confirm the actual file contents after Office exits.

## Author checks and final runtime acceptance

- Authors should personally execute and inspect at least one relevant check, recording commands or actions, environment, results, and gaps. Prefer exercising the changed behavior in the actual application when available. Authors must check AI-assisted code, tests, and descriptions; generated claims do not count as executed verification.
- Authors do not have to provide the entire two-architecture, three-host runtime matrix. Explain unavailable environments; maintainers may complete runtime acceptance after code review. If no relevant verification is possible, explain the limitation and agree on maintainer review and testing. Missing full runtime evidence alone does not require draft status, and unexecuted checks must never be reported as passed.
- Final acceptance covers real 32-bit and 64-bit Office as affected by the change. Shared behavior covers Word, Excel, and PowerPoint; host-specific changes identify the affected hosts. A VM with an interactive Office desktop is acceptable.
- Final acceptance for the management center, installer, and updater uses the built EXE, covering Simplified Chinese, English, Follow Windows, relevant themes, and default and minimum sizes for UI changes. Run the full UI matrix once for a shared EXE and verify affected installation or Office integration paths on both architectures. The actual tester records the results.
- Bind the results to the tested commit SHA, program version, and relevant DLL or EXE SHA-256. Record Windows and Office versions, Office bitness, test steps, expected and actual results, and screenshot, video, or redacted diagnostic links. Evidence must show the resulting state. For save or rename operations, verify the output file and retained content. For close or uninstall operations, verify process exit or the resulting installation state.
- Report scenarios as passed, failed, blocked, or skipped, distinguishing offline, preview, and actual runtime coverage. Mock COM tests, off-screen previews, and CI do not replace real application acceptance. Preserve failures and record retests separately. Changed runtime files require affected paths to be rechecked; reusable evidence must identify identical tested payload hashes.
- Documentation-only changes require text, link, and translation checks, with the runtime section marked not applicable and a reason.

## Review and merge

- A maintainer reviews the latest diff and CI result before merging. For fork workflows awaiting approval, inspect the submitted code and workflow changes before authorizing the run.
- Maintainers review code alongside author checks and complete agreed runtime acceptance. Ask the author to revise functional problems first; explain maintainer additions and retain contributor credit. Merge and release after code review and required acceptance pass.
- Coordinate overlapping PRs in the review. Update the later PR against the merged `main` and repeat affected checks on the combined code.

See the [development guide](docs/development.md) and [runtime test workflow](docs/runtime-test-workflow.md) for build and test procedures.
