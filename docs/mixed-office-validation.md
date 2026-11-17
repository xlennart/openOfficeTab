# Mixed Office development validation / 混合标签组开发验证

2026-11-17 · Development branch `codex/mixed-office-tabs`; no release.

## 试用反馈修正 / Review corrections

用户反馈确认首版仅能切换，仍暴露多个窗口，且标签宽度不同步。新增整组合并、共享宽度、按当前成员遮蔽后台窗口与切换任务栏入口；后台选中端失去心跳时恢复其他成员。

- 最终 x64 DLL：`36BE4288E03BF956A4BE0C6B65F557DA401108095FB7DD1BDF3EF615AC60E2FD`。
- 最终 x86 DLL：`502EFBE64D7068BB1C628092DE3869453C13F701F1E496A2B3ADE7623E4132F6`。
- 新实机结果：`artifacts/mixed-office-4bb2c5763fa5454cb4d3866d3ffc2e2b/result.json`。覆盖从两成员组向第三个应用整组合并、三端单标签宽度一致、真实切换时只有选中窗口未遮蔽且只有选中成员保留任务栏入口、解组后全部恢复。
- 双架构原生回归通过；x86 Office 实机、人工拖放、模态保存取消及跨屏 DPI 尚未验收。
- 已在真实桌面安装修正 DLL，沿用已保存的试用副本并保留三个进程。现场状态、加载哈希、后台遮蔽及任务栏入口记录为 `artifacts/mixed-review-7e6d5c86f8a54675a2466d8b12a004eb/fusion-ready.json`；同目录 `fusion-review.png` 是实际前台 Office 窗口截图，显示窗口内三种标签。

The first review exposed multiple windows, inconsistent widths and group splitting. The correction adds whole-group merging, shared widths, reversible background concealment and selection-driven taskbar membership. A stale selected endpoint reveals surviving members. Earlier passing checks below cover the initial implementation only.

The new real-Office run above passed whole-group merging onto a third app, matching per-tab widths, one unconcealed selected member and one retained taskbar entry after each switch, and recovery revealing all members. Both native architectures pass offline regression; x86 Office, human dragging, modal save cancellation and multi-monitor DPI remain unverified. The corrected x64 DLL is installed for user review; the retained fixture processes, loaded hashes, presentation state and actual Office screenshot are recorded in the review directory above. The x64/x86 hashes listed above identify the final payloads.

## 简体中文

- x64、x86 原生构建及原生回归通过。包含隔离的共享内存跨进程 ABI、成员变更、忙碌/锁定保护、窗口身份失效与失败关闭测试。受限开发令牌无法打开仅限登录用户的 IPC，正常用户令牌下测试通过；这不是 Office 实机证据。
- x64 宿主机三种真实 Office 已加载候选 DLL，并逐进程核对 SHA-256。结果为 `artifacts/mixed-office-dc18a84a53da49ac89305299957648be/result.json`，DLL 为 `390E42034607C341AA1D88F3285E3285FE2AAC243DA763F46E2255911189A1FF`。
- 已验证：明确接口加入 Word/Excel/PowerPoint，三端各显示三个成员；原生标签控件边界位于各自 Office 窗口内；三个方向的跨应用切换均检查实际前台 HWND；恢复命令清除组成员。
- 前两轮通过坐标驱动 `DropTab` 未形成预期组，保留失败及第二轮状态快照；调用返回成功不能证明加入了指定混合组。换用明确的目标 HWND 接口后通过，不将其写成人工拖放通过。
- 测试仅打开本轮文档副本，收尾正常关闭试件并恢复原插件注册。当前稳定版没有被本分支替换。
- 未覆盖：x86 虚拟机 Office、人工拖放、可视观感、实际输入与保存内容、保存提示取消、模态编辑、跨屏 DPI、已有客户端升级、正式安装器及发布流程。本记录不代表可直接发布。

## English

- Both native architectures built and passed native regressions, including isolated cross-process ledger ABI, membership, busy/locked guards, stale identities and fail-closed behavior. The restricted development token could not access logon-only IPC; the normal user token passed. These checks are not Office acceptance.
- Real x64 Word, Excel and PowerPoint loaded the candidate, with each loaded DLL hash checked. Evidence: `artifacts/mixed-office-dc18a84a53da49ac89305299957648be/result.json`. DLL SHA-256: `390E42034607C341AA1D88F3285E3285FE2AAC243DA763F46E2255911189A1FF`.
- Passed: explicit three-host joins, three members in each host, native tab-control bounds inside the Office frames, actual foreground HWND switching to each application, and recovery detaching membership.
- Two earlier coordinate-driven `DropTab` runs did not form the intended group. Failures and the second run's snapshot are retained. Success returned by that command does not prove the intended mixed join. The explicit HWND driver passed; this does not establish human drag acceptance.
- Tests used disposable copies, closed their fixtures and restored original registration. The installed stable plugin was not replaced by this branch.
- Not covered: x86 Office in VMware, human drag/visual acceptance, actual editing and saved content, save-prompt cancellation, modal editing, multi-monitor DPI, upgrades, final installer or release validation. This is not release clearance.
