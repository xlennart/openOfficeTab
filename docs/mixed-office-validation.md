# Mixed Office development validation / 混合标签组开发验证

2026-11-17 · Development branch `codex/mixed-office-tabs`; no release.

## 本轮界面与窗口行为 / Current UI and window behavior

本轮使用以下开发载荷，不能将后面的历史哈希或结果改绑到它们：

| Payload | SHA-256 |
| --- | --- |
| x64 DLL | `1D14AD1DF29F7DB9D8D0D5FEDE5E23912EF345C81A3B16660BECD276BC5B818D` |
| x86 DLL | `E9219380C775583FFD2A4B18A873A9BED1935EBE8C3A38463ED78F115FC53A59` |
| x64 management center | `D31BCFBFB236A13A2A37DD4764098CC3065BB29FDEE8312876560D47D5225281` |

- 双架构原生回归通过；标签采用本机 Office 图标、统一底座与对齐，菜单改为抗锯齿线条。原生明暗菜单预览通过，但预览不代替 Office 实机验收。
- 最终 x64 DLL 的三应用实机结果：`artifacts/mixed-office-67e1380bc33b4a18b192c34debbc9b59/result.json`。通过三成员整组合并、窗口内控件边界、共享宽度、真实前台切换及单窗口/任务栏呈现；整组最小化/还原、最大化/还原和原生移动通过；从真实窗口关闭当前文档后，其余两成员保留，恢复命令解组并揭示窗口。
- 最终管理中心真实 WebView2 检查：`artifacts/runtime-tests/a314ddcdcce443569b3c9183974770bf/result.json`。中英文、920×700 及 420×480、语言自动跟随、通道持久化及新分组入口的无 Office 提示通过；测试恢复偏好，自有窗口和进程退出。此前最小窗口被固定底栏遮挡的失败保留，已改为正文滚动、底栏独立后复测通过。
- PR #6 精确 head `1b9ddd95c9519c3c49099fb7132fe0c09ef74e79` 已保留贡献历史合入主分支，merge 为 `88720d5a55754e8198619f4dda8597e6a71c39a8`。本分支原先的本地合并为 `f1e7028`。最终 DLL 的普通及 Strict PPTX 原生重命名弹窗验证通过：`artifacts/ppt-live-rename-a4d03371ba7e4c2aa7b7daaec16536a3/result.json`。保持原对象与 HWND，旧路径清理，正常退出后检查新文件内容和主部件格式命名空间。
- 本轮 x86 Office 实机预检因 VMware Tools 未就绪而阻塞；没有重置客机，已通过固定入口请求正常关机。x86 编译和离线通过不算实机通过。
- 未覆盖：人工跨应用拖放手感、所有保存取消/模态组合、网络共享、跨屏 DPI、受保护视图和完整发行/升级矩阵。右键菜单的自动化定位失败未计通过；重命名实测通过与 Ribbon 相同的生产命令打开原生弹窗。

The hashes above identify this round's development payloads; historical evidence below retains its original identity. Both native architectures passed offline regressions. Mixed tabs use locally installed Office icons in aligned badges, and menu strokes use antialiasing; off-screen previews are not Office acceptance.

The final x64 three-host run passed whole-group merging, internal control bounds, shared widths, real foreground switching, a single presented window/taskbar entry, group minimize/restore, maximize/restore, native movement, closing the current document while retaining two members, and recovery. The final center passed real WebView2 checks in English/Chinese at default and minimum sizes, automatic language selection, persistent channels, and the no-Office branch of its new group entry. The earlier minimum-size overlap failure remains recorded; scrolling content and a separate footer resolved it. Preferences were restored and owned processes exited.

PR #6 head `1b9ddd9` was merged with contributor history preserved. The final-DLL native dialog run above passed ordinary/Strict PPTX renaming with the same object/window, old-path cleanup, normal PowerPoint process exit, and saved content/namespace checks. This used the production Ribbon-equivalent command; failed right-click automation is not counted as passed.

VMware Tools readiness blocked this round's x86 Office preflight; no reset was used, and the fixed runner requested normal shutdown. Human dragging, all modal/save-cancel combinations, network shares, multi-monitor DPI, Protected View, and full release/upgrade acceptance remain uncovered. Nothing in this record authorizes or establishes a new release.

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
