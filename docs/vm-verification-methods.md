# 虚拟机组合验证方法 / Combined VM verification

## 简体中文

默认入口仍为 `scripts/test-office-x86-vm.ps1`，客机串行入口仍为 `tests/vm/office-runtime-job.ps1`。各途径承担不同职责，共用唯一 jobId、输入哈希和证据目录。

| 途径 | 合适用途 | 验证边界 |
| --- | --- | --- |
| VMware Tools / vmrun | 启动、Tools/IP/进程检查、固定执行器调度、文件传输、客机截图、正常关机 | Tools 就绪不代表用户已登录；客机截图可帮助区分更新、锁屏与应用状态。 |
| 真实桌面上下文桥接 | 从 Codex 调用实际用户的安装、注册、VMware 管理入口 | 不能仅清除环境变量来假装桌面安装。桥接有短等待期限，长任务应在桥接内启动隐藏进程后立即返回，由 jobId/PID/启动时间跟踪完成。 |
| SSH / SFTP（可选） | 文件传输、日志回收、后台状态与哈希检查 | 不代替已解锁的交互执行器。SSH 启动的会话可能无法看到 Office 窗口，不把退出码 0 当作界面验收。 |
| Office COM / 插件接口 | 文档生命周期、原生保存、分组状态、语言、载荷身份 | 检查真实内容和结果；返回“请求已入队”不代表异步操作完成。 |
| Win32 / UI Automation | 实际窗口、最小化/最大化、菜单、弹窗、控件命中及进程退出 | 消息驱动不证明人工拖动手感；输入前核对所有者、前台、DPI 和窗口矩形。 |
| WebView2 CDP | 管理中心中英文、更新通道、布局、动态提示和持久化 | 仅测试进程临时回环开启，结束关闭；DOM 成功不代替 Office 或原生菜单验收。 |

### 已验证的组合

1. 在真实桌面运行 vmrun 预检，先确认登录解锁、位数、现有应用和唤醒状态。已见过 Tools 已安装但未就绪、用户尚未交互登录，以及隔离上下文启动停留的情况；分别记录，不能合并为“Office 失败”。
2. 固定执行器串行执行安装、Office COM 与窗口消息验证，输入和结果绑定本轮载荷。原生/格式单测在两种架构离线运行；管理中心完整尺寸矩阵在宿主机测试相同 x64 EXE，客机侧重 x86 Office 和入口集成。
3. 通过 vmrun 回收 evidence-index 逐件核对大小和哈希。`host-phase.json` 表示宿主调度步骤，应用进度以当前 `result.json.stages` 为准；不能仅凭未变化的宿主阶段判断卡死。
4. 失败先收状态和截图，同一接口最多两次，然后切换 COM、Win32 或其他已知驱动。恢复只处理本轮副本；无路径的新文档还须精确核对测试标记，先保留内容证据，再关闭。没有该宿主进程时应记录未运行，不尝试创建新 Office 实例用于清理。
5. 窗口关闭后等待 PID 对应的进程退出；恢复设置和唤醒状态，只清理本轮目录并正常关机。保留失败报告，不用复测通过覆盖它。

### SSH 试接与后续接入

专项复测可用 `-Suite mixed-office`；仅当 DLL 与统一 EXE 哈希完全相同时复用此前生命周期证据，保留原失败结果。`-Action Inspect -JobId <当前任务>` 回收该任务的原生窗口/UIA 状态，不执行输入。

本轮遇到的驱动差异：64 位 PowerShell 的普通 `Process.Modules` 未可靠枚举 WOW64 插件，改用共享服务的 `ModulePaths`（包含 32 位模块）并核对实际 DLL 哈希。Word 保存提示的按钮可能位于文档根 UIA 树且没有独立 HWND，须按进程和测试文档所有权选择可调用的取消控件。客机 GDI 截图全黑不证明锁屏，应同时检查解锁状态和 VMware 控制台截图。建立焦点时仅对已核对 PID、根窗口和实际命中的测试标签执行一次原生点击，后续切换独立核对真实前台。

2026-11-17 检查了宿主机 OpenSSH 客户端、现有 SSH 配置和当前 VMware 客机地址；未找到可复用的 SSH Host 配置，客机 TCP 22 探测不可用。本轮没有通过 SSH 执行任务，也未开放服务、修改防火墙或落盘密钥。

后续需要接入时，使用独立受限测试身份、密钥认证和固定主机指纹，限制网络可达范围；账号和私钥保存在受保护本机记录，公开文档只保留参数名称。SSH 只负责上传同一 job 包、触发受限调度或回收日志；交互任务仍由已登录用户的固定执行器串行执行。核对同一证据索引后再比较传输耗时、失败率和恢复成本，不能在未实测前宣称更快。

### MCP 选择与接入 / MCP choice

