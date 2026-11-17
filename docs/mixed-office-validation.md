# Mixed Office development validation / 混合标签组开发验证

2026-11-17 · Development branch `codex/mixed-office-tabs`; no release.

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
