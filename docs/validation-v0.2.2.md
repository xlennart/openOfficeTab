# 0.2.2 validation / 验证记录

2026-10-30. Offline checks and real desktop results are separated below. / 以下区分离线检查与真实桌面结果。

## Environments / 环境

| Architecture | Actual environment / 实际环境 |
| --- | --- |
| x64 | Windows 11 Enterprise 26H2 26300.9711, host; Office 16.0.20527.20088 x64; Word, Excel, PowerPoint. |
| x86 | **VMware VM**; Windows 11 Enterprise LTSC evaluation 26100 x64; Microsoft 365 Apps 16.0.20527.20112 x86; Word, Excel, PowerPoint. |

32 位 Office 在上述虚拟机中测试，不代表全部 Office 版本兼容。问题请按故障模板提供复现、Office 版本与位数、导出的脱敏日志，必要时附视频。 / This VM does not cover every Office build. Follow the bug report template with reproduction steps, Office version/architecture, redacted logs and a video when useful.

## Offline / 离线

- Eight native regression groups per architecture, 16 total; separate x86/x64 export, class-factory/ABI and embedded-payload checks passed. 两架构各八组原生回归及导出、调用和载荷检查通过。
- 55 C# and 29 PowerShell service checks, redacted timeline integration and the owned-worker wait/exclusion regression passed. 服务、脱敏时间线及维护等待回归通过。
- 23 browser/mock-service update scenarios cover availability, notes, errors, download, cancellation and waiting for Office; these are simulated frontend results. 23 项前端模拟结果与下方原生验收分开报告。
- The unchanged 0.2.1 metadata parser accepts the 0.2.2 signed envelope and installer hash. RSA, protocol and legacy asset filenames remain compatible. 原版解析器接受新版签名清单，保留验签、协议与既有附件名。

## Real desktop / 真实桌面

- **Both architectures, all three hosts:** normal file-open startup, matching registration and loaded DLL hashes, tab lifecycle/content selection, new and independent documents, multiple groups, cross-window drop commands, cancellation events, layout recovery, redacted diagnostics and final process exit after the client terminates. 两端三宿主覆盖上述操作，失败不计通过。
- English, Chinese and Follow Windows synchronization, actual Ribbon callbacks and stable panes/documents passed. 三种语言模式及实际功能区回调通过。
- Final EXE identity was checked before Chinese/English native OK/X tests on both machines. Closing the update window preserves its owner; tests then close the owner and wait for process exit. 最终 EXE 两端双语更新窗口与进程退出通过。
- Install, repair, uninstall retaining settings, reinstall and registration passed on both machines. Host upgrade from the signed, published **0.2.1** installer to **0.2.2** preserved settings byte-for-byte; all three hosts then loaded 0.2.2. 两端维护及旧版升级后实际加载通过，设置字节不变。
- Native **Save** on modified isolated copies passed in all three hosts and both architectures. After native close and process exit, file hashes changed and saved Office XML contained the generated marker. Originals were preserved and copies removed. 原生命令实际保存、内容与退出通过。
- x86 stress: Word **30,880,710 bytes**, 250 sections/tables and 40 images; Excel **53,493,390 bytes**, three sheets, 3,000,000 cells and 599,988 formulas. Three large/small selection rounds per host matched active content, preserved fixture hashes and exited normally. Observed open-to-tab times: 7.57 s / 11.84 s, specific to this VM. x86 大文件通过，时间仅为本环境观察，可复用样本保留在宿主机。

## Corrections and interruptions / 修复与中断

Real tests exposed and fixed cross-architecture module inspection, check contention, mixed-path recognition of owned workers and startup caption ordering. A disappearing window is not process exit. Attachment, normal Word file locks, child exit-code handling and an incorrect installer argument were corrected before reruns; failed attempts are excluded.

实测修复跨位数模块漏判、更新检查争用、自有后台服务混合路径识别与标题乱序；测试连接、正常文件锁、退出码及错误静默参数均纠正后重跑，失败不计通过。

The VM previously entered idle sleep; VMware also recorded a separate soft-suspend request whose requester was not established. A later black-screen/GUI stall interrupted Excel. A retained dump does not establish an add-in root cause. After the user-authorized hard reset, a plugin-disabled control and the same final native payload's full three-host rerun passed. A bounded keep-awake helper did not alter the power plan. After testing, the VM shut down normally; five owned tasks and 93,744,124 bytes of temporary data were removed.

客机曾空闲睡眠，另有来源未确认的 VMware 软挂起请求；一次黑屏中断 Excel，转储未证明插件根因。用户授权重置后，停用插件对照和同一原生载荷的三宿主复测通过。未改电源计划，测试后正常关机并清理任务和临时副本。

## Payload identities / 载荷身份

- Center/setup EXE: `D39581E7A4E50E925ABF39C47F3490AE18A6E1BD0071D8FE9CDCC09F54D4CBC4`
- x64 DLL: `B26F6E1082804CEB878619FA6303E8DB9290C75289E380679A594DB052058144`
- x86 DLL: `F56DE56A89E495F6D0BD3FFBED86DDC848D992D5C9EC6993684C9CAC6D37D815`
- ZIP and metadata use the release SHA256SUMS.txt and signed envelope; documentation can change ZIP bytes without changing DLLs. ZIP 与元数据以正式清单为准。

## Limits / 边界

Command-driven drop/selection does not prove manual drag smoothness or exclude all intermittent problems. Cancellation events do not cover every real save-dialog button combination; PowerPoint unsaved cancellation has the narrower event-based scope. New-version download/install UI used mock services; native OK/X used the live up-to-date response. Office architecture migration, other builds, 32-bit Windows and ARM64 were not fully runtime-tested.

命令驱动的拖放/切换不替代人工观感，也不排除全部间歇问题。取消事件不覆盖所有真实保存提示按钮，PowerPoint 未保存取消仅为事件层覆盖。新版本下载/安装界面使用模拟服务，原生确定/关闭使用真实最新版响应。位数迁移、其他版本、32 位 Windows、ARM64 未全量实测，继续通过脱敏日志接收反馈。