本轮比较了用户提供的四个项目，选择 [T-R-I-T-E-J/vmware-mcp](https://github.com/T-R-I-T-E-J/vmware-mcp/tree/3bf6d728419648be4d915a08dfc6fa367b1906e8)，固定提交 `3bf6d728419648be4d915a08dfc6fa367b1906e8`。源码包括 Windows 路径白名单、进程超时、密码参数脱敏及交互执行参数。当前 MCP 实际列出 77 个工具，与 README 的旧计数不同；35 项单测通过，真实客机信息和截图调用通过。上游测试脚本的目录参数在当前 Node 下失败，改为显式 `*.test.js` 后通过，未将第一次失败算作成功。

用户已明确授权持久安装和凭据接入。安装位于本机工具目录，独立于软件源码和发行包；Codex 仅启用 12 个日常调度/传输/观察工具。启动器从现有 DPAPI 记录读取凭据，解密后的 JSON 只进入用户开发目录中当前用户/SYSTEM ACL 保护的临时会话文件，退出即删除；实际凭据、MCP 实例配置与依赖不进入公开软件树。持久启动器经 MCP 客户端连接实测通过，退出后临时会话目录数为 0。没有安装 vmrest、配置 VNC 或开放新端口。

这不是对其他项目全部功能的否定：[ZacharyZcR](https://github.com/ZacharyZcR/vmware-mcp) 的 REST/vmrun/vmcli 覆盖面更广；[havu0](https://github.com/havu0/vmware-mcp) 更轻量，但当前源码的超时消息拼接完整 argv，凭据脱敏须额外处理；[slupro](https://github.com/slupro/vmware-mcp) 采用 Python 配置方式。选择依据是本轮 Windows 客机、证据回收和凭据边界，不以工具数量代替实测。

## English

Use `-Suite mixed-office` for a focused retest only when both DLL and unified EXE hashes match the reusable lifecycle evidence. Preserve original failures. `-Action Inspect -JobId <current-job>` collects native/UIA state without input. Use the shared service's `ModulePaths` to enumerate WOW64 modules; ordinary 64-bit `Process.Modules` is insufficient here. Word's save-prompt buttons may live under the document's UIA root with no independent HWND. Bind cancellation to the owned process and fixture. A black guest GDI capture does not establish a locked desktop; cross-check session state and VMware's console capture. Initial native clicks require verified PID, root and hit target; subsequent production switches are independently checked against the real foreground window.

### MCP choice

The selected fixed commit is linked above. It passed 35 unit tests and real MCP guest-info/screenshot calls; the live catalog contains 77 tools. The upstream directory-style test command failed on this Node version, while explicit test-file selection passed. The user authorized a persistent local install with protected credential integration. Codex enables 12 routine tools. A launcher reads the existing DPAPI record, creates a temporary credentials JSON under a current-user/SYSTEM-only ACL, and removes it on exit; no credentials or MCP runtime dependencies enter software source/releases. The persistent launcher passed an MCP-client probe, with zero temporary session directories left afterward. No vmrest, VNC or new listening ports were configured. Other candidates were reviewed for scope, Windows fit and credential handling; broader tool counts alone do not establish suitability. MCP dispatch/observation still uses the project's serial executor and does not replace GUI acceptance.

Keep `scripts/test-office-x86-vm.ps1` as the host entry and `tests/vm/office-runtime-job.ps1` as the serial interactive guest executor. All transports share a unique jobId, input hashes and evidence directory.

| Method | Suitable work | Boundary |
| --- | --- | --- |
| VMware Tools / vmrun | Startup, readiness/IP/process checks, executor dispatch, transfers, screenshots and normal shutdown | Connectivity does not establish an unlocked interactive logon. Screenshots distinguish updates, locks and app state. |
| Desktop context bridge | Actual desktop-user installation/registration and VMware control from Codex | Removing environment variables alone does not establish a real-user install. For long jobs, launch a hidden worker inside the short-lived bridge, then track jobId/PID/start time. |
| Optional SSH / SFTP | Transfers, log collection, background state and hashes | An SSH session may not access the Office desktop. It cannot replace the interactive executor or establish GUI acceptance. |
| Office COM / add-in API | Lifecycle, native saves, group state, language and loaded identities | Verify outcomes/content; queued asynchronous requests are not completed operations. |
| Win32 / UI Automation | Native windows, minimize/maximize, menus/dialogs, hit testing and exit | Check ownership, foreground, bounds and DPI. Native messages do not establish human drag feel. |
| WebView2 CDP | Center languages, channels, layout, dynamic messages and persistence | Enable only temporarily on loopback. DOM results do not prove Office/native behavior. |

The verified workflow uses desktop-user vmrun preflight, a serial guest executor, COM/native tests, and hash-verified evidence collection. Use current guest `result.json.stages` for app progress; a static host dispatch phase alone is not a hang. Collect screenshots/state after failure, limit identical-driver retries, and switch to a known driver. Recover only owned fixtures; an unnamed document requires an exact test marker and preserved content evidence. An absent host must be recorded as absent rather than started for cleanup. Wait for process exit, restore settings/wake state, clean only this job, and shut down normally. Keep original failures separate from retests. The shared x64 center's full UI matrix runs on the host; the VM focuses on real x86 Office and integration.

On 2026-11-17 the host OpenSSH client was present, no reusable SSH Host configuration was found, and the current guest's TCP port 22 was unavailable. SSH was not used for a task; no service, firewall or key changes were made. A future trial should use a restricted test identity, key authentication, pinned host identity and limited network reachability. Keep credentials/private keys in protected local storage. SSH may transfer the same job bundle or collect its indexed evidence; the logged-in user's fixed executor still owns interactive tasks. Measure transfers/recovery before claiming an efficiency improvement.
