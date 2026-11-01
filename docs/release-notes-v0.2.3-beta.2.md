# openOfficeTab 0.2.3-beta.2

[验证记录 / Validation](validation-v0.2.3-beta.2.md)

测试环境 / Test environments: x64 — Windows 11 Enterprise 26H2 26300.9711, Word/Excel/PowerPoint 16.0.20527.20088 x64; x86 — **VMware**, Windows 11 Enterprise LTSC Evaluation 26100.9711, Word/Excel/PowerPoint 16.0.20527.20112 x86. 其他 Office 构建与完整人工拖动观感未覆盖。 / Other Office builds and the full physical-drag matrix are not covered.

## 简体中文

- 修复一个已可控复现的焦点问题：后台标签窗格收到 OLE UI 激活通知时，可能激活它所属的文档窗口。现在仅允许当前前台、活动且可见启用的窗格接收该焦点；原生拖动和 Excel 编辑期间拒绝此焦点转移。
- 新增回归测试，原代码会失败，修复后通过；保留诊断日志与正式版/Beta 通道。
- 统一安装器继续包含 x86、x64 Office 插件，按实际 Office 位数选择；管理中心仍要求 64 位 Windows。
- 这是 **候选修复 Beta**。用户提供的日志没有记录 OLE 焦点调用，不能确认这是 issue #5 现场的唯一根因，也不宣称间歇故障已彻底解决。issue 保持打开。
- 32 位 Office 在 VMware 虚拟机中测试，具体系统、Office 版本、载荷哈希及覆盖边界见本版验证记录。程序接口测试不替代人工拖动观感。
- 如仍异常，请附复现步骤、大致时间、Office 版本和位数，以及管理中心导出的脱敏诊断 JSON；必要时附视频，无需敏感原文档。
- 默认仍使用正式通道，正式最新版保持 0.2.2；选择 Beta 后可从 beta.1 升级至 beta.2，切换通道不降级。

## English

- Fix a reproducible focus defect: an OLE UI-activation callback for a background tab pane could activate its document window. Focus is now restricted to a visible, enabled pane in the already active foreground root, and suppressed during native moves and Excel editing.
- Add a regression that fails on the original code and passes with the fix. Preserve diagnostic logging and Stable/Beta channels.
- The unified setup still includes x86 and x64 Office add-ins and selects by actual Office bitness. The management center requires 64-bit Windows.
- This is a **candidate-fix Beta**. The reporter's log does not trace the OLE focus call, so it cannot establish this as the unique cause of issue #5 or prove that every intermittent trigger is resolved. The issue stays open.
- x86 Office is tested in a VMware VM. See this version's validation record for actual OS/Office versions, payload hashes and coverage limits. API-driven checks do not establish manual drag smoothness.
- If symptoms persist, provide reproduction steps, approximate time, Office version/bitness and redacted diagnostics exported from the management center; add video when useful. Sensitive original documents are unnecessary.
- Stable remains the default channel and latest stable stays 0.2.2. The Beta channel permits beta.1 → beta.2; channel changes never downgrade.
