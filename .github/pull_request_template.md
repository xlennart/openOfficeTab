## 改动 / Changes

<!-- 说明问题、改动范围和关联 issue。Describe the problem, scope, and related issue. -->

## 离线检查与 CI / Offline checks and CI

<!-- 列出命令和结果。List commands and results. -->

## 实机验证 / Real runtime verification

<!-- 说明自己实际执行的验证及未覆盖项。缺少环境时与维护者约定补测，无需独自提供完整矩阵。纯文档说明不适用原因。Describe your actual checks and gaps. Agree on maintainer testing for unavailable environments; authors need not supply the complete matrix alone. Explain non-applicability for documentation-only changes. -->

- 测试提交 SHA、程序版本及 DLL/EXE SHA-256 / Tested commit SHA, version, and DLL/EXE SHA-256
- Windows、Office 版本与位数，语言、主题、窗口尺寸和缩放 / Windows and Office versions and bitness, language, theme, window size, and display scale

| 场景 / Scenario | 操作步骤 / Steps | 预期与实际结果 / Expected and actual results | 通过、失败、阻塞或跳过 / Passed, failed, blocked, or skipped | 证据链接 / Evidence link |
| --- | --- | --- | --- | --- |
| | | | | |

- [ ] 我已亲自执行并核对至少一次相关验证；确实无法执行时已说明限制并与维护者约定补测。/ I personally executed and checked at least one relevant verification, or explained why none is possible and agreed on maintainer testing.
- [ ] 我已核验 AI 辅助改动和描述，区分已执行、未覆盖及失败项，没有把模拟或预览写成实机通过。/ I checked AI-assisted work and separated executed, missing, and failed checks without presenting mocks or previews as runtime acceptance.

## 审阅要点 / Review notes

<!-- 说明风险、失败或取消时的结果，以及依赖或冲突的 PR。Describe risks, failure or cancellation behavior, and dependent or overlapping PRs. -->
