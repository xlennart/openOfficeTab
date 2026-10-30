# 0.2.3-beta.1 validation / 验证记录

2026-10-30. This is a diagnostic Beta, not a confirmed resolution of issue #5. / 这是诊断增强测试版，尚未确认彻底解决 #5。

## Environments / 环境

| Architecture | Actual environment / 实际环境 |
| --- | --- |
| x64 | Host Windows 11 Enterprise 26H2 26300.9711; Office 16.0.20527.20088 x64; Word, Excel, PowerPoint. |
| x86 | **VMware VM**, Windows 11 Enterprise LTSC evaluation 26100 x64; Microsoft 365 Apps 16.0.20527.20112 x86; Word, Excel, PowerPoint. |

32 位在此虚拟机真实桌面测试，不代表全部 Office 版本。 / The x86 VM tests do not cover every Office build.

## Offline / 离线

- Eight native regression groups per architecture passed, including trace nesting/error propagation, sampling gates, bounded logging and privacy. 两架构各八组原生回归通过。
- 76 C# and 32 Windows PowerShell service checks and diagnostic timeline integration passed. Includes persisted Stable/Beta preferences, legacy requests preserving the channel, invalid-channel rejection, numeric Beta ordering, same-core stable promotion and no downgrade. 服务、通道及脱敏时间线检查通过。
- Real signed Beta metadata checks cover identity, bilingual candidate notes, signature tampering, stable-parser rejection and installed-client channel/version decisions. A future 0.2.3 stable installer is not yet available: comparison-policy checks do not prove installation of that future binary. 真实签名校验与比较策略通过，不等同于安装未来正式版。
- Frontend translation/build, dual embedded payloads, ZIP paths, hashes and private-asset exclusion passed. 前端翻译与双载荷发行检查通过。

## Real Office / 真实 Office

- Both architectures, all three hosts: matching installed/loaded DLLs; tab/content selection, new and independent documents, grouping, command-driven cross-window drops, cancellation events, layout recovery, redacted diagnostics, native close and separate process exit. 两端三宿主完成上述测试；命令拖放不等同于人工鼠标手感验收。
- Actual native Save on modified isolated copies passed in all six host/architecture combinations. Saved XML markers and changed hashes were checked after process exit; originals were preserved. 两端三宿主实际保存、退出后内容核对通过。
- Chinese, English and Follow Windows settings, live synchronization and actual Ribbon callbacks passed in all three hosts on both systems. 双语及自动语言通过。
- Large/small selection tests used Word **30,880,710 bytes** and Excel **53,493,390 bytes**, three rounds per host on both systems, preserving fixture hashes and checking content/exit. This does not establish performance on every large document. 大文件检查不代表全部复杂文档的性能保证。
- Diagnostic exports contain causal IDs, API and layout evidence while excluding synthetic private document markers. Correlation improves investigation; it does not guarantee unique causation or complete crash tails. 日志增加现场证据，不保证每次都能确定唯一根因。

## Management center / 管理中心

Both systems use the same x64 management-center EXE. Full UI acceptance is performed on the host; guest testing focuses on x86 Office and installation/entry-point integration, as agreed with the user. / 两端同一 x64 管理中心，按确认后的范围在宿主机做完整界面回归，客机重点测 x86 Office 与安装、入口集成。

- Final EXE on the host: real WebView2 controls, Chinese/English and automatic language, Stable/Beta persistence, 920×700 and 420×480 client areas, overflow/boundary and pointer-interception checks passed. Host run `2f8297f1a247479c9c7938ce8bc9cd48`, 200% DPI. 宿主机最终界面通过。
- Guest final EXE (100% DPI): English channel input/persistence at both sizes and Chinese input/persistence at the default size passed. Chinese 420×480 layout/display checks passed for both saved channel values (`77ef5966636240b5acc267aac262bb92`, `9637586c8797468f8c7fdb8f93845fdf`); these two runs are **layout-only**, not Chinese minimum-size keyboard acceptance. Each owned center exited normally. 客机中英文通道及最小布局按上述实际范围通过，中文最小窗口的输入驱动拦截保留为未完成输入验收，不冒充通过。
- Both systems: final EXE update window OK/X in Chinese and English, owner preservation and process exit; repair, settings-preserving uninstall, reinstall and registration passed. 两端最终维护及更新弹窗通过。

## Interruptions and boundaries / 中断与边界

The first host Excel drop attempt found both owned windows minimized; the logger did not establish who minimized them. The harness now restores owned roots and verifies they are visible before issuing the drop; the separate rerun passed. The initial failure is retained. 宿主机首次 Excel 失败不计通过，修正前置条件后复测通过。

Guest idle sleep was distinguished from application failure using Windows event 42 (System Idle). Job-scoped wake requests cover testing without rewriting the power plan. Guest WebView2 156.0.4314.50 did not expose the test-only CDP endpoint. The native fallback initially selected Tauri's hidden resize window; failed runs and normal cleanup are recorded separately. 客机睡眠、驱动问题与插件结果分别记录。

Issue #5 remains open. The supplied diagnostic attachment identifies itself as 0.2.0; it does not prove a 0.2.2 regression or a unique root cause. Protected View, every save-dialog button, manual drag smoothness, other Office builds and future stable-installer installation are not fully covered. #5 尚未确认彻底解决；以上未覆盖范围保留。

## Payload identities / 载荷身份

| Payload | SHA-256 |
| --- | --- |
| Center EXE | `77F0A693CF021BFCF13115737821EEC9125D75192F607BCAFA936CDFC83FC1F9` |
| x64 DLL | `5D326F217465208769E089E01D378CBA7C3A9EE7E2C7BC52F602519DCAD17D66` |
| x86 DLL | `F7F1D361990BB98D273CAB78B47C4008DA15D98FC951044B94A1F91A5716C0F3` |

Native evidence remains valid across the final frontend-only fixes because both DLL hashes are unchanged. Failed jobs are not counted as passes. 原生证据按相同 DLL 哈希复用，失败不计通过。

If a problem persists, use the bug template with reproduction steps, Office version/architecture, file size and exported redacted logs; add a video when useful. / 若仍有问题，请按故障模板提交复现、Office 版本与位数、文件规模及导出的脱敏日志，必要时附视频。
