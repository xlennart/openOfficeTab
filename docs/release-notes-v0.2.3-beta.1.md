# openOfficeTab 0.2.3-beta.1

## 简体中文

这是诊断增强测试版，用于验证既有候选修复及收集 issue #5 的现场证据。尚未确认完全解决间歇性选中状态异常，issue 保持打开；正式版仍为 0.2.2。

- 增加操作关联编号、布局任务的请求/延迟/执行/取消记录，以及插件窗口 API 调用前后的状态和错误码。
- 限频采样实际绘制的选中/悬停数量、DPI、窗口状态和各视图状态，帮助区分选中错误、悬停外观与外部激活。
- “关于与更新 → 更新通道”可选择“正式版”或“Beta 版”，默认正式版。设置适用于手动与自动更新；切换不会降级。Beta 按序号比较，同版本号正式版可接替 Beta。
- 修正管理中心版本显示及最小窗口下更新通道被底栏遮挡的问题。
- 保留有界缓冲、轮转、脱敏导出及签名和 SHA-256 校验。日志可增加诊断证据，但无法保证每次故障都有完整根因；崩溃、缓冲丢弃和外部并发动作仍可能造成证据缺口。

若仍出现问题，请按故障模板提交复现步骤、Office 版本与位数、文件规模和导出的脱敏日志，必要时附视频，不要上传敏感原件。32 位 Office 的真实桌面验证在 VMware 虚拟机内进行，Windows 11 LTSC 26100 / Microsoft 365 Apps 16.0.20527.20112 x86；[本版验证记录](https://github.com/xlennart/openOfficeTab/blob/v0.2.3-beta.1/docs/validation-v0.2.3-beta.1.md)列出覆盖边界。

## English

This diagnostic Beta helps evaluate the existing candidate changes and collect evidence for issue #5. The intermittent selection problem is **not confirmed fully resolved**; the issue remains open. Stable remains 0.2.2.

- Adds operation correlation IDs, queued layout request/defer/run/cancel records, and state/error evidence around plugin window API calls.
- Rate-limits samples of actual selected/hovered tab drawing counts, DPI, window state and per-view state.
- Choose **About & updates → Update channel → Stable / Beta**. Stable is the default. The preference applies to manual and automatic checks. Changing channels does not downgrade; Beta numbers compare numerically, and the matching stable version supersedes its Beta.
- Fixes the management-center version label and footer interception of the channel selector at the minimum window size.
- Preserves bounded buffers, rotation, redacted exports, RSA signatures and SHA-256 checks. These records improve evidence; they cannot guarantee a root cause for every incident. Crashes, dropped buffers and concurrent external actions can leave gaps.

If the problem persists, use the bug template and include reproduction steps, Office version and architecture, file size and an exported redacted diagnostic report; add a video when useful. Do not attach sensitive original documents. Real 32-bit Office desktop tests run in VMware; Windows 11 LTSC 26100 / Microsoft 365 Apps 16.0.20527.20112 x86. See the [validation record](https://github.com/xlennart/openOfficeTab/blob/v0.2.3-beta.1/docs/validation-v0.2.3-beta.1.md) for coverage limits.
