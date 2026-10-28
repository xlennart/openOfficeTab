# openOfficetab 0.2.0

- 新增桌面管理中心，整合安装、外观设置、诊断修复和更新；从 Office 功能区进入时直接打开管理页。
- 对标签选择与原生文档窗口不同步的问题进行初步修复，完善活动视图核对和重绘。相关 issue 保持打开，继续收集现场反馈。
- 新增有容量限制的结构化运行日志。发生问题后进入 **管理中心 → 诊断与修复 → 导出诊断与日志**，将 JSON 附在原 issue 下并说明触发步骤、发生时间。
- 诊断默认不采集文档名称、路径、正文、单元格、账号或令牌，不自动上传。
- 检查更新时自动探测线路；验证发布签名与安装包 SHA-256，下载失败时尝试其他可用线路。
- 恢复原有字号和紧凑窗口，统一功能区图标及主次布局，修复静默安装的 PowerShell 模块路径继承问题。

Windows x64 / 64 位桌面 Office。安装前保存并退出 Word、Excel、PowerPoint；安装不会自动关闭文档，更新保留原设置。

本机完成离线回归、安装载荷核对及实际 Office 验证；另生成 51/61 MiB Excel 和 29/44 MiB Word 合成样本供使用者试用。Excel 基本正常；Word 拖动卡顿在临时停用插件后仍存在，本次不将其判定为插件独有问题。没有宣称已复现或彻底排除原报告的全部触发条件。详见 [验证记录](https://github.com/xlennart/openOfficetab/blob/main/docs/validation.md)。

## English

This release adds a desktop management center, initial improvements to tab/native-document selection alignment, and bounded structured runtime logs. Related issues remain open for follow-up. Export a sanitized JSON report from the management center's diagnostics page when reporting a recurring problem. Document names, paths, content, cell values, accounts and tokens are excluded by default; nothing is uploaded automatically.

Update checks select available routes automatically and verify signed metadata and installer hashes. The release also restores the original compact typography/window sizing, aligns Ribbon icon layouts, and fixes silent installation when launched through PowerShell 7.

Windows x64 and 64-bit desktop Office are required. Save and close Office before installing. Settings are preserved. Local validation does not establish complete coverage of every Office build or intermittent trigger.
