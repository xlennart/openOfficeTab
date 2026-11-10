# Contributing to openOfficeTab

[简体中文](CONTRIBUTING.zh-CN.md)

## Open a pull request

1. Fork the repository and start from the current `main`. Keep the PR focused on one change and name any overlapping or dependent PRs.
2. Describe the problem, the change users will see, and the checks you ran. Link the relevant issue when there is one. Keep the PR in draft until the required verification passes and its evidence is attached.
3. Use synthetic Office files for tests and screenshots. Keep personal paths, documents, credentials, raw logs, and local workspace files out of commits and public attachments.

## Provide verification evidence

- For **native code**, run `pwsh -File scripts/test-native.ps1 -Architecture x64` and repeat with `-Architecture x86`. The [Build workflow](.github/workflows/build.yml) builds both Office payloads and checks the unified package.
- For **UI changes**, follow the [interface design guide](design.md#english) and include before/after captures of the affected surface. Update Simplified Chinese and English text, accessible names, and affected documentation. Check Follow Windows, keyboard navigation, and relevant light, dark, and high-contrast states. For window layout changes, include default and minimum sizes and the display scale.
- For **Office behavior**, use disposable fixtures to cover the changed paths and their failure and cancellation behavior. Changes that save, rename, overwrite, or remove files must also cover an existing destination and confirm the actual file contents after Office exits.

## Real runtime verification is required

- The PR author must personally verify the affected behavior in the actual application and provide evidence that it passed before marking a runtime or UI change ready for review. The author is responsible for checking AI-assisted code, tests, and descriptions. AI-generated claims, mock COM tests, off-screen previews, and green CI alone do not satisfy this requirement.
- Native add-in and Office integration changes must cover real 32-bit and 64-bit Office. A VM with an interactive Office desktop is acceptable. Shared behavior must cover Word, Excel, and PowerPoint. Host-specific changes must identify and test the affected hosts.
- Management center, installer, and updater changes must be exercised in the built EXE. For UI changes, cover Simplified Chinese, English, Follow Windows, relevant themes, and default and minimum sizes. When the same EXE serves both Office architectures, run the full UI matrix once and verify affected installation or Office integration paths on both architectures.
- Bind the results to the tested commit SHA, program version, and relevant DLL or EXE SHA-256. Record Windows and Office versions, Office bitness, test steps, expected and actual results, and screenshot, video, or redacted diagnostic links. Evidence must show the resulting state. For save or rename operations, verify the output file and retained content. For close or uninstall operations, verify process exit or the resulting installation state.
- Report each required scenario as passed, failed, blocked, or skipped. Required scenarios must pass before the PR is ready. After a failure, fix the cause and attach a new test result. Changed runtime files invalidate earlier evidence for their affected paths. Reusable evidence must identify identical tested payload hashes.
- If the required environment or evidence is missing, keep the PR in draft and describe the gap. Documentation-only changes require text, link, and translation checks, with the runtime section marked not applicable and a reason.

## Review and merge

- A maintainer reviews the latest diff and CI result before merging. For fork workflows awaiting approval, inspect the submitted code and workflow changes before authorizing the run.
- A maintainer reviews the author's runtime evidence and may repeat tests. Required runtime evidence and passing results are merge requirements. Further maintainer testing supplements the author's verification.
- Coordinate overlapping PRs in the review. Update the later PR against the merged `main` and repeat affected checks on the combined code.

See the [development guide](docs/development.md) and [runtime test workflow](docs/runtime-test-workflow.md) for build and test procedures.
