# 0.2.3-beta.2 validation / 验证记录

2026-10-31–2026-11-01. Candidate fix for issue #5, pending reporter feedback. / #5 候选修复，仍待现场反馈，不认定间歇问题已彻底解决。

## Environments / 环境

| Architecture | Actual environment / 实际环境 |
| --- | --- |
| x64 | Host Windows 11 Enterprise 26H2, build 26300.9711; Word, Excel and PowerPoint 16.0.20527.20088 x64. |
| x86 | **VMware VM**, Windows 11 Enterprise LTSC Evaluation, build 26100.9711; Word, Excel and PowerPoint 16.0.20527.20112 x86, verified from PE headers. |

两端均为真实 Office 桌面；不代表其他 Office 构建。客机注册表 ProductName 保留旧 Windows 10 名称，系统身份使用 CIM caption 与构建号。 / These are real Office desktop tests, not coverage of every Office build. Guest OS identity uses CIM caption/build because its legacy ProductName registry value says Windows 10.

## Defect and evidence / 缺陷与证据

本轮反馈的 beta.1 脱敏日志包含 425 条连续事件、零丢弃。相关布局结束后仍保留源窗口，随后发生原生窗口激活，再提交对应标签选择；日志没有记录触发激活的 OLE 焦点调用，不能确认唯一根因，也不能把 parentTrace=0 等同于外部操作。关闭后重开不是该附件所记录的完整生命周期。

The beta.1 attachment contains 425 contiguous events with no reported drops. Layout completes with the source root retained; later native activation precedes selection commit. The untraced OLE focus path prevents a unique causal conclusion. A zero parent trace does not prove external input, and the attachment does not capture a complete close/reopen sequence.

新增原生回归先激活另一窗口及其输入框，再向后台标签窗格发送 OLE UI 激活。原代码导致活动根和焦点改变并断言失败；修复后保持原活动根及焦点。修复仅允许已经前台且活动、可见启用的窗格取得焦点，并在原生拖动、Excel 编辑期间拒绝转移。

The new regression activates another root/input before a background pane receives OLE UI activation. Original code fails by changing the active root/focus; the fix preserves them. Focus transfer is restricted to an already active foreground, visible/enabled pane and suppressed during native moves and Excel editing. This proves a reproducible defect, not every reporter-side trigger.

## Offline / 离线

- Both architectures passed all eight native regression groups, including the new focus regression; native payload ABI/export checks passed. 两架构各八组原生回归及 ABI/导出检查通过。
- 76 C# and 32 PowerShell service checks plus diagnostic timeline integration passed. Existing channel coverage includes numeric Beta ordering, same-core stable promotion, legacy preference compatibility and no downgrade. 服务、通道及脱敏时间线检查通过。
- Final unified EXE/ZIP checks passed: both embedded DLL hashes, architecture selection, paths, licenses, checksums and private-asset exclusion. 双载荷发行检查通过。
- The final signed metadata passed RSA verification and installer identity checks; a modified signature was rejected. The real service parser rejects Beta in Stable mode and accepts it with Beta enabled. 最终签名、篡改拒绝及正式/Beta 解析策略通过。
- Uninstall-worker waiting and unrelated-process exclusion passed on the separate real-context rerun; the sandbox CIM failure is retained. 卸载等待与无关进程排除复测通过，保留沙箱 CIM 失败记录。
- The reusable VM-runner protocol regression passed negative existence status, no-overwrite transfers, repeated preflight, idempotent cleanup, soft shutdown and rejection of a mismatched fixed executor. This offline stub does not replace VM runtime acceptance. 可复用调度协议测试通过，不代替客机实测。

## Real Office / 真实 Office

- Final Beta2 on both systems, Word/Excel/PowerPoint: installed/loaded DLL identity, tab/content selection, new documents, grouping, command-driven cross-window drop, cancellation events, layout recovery, native close and separate process exit passed. 两端三宿主上述测试通过；命令拖放不代表人工手感。
- Actual native Save passed in all six combinations. Modified isolated files were checked for changed hashes and saved XML markers after Office exited; originals were preserved. 两端三宿主实际保存及退出后内容核对通过。
- Chinese, English and Follow Windows, live preferences and Ribbon callbacks passed on both systems. 双语和自动语言通过。
- Host real-desktop installation upgraded the previous Beta1; repair, settings-preserving uninstall/reinstall and final registration passed. 宿主机真实用户上下文升级及维护通过，未把隔离 HKCU 注册当作安装。
- Final x86 run `beta2-vm-20261031-03` passed all 16 stages: unified install, registration, lifecycle ×3, save, fixture opening, language, fixture exit ×3, center entry/exit, repair, uninstall, reinstall and final registration. Inputs bind final EXE/DLL hashes; every downloaded nonempty evidence file was checked by length/SHA-256. 客机最终 16 阶段通过，证据逐文件核验。
- Guest preflight confirmed an unlocked interactive session and 100% DPI. Job-scoped wake protection was released, owned run directories cleaned, reusable executor/credential preserved, and normal shutdown completed; VMware reported zero running VMs. 客机唤醒、清理及正常关机完成。

