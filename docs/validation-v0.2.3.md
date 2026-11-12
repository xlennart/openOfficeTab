# 0.2.3 验证记录 / Validation record

2026-11-12，项目所有者完成本地基本试用并授权正式发行。 / The project owner completed basic local acceptance and authorized stable publication on 2026-11-12.

## 载荷及范围 / Payload identity and scope

| Payload | SHA-256 |
| --- | --- |
| Unified EXE | `2CA21C7DBFB2C651EC7E38B5F539B2C1F91A1170F110A1EFA9318F87FF224B66` |
| x64 DLL | `39F51425A74434D4662A34AAE7AF4EEF95B2595AAB7BDF985E2F6C9EBB6CBEA1` |
| x86 DLL | `D24B6ACC83A329B64EF5C145DE519D107D1E5FC7FAA3C7AAE6DA6F5C5ED129AC` |

EXE/ZIP 保留双载荷，中心 x64。ZIP 内说明在发行前定稿，最终附件哈希见同版 `SHA256SUMS.txt`。纳入 #7 `6bfd683`、#6 `eae73dc` 及维护者修正；#6 后续 `75be699` 的 PowerPoint 保持打开另存流程未纳入本版。 / EXE/ZIP retain both native payloads and the x64 center. Packaged documentation is finalized before publication; final asset hashes are in the release checksum file. Includes #7 `6bfd683`, #6 `eae73dc` and maintainer fixes; the later PowerPoint keep-open SaveAs flow in `75be699` is excluded.

## 实机结果 / Runtime results

- **x64 宿主机**：Windows 11 Enterprise build 26300，加载的 Office `16.0.20527.20150` x64。三宿主最终模块哈希已核对。最终 `host-critical` 的三宿主保存后文件内容、进程退出、安装/注册及中英文/自动语言同步通过，试件和临时状态已收尾。 / Host Windows 11 Enterprise build 26300, loaded Office `16.0.20527.20150` x64. All three final modules were fingerprinted. Final `host-critical` passed saved contents after exit, process exit, install/registration and English/Chinese/automatic language synchronization; fixtures and temporary state were cleaned up.
- **x86 VMware 虚拟机**：Windows 11 Enterprise LTSC Evaluation build 26100，32 位 Microsoft 365 Apps；此前核验的 Office 基线 `16.0.20527.20112`，本轮未重采补丁号。最终 `stable023-x86-20261112-03` 使用上述 EXE/x86 DLL，统一安装、注册、三宿主生命周期、保存、示例打开、语言、三宿主退出、中心入口/退出、修复、卸载、重装和最终注册共 16 阶段全部通过。索引校验、清理回执通过，唤醒释放、正常关机。 / VMware Windows 11 Enterprise LTSC Evaluation build 26100, 32-bit Microsoft 365 Apps. Previously verified Office baseline: `16.0.20527.20112`; its patch number was not resampled. Final job `stable023-x86-20261112-03`, using the EXE/x86 DLL above, passed all 16 install/registration, three-host lifecycle, save, fixtures, language, three-host exit, center entry/exit, repair, uninstall, reinstall and final-registration stages. Evidence-index checks and cleanup passed; wake was released and the VM shut down normally.
- **用户验收**：已安装候选及独立文档副本由所有者试用，确认基本可用。最后仅调整关闭图标及复选框绘制，保留原生状态、Space/Tab 和系统高对比度回退；按用户指示不重复完整界面矩阵。 / Owner acceptance used the installed candidate and separate document copies. Final cosmetic glyph/checkbox changes retain native state, Space/Tab and system high-contrast fallback; the full UI matrix was not repeated at the owner's request.

## 失败与限制 / Failures and limits

- 前一绘制版本的 Word/Excel 生命周期复测通过，不改绑到最终哈希。原 Word 首轮及 PowerPoint 坐标命中失败保留。最终宿主机 PowerPoint 同一驱动仍失败：采集时试件在屏外最小化坐标 `-32000`，目标命中为 0；该项不计作拖放通过，停止重复接口，不据此断言插件故障。客机命令通过不代表宿主机人工拖动手感。 / Earlier Word/Excel lifecycle retests passed on the preceding cosmetic build and retain their original hashes. Initial Word and PowerPoint coordinate failures remain recorded. Final host PowerPoint still failed that driver: captured fixtures were minimized off-screen at `-32000`, with no hit target. This is not counted as a host drag/drop pass; repeated use stopped and does not alone establish a product defect. Guest command checks do not establish host manual drag quality.
- 早期客机报告锁和 PowerShell 空字符串转换失败保留；最终新 jobId 已通过，不使用旧结果替代本轮证据。 / Earlier guest report-lock and PowerShell null-string failures remain recorded; the final fresh job passed without substituting stale results.
- 作者重命名格式/过渡探针、模拟和离屏预览保留各自提交及范围，不等于最终菜单全部路径通过。宏/模板/放映、真实并发、网络共享、全部高对比度/缩放/辅助技术及其他 Office 构建未全量覆盖。 / Author format/transition probes, mocks and off-screen previews retain their original commits and scope, without claiming every final menu path. Macro/template/show formats, real races, network shares, all high-contrast/scaling/accessibility combinations and other Office builds are not exhaustively covered.
- issue #5 唯一根因未确认，保留焦点保护与内置诊断，不宣称彻底解决间歇故障。 / Issue #5 has no confirmed sole cause; focus protection and built-in diagnostics do not prove complete resolution.

## 离线及发行检查 / Offline and release checks

- 两架构构建、各 10 组原生回归、五个导出和 COM ABI、81 项服务、32 项 PowerShell、诊断时间线、本地化及夹具检查通过。 / Both builds, ten native suites each, five exports/COM ABI, 81 service checks, 32 PowerShell checks, diagnostic timeline, localization and fixture checks passed.
- 统一包分别核验嵌入架构/哈希、白名单、许可及签名更新清单；更新签名不等于 EXE 代码签名。 / Package architectures/hashes, whitelist, licensing and signed update metadata are checked separately; update signing is not EXE code signing.
- OSS 是最后兜底，既有 0.2.2 四附件已上传并匿名校验；0.2.3 使用与 GitHub/CNB 相同字节。发布步骤另行核对双端 latest、匿名附件哈希及 RSA 签名，结果保存在本机发行回执。 / OSS is the final fallback; existing 0.2.2 assets were mirrored and anonymously verified. Version 0.2.3 uses identical bytes to GitHub/CNB. Publication separately checks both latest pointers, anonymous hashes and RSA signatures, with local receipts.
- 源码、附件和 ZIP 解压内容扫描本机已知 OSS 凭据与私钥值，扫描不输出密钥；凭据、用户文档和本机治理资产不交付。 / Source, assets and uncompressed ZIP contents are scanned for known local OSS credentials/private-key values without logging them; credentials, user documents and local governance assets are excluded.

遇到问题请按故障模板提供步骤、Office 版本/位数及脱敏日志，必要时附视频，无需原文档。 / Report reproduction steps, Office version/bitness, redacted logs and a video when useful; original documents are unnecessary.
