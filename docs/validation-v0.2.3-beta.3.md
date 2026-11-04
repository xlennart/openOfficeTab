# 0.2.3-beta.3 validation / 验证记录

2026-11-04. Diagnostic candidate for issue #5. / 诊断候选，不认定间歇跳转已彻底解决。

## Scope and environment / 范围与环境

本轮按用户要求仅验宿主机真实 **x64 Word、Excel、PowerPoint**，系统 Windows 11 Enterprise、build 26300，三宿主构建均为 **16.0.20527.20088**。Excel 点击观察 DPI 为 192，反馈环境为 182，不能视为等同环境。x86 共享诊断源码并编译为独立载荷；本轮未启动 VMware、未执行 x86 实机或单元测试，不声称其运行影响已经验证。

At the user's request, this round tests real host x64 Word, Excel and PowerPoint only, on Windows 11 Enterprise build 26300, Office 16.0.20527.20088. Excel runs at DPI 192 versus 182 in the reporter's log. The shared diagnostics also compile into x86; no VM, x86 runtime or x86 unit tests were run this round. Runtime impact on x86 remains unverified.

## Evidence and change / 证据与改动

反馈日志中的原生窗口激活先于标签选择提交，现有字段不足以确定最初点击由哪个窗口收到，也不能确认唯一根因。Beta3 保留 Beta2 的后台 OLE 焦点限制，将同线程鼠标/激活观察集成进插件，通过原有管理中心导出；不再要求用户单独运行采集器。`inputId` 仅表示时间关联，CBT 激活请求不等于完成；实际加载路径所指文件哈希不等于进程内存验真。

The reporter's native activation precedes selection commit, but the original recipient and unique cause remain unproven. Beta3 retains Beta2's background OLE focus restriction and integrates same-thread mouse/activation observation into the existing export flow. Input IDs indicate temporal association; a CBT activation request is not completion, and a loaded-path file hash is not memory attestation.

## Verified / 已验证

- x64 八组原生离线回归通过，包括观察器挂载/卸载、窗口消息结果保持、无额外模拟 COM 调用及焦点保持；随后仅同步版本标识，再构建最终双架构 DLL。Both final native builds passed; the eight-group x64 regression used the same functional code before the version-only stamp.
- 最终服务 79 项 C#、32 项 PowerShell 及诊断时间线集成通过；前端中英文词条构建检查通过。Final service checks and bilingual frontend coverage passed.
- 最终 x64 DLL 在三宿主加载并核对哈希：正常文件打开、标签/内容选择、新建、分组、跨窗命令、取消关闭、布局恢复、宽度边界、原生关闭及独立进程退出通过。Final x64 native lifecycle and startup checks passed in all three real hosts.
- 三宿主通过真实原生保存命令，Office 退出后检查文件 XML 含本轮生成标记，并清理保存试件。Real saves persisted generated markers, verified after process exit.
- 三宿主中英文/跟随系统、共享偏好实时同步、实际 Ribbon 回调及窗格/文档身份稳定通过，原语言偏好恢复。English, Chinese and automatic language synchronization passed; the original preference was restored.
- Excel 三个临时工作簿的真实标题栏、最上沿与标签点击，记录 6 条 `window.input`、5 条 `window.activation.request`、34 条处理前后消息及 115 条有界窗口状态，所属 PID 2492；标题命中码 2、上沿命中码 12，实际接收 NetUI 子窗及所属根分别记录。Real Excel input and activation evidence was collected by the built-in observer, without the external helper.
- 最终 EXE 经真实桌面桥接启动，英文 **Diagnostics & repair → Export diagnostics and logs** 原生保存对话框导出成功。JSON 含三宿主准确 PID/启动时间/最终 DLL 哈希及上述点击事件；导出不含测试文档名、路径或内容。中文界面也检查过，但其首次启动环境的注册匹配异常不计真实桌面通过。The final real-desktop English GUI export passed with all three loaded fingerprints and input events; privacy checks passed.
- 本地统一 EXE/ZIP 的双载荷哈希、PE 架构、路径安全、许可证、私有治理资产排除和校验和通过。实际 Beta 签名清单经服务解析器验签，Stable 拒绝、Beta 接受、篡改签名拒绝，安装器哈希匹配。Local package and signed metadata checks passed. Publication and anonymous-download verification are recorded separately after these local checks.

