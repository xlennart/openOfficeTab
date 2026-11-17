# 0.3.0-beta.1 验证记录 / Validation record

2026-11-17。混合 Office 标签组公开测试版；正式通道保持 0.2.3。 / Public mixed-Office beta; stable remains 0.2.3.

## 发布核验 / Publication verification

发行源码提交：`33ec7e5538473b0072da5f3365c43e3a8295f5f6`，标签 `v0.3.0-beta.1`；已合入并推送两端 main，PR #6 最终 head 已合并。2026-11-17 核验 [GitHub](https://github.com/xlennart/openOfficeTab/releases/tag/v0.3.0-beta.1) 与 [CNB](https://cnb.cool/xlennart/openofficetab/-/releases/tag/v0.3.0-beta.1) 均公开且为 prerelease，稳定 latest 保持 v0.2.3。两端各四个附件匿名完整下载，与本地逐字节身份一致，Beta RSA 签名通过。CNB 元数据 API 匿名返回 401，发布状态通过授权 API 核对；稳定 latest 另通过公开页面重定向核对，不将认证查询写成匿名成功。

[OSS 版本目录](https://openofficetab-xlennart.oss-cn-chengdu.aliyuncs.com/releases/v0.3.0-beta.1/SHA256SUMS.txt) 的四个附件同样经匿名下载、大小、SHA-256 与签名核验；OSS 稳定签名指针仍为 0.2.3。官网默认稳定版不变；本版截图只用于 Beta 展示。凭据、MCP 运行依赖、本机规则及认知均不在公开源码与发行包中。

Release source commit: `33ec7e5538473b0072da5f3365c43e3a8295f5f6`, tag `v0.3.0-beta.1`, merged and pushed to both main branches. GitHub/CNB releases are public prereleases; stable latest remains v0.2.3. All four assets on each platform were downloaded anonymously and matched local size/SHA-256; Beta RSA signatures passed. CNB's metadata API returned anonymous 401, so release status used authenticated API checks while stable latest was independently checked by public-page redirection. All four OSS versioned objects also passed anonymous size/hash/signature checks; its stable signed pointer remains 0.2.3. The stable website is unchanged. Private credentials, MCP dependencies and governance assets are excluded.

## 载荷 / Payload identity

| Payload | SHA-256 |
| --- | --- |
| Unified EXE / Center | `6911B44A80EA7E0705C3CB79C74DB90E3128E5D91E05C673148579C581A8BC2A` |
| ZIP | `64427D8A3D8DE999CB63C1E5AD953C1735937758DA4A92C53AF687A8E98E3184` |
| x64 DLL | `FFBC4607E0AF0525F261F19F15944962C3ED71A6FCA4FE04FE591FA5A46A278A` |
| x86 DLL | `3ADD73E49D66BB9D426BAFB0DE0C6E345AD0A5FDC311936E19382C896B7A13AE` |

统一 EXE、ZIP 均含两架构 DLL；管理中心为 x64。包含 PR #6 最终提交 `1b9ddd95c9519c3c49099fb7132fe0c09ef74e79`，保留贡献历史。 / Both packages contain both native DLLs and the x64 center. Includes PR #6 head `1b9ddd95c9519c3c49099fb7132fe0c09ef74e79`, preserving contribution history.

## 实测 / Runtime evidence

- **x64 宿主机**：Windows 11 Enterprise build 26300，Office `16.0.20527.20150` x64，DPI 192。最终哈希的三宿主生命周期、普通分组、关闭/取消与进程退出通过，保存后核对文件内容。混合组 7 项通过：三应用成员及窗口内标签边界、远端宽度同步、实际前台切换与后台遮蔽/任务栏、整组最小化还原、最大化还原与移动、关闭当前保留两成员、恢复解组。普通/Strict PPTX 原生重命名弹窗通过，保留 Presentation/HWND，退出后核对内容和命名空间。
- **x86 VMware**：Windows 11 Enterprise LTSC Evaluation build 26100、32 位 Microsoft 365 Apps，DPI 96。此前采样补丁基线为 `16.0.20527.20112`；本轮报告 16.0，补丁号未重新采样。`beta030-20261117-04` 同一最终 DLL/EXE 的安装注册、三宿主生命周期、保存后内容、语言及三宿主退出通过，混合专项因模块枚举驱动失败而整轮保留 failed。`beta030-20261117-10` 专项复测，混合组 7 项、普通/Strict PPTX 原生重命名、中心入口退出、修复、保留设置卸载、重装及最终注册共 10 阶段通过。
- **管理中心**：最终 EXE 经真实 WebView2 回环 CDP 验证默认 920×700、最小 420×480，中英/跟随 Windows、通道保存、无 Office 失败提示、窗口关闭及进程退出；宿主机到 Word 的混合组选择器正向调用与取消也通过。客机侧重 x86 架构选择与维护集成，未重复相同 EXE 的完整界面矩阵。
- **收尾**：客机证据按索引逐件长度/哈希核对，清理回执通过、唤醒释放、正常关机。宿主机独立试用副本保留打开，未纳入公共附件。[截图](beta-0.3.0-gallery.md)仅展示此 Beta，避开账号信息。

Host x64 tests used Windows 11 Enterprise build 26300, Office `16.0.20527.20150`, DPI 192. All three hosts passed ordinary lifecycle/grouping, close/cancel and process-exit checks, with saved content checked after exit. Seven mixed-group checks covered membership/in-window bounds, shared widths, real foreground/presentation/taskbar changes, group minimize/restore, maximize/restore/move, surviving members after native close, and recovery. Regular/Strict PPTX native rename dialogs retained the presentation/window and passed post-exit content/namespace checks.

Real x86 Office ran in VMware on Windows 11 Enterprise LTSC Evaluation build 26100, DPI 96. The previously sampled patch baseline was `16.0.20527.20112`; this run reported 16.0 without resampling the patch. Job `beta030-20261117-04` passed final-payload ordinary lifecycle/save/language/exit stages but remains failed due to the mixed-test module-enumeration driver. Job `beta030-20261117-10` passed ten focused stages on identical payloads: install, registration, mixed grouping, PPTX rename, center entry/exit, repair, uninstall, reinstall and final registration. Original failures remain separate. Indexed evidence and cleanup passed, wake was released, and the VM shut down normally.

The final center passed real WebView2 checks at default/minimum sizes in English/Chinese/automatic language, channel persistence, negative grouping feedback, window close and process exit. Its positive Word chooser/cancel path also passed. The VM covered x86 installation and center integration rather than duplicating the same x64 center UI matrix.

## 证据索引 / Local evidence IDs

These identifiers refer to private local evidence, deliberately excluded from releases:

| Area | Evidence |
| --- | --- |
| x64 lifecycle / process exit | `beta030-host-a7091acda82c43178ee4a8942acbbdb9` |
| x64 mixed group | `mixed-office-55ba125b08194023b617d46f96240e9b` |
| x64 PPTX native rename | `ppt-live-rename-22db3f054931444380eff86892fedfdb` |
| Center full UI / positive grouping | `runtime-tests/b9903c5420494a6581e4388a7b789ef8`, `center-mixed-positive-00cd14e243e34a8f9681b4e686082215` |
| x86 ordinary / focused retest | `runtime-vm/beta030-20261117-04`, `runtime-vm/beta030-20261117-10` |

## 离线与失败边界 / Offline checks and limits

双架构原生回归、ABI/导出、81 项服务检查、32 项 PowerShell 服务检查、诊断集成、翻译覆盖、双载荷自检、ZIP 路径/许可/校验和及 Beta 更新签名通过。普通分组实测发现同 Session 拖放被错误转为混合组，修正后最终两架构编译通过，并由上述两端普通生命周期覆盖该路径。离线检查不替代 Office 验收。

保留的驱动失败包括 WOW64 模块枚举、初始前台获取、没有独立 HWND 的 Word 取消按钮和一次重复 C# 声明。最终驱动使用包含 WOW64 的共享枚举、绑定所有权的 UIA 取消和经命中核对的初始原生点击；后续切换直接核对真实前台。一次 MCP 补丁号查询因引号传递失败，未算采样成功：工具成功或退出码 0 不代表命令得到预期结果。方法见[组合验证流程](vm-verification-methods.md)。

Offline native/ABI, service (81), PowerShell (32), diagnostics, localization, dual-payload package/checksum and Beta signature checks passed. A same-session ordinary-drag regression was corrected before the final build and exercised by both final-payload runtime lifecycle suites. Retained driver failures include WOW64 enumeration, initial foreground acquisition, Word cancellation without an independent HWND, and a duplicate C# declaration. A later MCP patch-version query failed due to quoting despite process exit 0; it is not counted as a successful sample.

命令/窗口消息不代表人工拖动手感或逐帧视觉验收。混合组公式编辑、模态保存、保护视图、放映、多显示器/混合 DPI、崩溃恢复及其他 Office 构建没有完整矩阵。本版为实验性公开 Beta，不宣称既有间歇性问题彻底解决；请按模板提供构建/位数、复现及脱敏诊断。

Command/native-message tests do not establish human drag smoothness or frame-by-frame visual acceptance. Mixed-group formula editing, modal saves, Protected View, slide shows, multiple monitors/mixed DPI, crash recovery and other Office builds are not exhaustively covered. This experimental beta does not establish a complete fix for historical intermittent reports. Use the issue template with build/bitness, reproduction and redacted diagnostics.
