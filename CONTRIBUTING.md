# Contributing to openOfficeTab

[简体中文](CONTRIBUTING.zh-CN.md)

## Open a pull request

1. Fork the repository and start from the current `main`. Keep the PR focused on one change and name any overlapping or dependent PRs.
2. Describe the problem, the change users will see, and the checks you ran. Link the relevant issue when there is one. Use a draft PR while the change is in progress.
3. Use synthetic Office files for tests and screenshots. Keep personal paths, documents, credentials, raw logs, and local workspace files out of commits and public attachments.

## Provide verification evidence

- For **native code**, run `pwsh -File scripts/test-native.ps1 -Architecture x64` and repeat with `-Architecture x86`. The [Build workflow](.github/workflows/build.yml) builds both Office payloads and checks the unified package.
- For **UI changes**, follow the [interface design guide](design.md#english) and include before/after captures of the affected surface. Update Simplified Chinese and English text, accessible names, and affected documentation. Check Follow Windows, keyboard navigation, and relevant light, dark, and high-contrast states. For window layout changes, include default and minimum sizes and the display scale.
- For **Office behavior**, record the Office application, version, and bitness. For changes that save, rename, overwrite, or remove files, cover success, cancellation, failure, and an existing destination using disposable fixtures. Report any checks you could not run. List offline tests, previews, and real Office results separately.

## Review and merge

- A maintainer reviews the latest diff and CI result before merging. For fork workflows awaiting approval, inspect the submitted code and workflow changes before authorizing the run.
- For Office behavior changes, the maintainer verifies the affected paths in real Office on the x64 host and in the x86 VM, covering Word, Excel, and PowerPoint where applicable. For documentation changes, check the text, links, and translations. Record any verification gaps and resolve merge blockers before merging.
- Coordinate overlapping PRs in the review. Update the later PR against the merged `main` and repeat affected checks on the combined code.

See the [development guide](docs/development.md) and [runtime test workflow](docs/runtime-test-workflow.md) for build and test procedures.
