# openOfficeTab 0.3.0-beta.1

## 简体中文

首个混合 Office 标签组公开测试版。默认正式通道仍为 0.2.3；选择 Beta 通道后才接收本版。

- Word、Excel、PowerPoint 可加入同一临时组。标签仍在 Office 窗口内；切换显示对应应用的真实功能区和文档，后台窗口遮蔽且可恢复，仅当前成员保留任务栏入口。
- 合并已有组保留全部成员，单标签宽度同步。整组最小化、最大化、还原和移动；关闭当前文档保留剩余组。各应用仍拥有自己的原生窗口，未跨进程重挂编辑器。
- 标签应用图标取自本机 Office，统一底色、尺寸和对齐；菜单图标使用抗锯齿绘制。“…”菜单精简，详细设置进入子菜单。
- Office 的 openOfficeTab 功能区及管理中心概览新增混合组入口。功能区新增重命名文件；混合组当前应用的标签也可重命名。
- 合入 [PR #6](https://github.com/xlennart/openOfficeTab/pull/6)，感谢 @lmdbot：普通及 Strict PPTX 重命名保持演示文稿打开，核对新文件后安全清理旧路径。Word/Excel 及其他 PowerPoint 格式保留原有保存、关闭、原子改名和重新打开流程。

混合组为实验功能，尚不保存工作区，也不提供跨应用批量保存/关闭。公式编辑、Office 模态提示、受保护视图、放映及跨屏场景存在覆盖边界，请先用文档副本试用。可从“恢复标签栏与窗口”退出混合协调。

验证环境和结果见[本版验证记录](validation-v0.3.0-beta.1.md)。32 位 Office 的测试环境是 VMware 虚拟机，64 位 Office 在宿主机验证；自动化窗口消息不等于人工拖动手感。问题请按故障模板提供 Office 版本/位数、复现步骤及管理中心导出的脱敏诊断，必要时附视频。本版不宣称既有间歇性故障已彻底解决。

GitHub、CNB 交付相同双架构安装器与 ZIP，并保留签名更新元数据和哈希校验；OSS 仅作最后兜底。Beta 不更新正式 latest 或官网默认稳定下载。

## English

The first public beta of mixed Office tab groups. Stable remains 0.2.3; opt into the Beta channel to receive this build.

- Group Word, Excel and PowerPoint documents temporarily. Tabs stay inside Office; selecting one presents its real Ribbon and document. Background windows are recoverably concealed, with only the selected member retained on the taskbar.
- Merge entire existing groups and synchronize per-tab widths. Minimize, maximize, restore and move the group; closing the current document retains surviving members. Each application keeps its native windows; editors are not reparented across processes.
- Application icons come from local Office installations, with aligned, equally sized badges. Menu strokes use antialiasing. The shorter **…** menu keeps detailed preferences in a submenu.
- Mixed-group entries are available in the openOfficeTab Ribbon and management center overview. Rename is also available on the Ribbon and on local mixed-group tabs.
- Includes [PR #6](https://github.com/xlennart/openOfficeTab/pull/6), with thanks to @lmdbot: regular/Strict PPTX rename keeps the presentation open and safely cleans up the original after verifying the destination. Word, Excel and other PowerPoint formats retain save/close/atomic rename/reopen behavior.

Mixed grouping is experimental. Workspaces are not persisted and cross-application bulk save/close is not supported. Formula editing, modal prompts, Protected View, slide shows and multi-monitor scenarios have coverage limits. Start with disposable document copies; **Recover tab bar and windows** exits mixed coordination.

See [version validation](validation-v0.3.0-beta.1.md) for actual environments/results. 32-bit Office is tested in VMware; 64-bit Office is tested on the host. Native-message automation does not establish human drag smoothness. Report issues with Office build/bitness, reproduction steps and redacted diagnostics exported by the management center; attach a video when useful. This beta does not establish that existing intermittent faults are fully resolved.

GitHub and CNB deliver identical dual-payload installers/ZIPs with signed update metadata and hash checks. OSS remains the final fallback. Beta does not replace stable latest or the website's default stable download.