## Management center / 管理中心

两端相同 x64 EXE，完整界面矩阵在宿主机完成，客机覆盖 x86 安装和入口集成。 / Both systems use the same x64 EXE. Full UI testing is on the host; the guest covers x86 installation and entry integration.

- Host WebView2 run `499225bbc323444fbc6f6efb5a878897`: final EXE at 200% DPI, Chinese/English × 920×700/420×480, Follow Windows, Stable/Beta persistence, control boundaries and pointer/footer checks passed. Four screenshots were visually inspected. Test-only CDP stayed on loopback, and temporary preferences/profile were restored. 宿主机真实控件及四种语言/尺寸组合通过。
- Host update-window OK/X in both languages, owner preservation and process exit passed. 客机仅计管理中心入口、可见窗口、退出和维护，不重复计全套界面矩阵。 / Host update dialogs passed; guest center acceptance is limited to visible entry, exit and maintenance.

## Retained failures and limits / 保留的失败与边界

改版本号前的 x64 候选首轮在无关的非阻塞持久化断言处失败，独立复测通过。宿主机首次 Word 保存退出超时，随后正常退出，独立完整复测通过。客机试跑 01 在回收 COM 集合时失败，修复显式分支后恢复；试跑 02 全部运行阶段通过但取回清理回执失败，修复路径拼接及幂等回执处理；最终新 jobId 03 完整通过。原失败保留，不改写为通过。

Before versioning, one x64 candidate run failed an unrelated nonblocking persistence assertion; its separate rerun passed. An initial host Word save exceeded the exit deadline, later exited normally, and passed a separate full rerun. Guest trial 01 failed fixture collection cleanup; trial 02 passed runtime stages but failed cleanup-receipt retrieval. Explicit collection handling and receipt/path logic were corrected, and fresh job 03 passed end to end. Original failures remain recorded.

基线三次真实 Excel 标题栏拖动没有复现间歇问题。候选的完整人工拖动矩阵受计算机输入停止信号中断，后续使用 COM/原生消息驱动；不计为人工观感验收。未完全覆盖所有保存对话框按钮、受保护视图、其他 Office 构建及未来正式版安装。未保存取消事件不等同于每个实际按钮。issue #5 保持打开。

Three physical baseline Excel title-bar drags did not reproduce the intermittent symptom. The candidate physical-drag matrix was interrupted by the computer-input stop signal; later tests used COM/native messages. Manual drag smoothness, every save-dialog button, Protected View, other Office builds and a future stable installer are not fully covered. Issue #5 stays open.

## Payload identities / 载荷身份

| Payload | SHA-256 |
| --- | --- |
| Unified setup / center EXE | `6191E117189A4926B30DBBD2A45A08E78093199738FB536E50716DA28B128195` |
| x64 DLL | `563672ED10E3992E1B052F9AD6DFCAE69AB6D1F5DFD35A7E2E0B7271989BB6CC` |
| x86 DLL | `52C5523FBFF1B54210A2FEF61EEA7B3DD2C015613C991C594E2B38C7F6388B47` |
| ZIP | `53A323D74623D13361D8471708A84F4BA2A2BE2A6845409BA67D145559152044` |
| Signed Beta metadata | `5147561D58E8C043AE7D665C6ED2276703DB1DC28ACA39B16559536B01082885` |

固定入口和故障恢复见 [32 位虚拟机流程](office-x86-runtime.md)；分层验收见 [测试流程](runtime-test-workflow.md)。原始过程证据保留本机，不公开用户日志、凭据或私有认知资产。 / See the [reusable VM workflow](office-x86-runtime.md) and [layered test workflow](runtime-test-workflow.md). Raw evidence stays local; user logs, credentials and private cognition assets are excluded from public delivery.
