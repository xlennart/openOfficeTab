# 混合 Office 标签组 / Mixed Office groups

[简体中文](#简体中文) · [English](#english)

## 简体中文

此功能目前在 `codex/mixed-office-tabs` 开发分支，尚未发布。Word、Excel、PowerPoint 的原生编辑窗口可以加入同一临时标签组。标签栏继续使用各宿主窗口内的现有控件，切换后显示目标 Office 的功能区和文档。

### 试用

1. 使用本分支构建的对应位数插件，分别打开 Word、Excel、PowerPoint 的测试文档副本。
2. 在标签栏设置菜单或普通标签右键菜单选择“混合 Office 标签组（实验）…”，选择另一个文档。第三个文档可以再加入该组。也可将标签拖到另一个 Office 的窗口内标签栏。
3. 点击带 W、X、P 标记的标签，检查对应应用的功能区、文档内容和实际键盘输入。可以拖动标签调整组内顺序。
4. 关闭单个标签时，保存提示由文档所属的 Office 处理；检查取消关闭是否保留内容。混合组的窗口关闭按钮只关闭当前文档，不沿用旧的“关闭本组”记忆。
5. 在标签右键菜单选择“退出混合标签组”，或通过“恢复窗口排列与任务栏”退出并恢复。同组仅剩一个成员时自动解组。手动调整过且已不匹配插件最后应用位置的窗口不会被强行移回旧位置。

### 当前边界

- 使用同一次 Windows 登录、相同权限级别的三种 Office 普通编辑窗口；各端均需此分支插件。保留每个 Office 窗口的任务栏入口。
- 混合组仅在当前运行期间存在，不保存工作区、文件路径或组配置。各应用仍负责自己的文档、COM 对象和保存流程。
- 暂不支持混合组批量保存或批量关闭、跨应用重命名、跨应用公式引用、受保护视图及幻灯片放映窗口。其他文档可新建后手动加入。
- Excel 正在编辑公式或 Office 正在显示模态提示时，暂停混合切换和布局协调。后台窗口在空闲轮询时跟随前台成员位置；连续拖动、跨显示器和 DPI 变化的观感仍需实机验收。
- 单架构编译、隔离通信测试和命令驱动的加入/切换均不能代替双架构 Office 与人工拖动验收。

### 实现约束

现有 Automation 接口保留 DISPID 1–47；48 为 `JoinOfficeGroup(viewId, targetHwnd)`，49 为 `LeaveOfficeGroup(viewId)`。`GetState().mixedTabs` 提供当前混合组的显示顺序与宿主类型。远端标签编号保留在有符号 32 位整数范围内。

使用本项目自有的 Windows 登录及完整性级别隔离共享内存与零等待互斥，不传递 COM 指针、凭据或文档路径。共享数据包含有界标题、成员身份和单个待处理命令，仅授权本次登录与 SYSTEM 访问。成员身份绑定进程启动、会话随机身份、文档视图及窗口寿命标记。

各宿主 STA 消费自己的命令并只调整自己的窗口。真正的前台成员提供组的位置；旧的同应用布局协调不再管理混合成员。通信争用稍后重试；协议错误、事务中断或成员消失时退出混合协调，保留原生窗口和恢复入口。关闭请求异步执行，不将入队成功报告为关闭完成。

外部分析报告只作为机制参考；没有引入 Groupy 二进制、专有源码、全局注入、未公开 DWM 接口或跨进程窗口重挂。报告中的静态推断与未完成交互验证不构成本项目验证证据。

## English

This feature is under development on `codex/mixed-office-tabs` and is not released. Native Word, Excel and PowerPoint editing windows can share a temporary tab group. Tabs remain inside each Office window using the existing control; selecting a tab brings up the target application's real Ribbon and document.

### Try it

1. Build this branch, use the plugin matching Office's architecture, and open disposable document copies in the three applications.
2. Choose **Mixed Office group (experimental)…** from the tab bar settings or a regular tab's context menu, then select another document. Add the third document the same way. Dropping a tab onto another Office window's internal tab bar also joins them.
3. Select the W, X and P tabs. Check the actual Ribbon, document content and keyboard input. Drag tabs to reorder the group.
4. Close one tab and check the owning Office application's save/cancel behavior. The window close button closes only the current mixed-group document; existing close-group preferences do not expand across applications.
5. Choose **Leave mixed group** from a tab's context menu, or **Restore window layout and taskbar** to detach and restore. A group dissolves when only one member remains. Restoration respects subsequent manual placement changes.

### Current limits

- Normal editing windows in the same Windows logon and at the same integrity level; all participants require this branch's plugin. Every Office window retains a taskbar recovery entry.
- Groups are temporary. No workspace, paths or group configuration are persisted. Documents and COM objects remain owned by their original applications.
- No mixed-group bulk save/close, cross-application rename, formula references, Protected View or slide-show support. Newly created documents can be added manually.
- Formula editing and Office modal prompts suspend mixed switching and layout. Idle polling aligns background windows with the foreground member. Continuous dragging, monitor changes and DPI behavior require desktop acceptance.
- Builds, isolated IPC tests and command-driven joins/switches do not establish dual-architecture Office or human drag acceptance.

### Implementation constraints

Automation DISPIDs 1–47 are retained. DISPID 48 is `JoinOfficeGroup(viewId, targetHwnd)` and 49 is `LeaveOfficeGroup(viewId)`. `GetState().mixedTabs` exposes the current mixed group's display order and host types. Remote tab identifiers stay within the signed 32-bit argument contract.

A logon- and integrity-scoped shared-memory ledger uses a zero-wait mutex. It carries bounded titles, lifetime-bound member identities and one pending command per member, with access limited to the logon SID and SYSTEM. It carries no credentials, document paths or COM pointers. Identities include process start, randomized session ownership, view identity and a native window lifetime marker.

Each owning STA executes its own commands and adjusts only its own windows. The actual foreground member supplies the group slot; the original same-application coordinator excludes mixed members. Contention retries later. Invalid protocols, abandoned transactions and vanished members disable mixed coordination while preserving native windows and recovery. Enqueuing a close request is not reported as completed closure.

The external analysis informed architectural choices only. No proprietary Groupy code, binaries, global injection, undocumented DWM APIs or cross-process reparenting were imported. Its static inferences and incomplete interaction tests are not evidence for this implementation.