## Failures, recovery and limits / 失败、恢复与边界

语言测试临时收尾脚本先遇到 PowerShell COM 集合枚举错误，随后其哈希比较返回假失败；不算首次收尾通过。重新核对唯一文档、完整路径、PID 与启动时间后正常退出；独立逐字节比较确认三份公开样本与 Git 原件相同，失败记录保留。首次直接从自动化工具启动的管理中心读取了隔离 HKCU/AppData，显示旧注册；该次不计真实桌面状态通过，保留导出。改用现有桌面桥接启动同一 EXE 后，三宿主注册、载荷及实际导出匹配。

The temporary language cleanup first failed on COM collection enumeration, then on its hash comparison; neither was counted as successful cleanup. Scoped normal closure and independent byte-for-byte Git comparison confirmed exit and unchanged fixtures, with failures retained. The first UI launch used an isolated registry/data view; its mismatch was retained and excluded. The same final EXE passed after launch through the existing real-desktop bridge.

命令跨窗拖放不代表真实鼠标拖放手感；本轮标题点击工具会先激活目标，不能完整模拟现场的延迟/后台激活时序。PowerPoint 取消回归含 Saved 标记路径，不代替真实保存提示全部按钮验证。未重跑所有管理中心窗口尺寸、错误提示、完整安装/卸载/旧客户端升级、更新弹窗、其他 Office 构建或大文件长时矩阵。新增诊断没有更改前端交互布局。未稳定复现反馈中的意外跳转，不据此认定已经修复；issue 保持打开。

Command-driven cross-window drop is not mouse-drag feel acceptance. The input driver activates its target first, so it does not reproduce all delayed/background timing. PowerPoint cancellation includes the Saved-flag path, not every native prompt button. Full center size/error/maintenance/upgrade/update-popup, other Office builds and long-running large-file matrices were not rerun. The intermittent jump remains unconfirmed as resolved; the issue stays open.

测试前原有空 Word 经用户授权、重新核对零文档和进程身份后正常退出。所有本轮 Office 和管理中心进程已正常退出，原 Beta2 原生注册与三宿主启用状态恢复，未增加管理中心安装。保存副本已清理；保留可复用合成工作簿和必要本地证据。AC 原策略禁止睡眠；后续保存/语言/导出/收尾任务还核对临时唤醒请求及释放，未修改电源方案。All test-owned processes exited, original Beta2 registration was restored, and no center installation was added. Save copies were removed; synthetic input fixtures and evidence remain. Original AC sleep was disabled; later tasks additionally recorded temporary wake acquisition/release without changing the power plan.

## Payloads / 载荷身份

| File / 文件 | SHA-256 |
| --- | --- |
| x64 DLL | `F809A05548FD702A718B0FA9AAE50A84F94FAD7FE575B6901DA40104EBD41A53` |
| x86 DLL (compile only / 仅编译) | `164F9E5A38DAA830A37994546881CF2E85B2A04D7ED819AAC7F1D4D6F03022A5` |
| Final center / setup EXE | `C8CE0E831FBCC26BF3B649361D1B8EDE576CA1FE291BABF9A9097E2372BD4649` |
| Local ZIP | `01D5FFE0CBD574E91A1DC257B9CF4CB403B60730FD936FB4CEC537E8CD68E687` |

本地证据：`artifacts/beta3-x64-20261104/`，包括按宿主生命周期/保存/语言、点击归属、真实界面导出、包验签及恢复结果。证据可能包含本机路径和合成样本身份，不进入发行包。Local evidence contains machine paths and fixture identities and is excluded from releases. Built files are under `delivery/beta-0.2.3-beta.3/`. Publication, anonymous-download checks and issue communication have separate receipts; local packaging alone does not prove publication.
