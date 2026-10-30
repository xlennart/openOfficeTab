# 真实运行测试 / Runtime testing

## 简体中文

按顺序执行：离线回归 → 候选安装与加载身份 → 三宿主生命周期及实际保存 → 管理中心通道、语言与窗口尺寸 → 修复、保留设置卸载与重装 → 汇总及清理。x64 使用宿主机 Office，x86 使用指定 VMware 客机的交互桌面。传输成功或会话 0 进程不代表桌面验收通过。

管理中心两端使用同一 x64 EXE，完整界面矩阵在宿主机执行；客机重点验证 x86 Office、载荷选择、维护及管理中心入口。仅出现客机特有问题时补做对应界面测试。

管理中心提供独立的真实 WebView2 测试入口：

```powershell
npm ci --prefix tests/webview2
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/center-webview2-runtime.ps1
```

它使用已安装且与候选哈希一致的 EXE，在本机回环端口临时启用 CDP，并隔离 WebView2 配置目录。生产配置不启用调试端口。测试通过实际控件切换正式版/Beta、核对共享配置，验证中英文及自动语言，并把真实原生客户区缩至 420×480，检查控件边界、水平溢出和底栏遮挡。保存截图、阶段与异常；通过后点击窗口关闭并独立等待自有进程退出。Playwright 只负责管理中心网页内容，不能替代 Office COM、真实拖动、原生对话框或进程退出验收。

每轮使用唯一 `jobId`，结果位于 `artifacts/runtime-tests/<jobId>/result.json`。预检绑定 EXE/DLL 哈希、Office 位数、PID/启动时间、交互会话、已解锁桌面、DPI及睡眠策略。唤醒请求由本轮执行器持有，覆盖整个任务并在 `finally` 释放；偏好、语言和自有窗口同样在收尾恢复。失败与复测分别保留，不覆盖先前证据。锁屏、已有未知进程等前置问题记为 `blocked`，驱动和断言失败记为 `failed`。

CDP 是否可用取决于实际 WebView2 运行时；客机 156.0.4314.50 未暴露临时调试端口，本轮改用 `tests/center-native-channel-runtime.ps1`，由 UIA 发现并读取控件，通过限定自有窗口的键盘操作核对持久配置；输入前先确认命中和前台身份。`-LayoutOnly` 只做布局与显示值检查，不能计为输入通过。窗口选择须核对 PID、标题及尺寸，不能把 Tauri 隐藏调整窗口当作主窗口。

客机通过固定交互执行器串行运行测试入口；可临时投放受信任 Node 和锁定的 `playwright-core`，无需安装浏览器、开启 SSH 或更改安全策略。任务结束清理临时运行时与配置副本，保留测试文件和必要证据。重复的接口失败最多尝试两次，再根据已采集能力切换驱动；不盲发全局按键。相同 DLL 哈希的有效 Office 通过证据可以复用，前端变化重点重测最终 EXE 的相关界面和两端维护。

## English

Run the stages in order: offline checks → candidate installation and loaded identity → Word/Excel/PowerPoint lifecycle and actual saves → management-center channels, language and window sizes → repair, settings-preserving uninstall and reinstall → evidence and cleanup. Use real x64 Office on the host and real x86 Office on the VMware interactive desktop. File transfer or a session-0 process does not prove desktop acceptance.

Both machines use the same x64 management-center EXE. Run the full UI matrix on the host; focus guest testing on x86 Office, payload selection, maintenance and the management-center entry point. Add targeted guest UI tests for guest-specific problems.

Use the commands above for the installed management center. The test enables CDP on a temporary loopback port and isolates its WebView2 profile; shipping configuration keeps debugging off. It changes channels through actual controls, checks persisted preferences, tests English/Chinese and automatic language, resizes the native client area to 420×480, and checks boundaries, horizontal overflow and footer interception. Screenshots and failures are retained. Window closure and owned-process exit are separate checks. Playwright covers web content; Office COM, real dragging, native dialogs and process lifetime retain their own tests.

Each run has a unique `jobId` and a `result.json` under the path above. Preflight records EXE/DLL hashes, Office architecture, PID/start time, session, unlocked desktop, DPI and sleep policy. The executor holds a wake request for the whole job and releases it in `finally`, restores preferences and language, and closes only owned windows. Failed runs and retests keep separate evidence. Missing prerequisites are `blocked`; driver/assertion failures are `failed`.

CDP availability depends on the actual WebView2 runtime. Guest runtime 156.0.4314.50 did not expose the temporary debugging endpoint; use `tests/center-native-channel-runtime.ps1` as the native fallback. UIA discovers and reads controls; scoped keyboard input verifies preference changes only after hit-testing and foreground checks. `-LayoutOnly` checks geometry and displayed values and does not count as input acceptance. Select the window by PID, title and size, rather than the hidden Tauri resize window.

Run guest jobs serially in one fixed interactive executor. A trusted temporary Node runtime and the locked test library suffice; no additional browser, SSH service or security-policy changes are required. Clean temporary copies after testing. Stop repeating an unsupported interface after two attempts and choose a driver using the collected evidence. Reuse valid Office evidence only when its DLL hash still matches; frontend changes focus on the final EXE and maintenance on both systems.

Driver basis: [Playwright WebView2 documentation](https://playwright.dev/dotnet/docs/webview2).
