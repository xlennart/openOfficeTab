# openOfficeTab 0.2.3-beta.3

诊断增强 Beta，根因仍待定位。 / Diagnostic Beta; the root cause remains under investigation.

## 简体中文

- 点击与激活取证集成进插件，使用原有 **管理中心 → 诊断与修复 → 导出诊断与日志**，无需单独运行采集工具。
- 记录实际点击接收窗口、命中码、激活请求和窗口处理前后状态，并记录有界的同组窗口层级、DPI 与临时遮蔽状态。
- 导出实际加载模块路径所指文件的 SHA-256、可读状态和进程身份，不导出模块路径或文档内容；不记录键盘输入，不自动上传。
- 此版本用于继续定位 issue #5。现有证据尚未确认唯一根因，不宣称彻底修复；issue 保持打开。
- 本轮按用户要求仅进行宿主机 64 位 Office 实机验证；32 位载荷执行编译检查，未在虚拟机重新实测。完整覆盖及载荷身份以本版验证记录为准。
- 默认正式更新通道不变；Beta 为手动选择的候选版本。试用前保存文档并退出 Office，出现问题按故障模板补充复现步骤、Office 版本/位数和脱敏诊断。

## English

- Mouse and activation observation is built into the add-in. Use **Diagnostics & repair → Export diagnostics and logs**, without a separate collector.
- Records actual mouse recipients, hit codes, activation requests and before/after window state, plus bounded group order, DPI and temporary mask state.
- Exports hashes of files at actual loaded-module paths, readability and process identity. Module paths, document content and keystrokes are excluded. Nothing is uploaded automatically.
- This diagnostic candidate continues issue #5 investigation. A unique cause and complete resolution remain unproven; the issue stays open.
- At the user's request, this round tests real x64 Office on the host only. The x86 payload receives a compile check; it is not retested in the VM. See this version's validation record for exact coverage and payload identities.
- Stable remains the default update channel; Beta is an explicit opt-in. Save documents and exit Office before installing. Report failures with reproduction steps, Office version/architecture and exported redacted diagnostics.
