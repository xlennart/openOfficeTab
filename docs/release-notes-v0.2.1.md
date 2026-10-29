# openOfficetab 0.2.1

安装器与管理中心补齐中英文，并与原生插件共享“跟随 Windows / 简体中文 / English”语言偏好。静态文案、加载状态、更新与失败提示、可访问名称及预览示例均同步翻译；用户文档名称、路径和内容不翻译。

语言入口移到管理侧栏底部或安装内容区，标题栏保留品牌与窗口控件。修正英文长文案换行、小窗口导航及底部按钮布局；同步修正英文 README 和安装说明的控件名称。

保留 0.2.0 的结构化日志、签名更新与标签选择改进；本版没有新增 Office 窗口协调算法。原有间歇问题继续保持打开，若再次发生，请从管理中心导出诊断与日志并附到原 issue。

提供 Windows x64 EXE、ZIP、SHA256SUMS.txt 和签名更新清单，GitHub 与 CNB 使用相同附件。实际验证范围及限制见 [验证记录](validation.md)。

## English

Setup and the management center now support English and Simplified Chinese, sharing the native add-in's saved language preference. This covers static labels, runtime status, updates, errors, accessible names and preview samples. User document names, paths and content remain unchanged.

The language control now lives in the sidebar or setup content area. Long labels wrap, smaller windows have a responsive layout, and window controls and primary footer actions remain accessible. English README and installation instructions use the actual English control names.

This version retains the structured logs, signed updates and tab-selection improvements from 0.2.0. It introduces no new Office window-coordination algorithm. Existing intermittent issues remain open; if one recurs, export diagnostics and logs from the management center and attach them to the original issue.

Windows x64 EXE, ZIP, checksums and signed update metadata are identical on GitHub and CNB. See the validation record for the tested scope and remaining limits.
