# 0.2.2 — 2026-10-30

## 简体中文

统一的 Windows x64 安装器携带 x86、x64 原生插件，自动匹配 32 位或 64 位 Office；安装、修复和诊断使用同一架构判断，检查对应 COM 注册视图。保留共享设置和原有签名更新协议。新增两种架构的离线回归与 DLL 导出/调用验证。三款 Office 分别完成宿主机 x64 与虚拟机 x86 运行回归，具体范围见验证记录。

32 位 Office 测试环境为 VMware 虚拟机中的 Windows 11 企业版 LTSC 26100（64 位）与 Microsoft 365 Apps 16.0.20527.20112（32 位）；64 位 Office 在宿主机测试。如仍遇到问题，请按故障报告模板提交复现步骤、Office 版本与位数、导出的脱敏诊断日志，必要时附视频。

检查更新现在打开独立窗口，展示版本与更新说明；确认后下载并校验，Office 运行时等待退出再安装。直接显示保存文档和保留设置的提示。产品显示名统一为 openOfficeTab，窗口标题按页面及语言顺序更新。

修复更新窗口的确定与关闭行为，协调自动与手动检查，避免短暂的检查锁冲突；阻止后台分组布局覆盖用户刚选择的前台标签。修复 64 位管理中心漏判 32 位 Office 插件加载的情况；卸载前等待自有后台服务结束，避免清理缓存时文件占用。

## English

The unified Windows x64 installer includes native x86 and x64 add-ins and selects the matching Office architecture. Installation, repair and diagnostics share architecture detection and inspect the correct COM registry view. Shared settings and signed updates are preserved. Both native builds have offline regression and DLL export/ABI checks. Word, Excel and PowerPoint have runtime regression evidence on the x64 host and x86 VM; see the validation record for scope.

32-bit Office is tested in a VMware VM running Windows 11 Enterprise LTSC 26100 (64-bit) and 32-bit Microsoft 365 Apps 16.0.20527.20112. 64-bit Office is tested on the host. If a problem persists, follow the bug report template with reproduction steps, the Office version/architecture, an exported redacted diagnostic log, and a video when useful.

Check for updates now opens a separate window with the version and release notes. Updates are downloaded and verified after confirmation, then installed once Office exits. The save-work and settings-retention notice stays visible. The product display name is now openOfficeTab; window titles follow the active page and language in order.

Fixes confirmation and close actions in the update window, coordinates automatic and manual checks to avoid transient lock conflicts, and prevents background group layout from overriding an explicit foreground tab selection. Fixes 32-bit Office loading detection from the 64-bit center and waits for its own background service before uninstalling to avoid locked cache files.
