# openOfficeTab 0.2.3

## 简体中文

测试环境：宿主机 Windows 11 26300、Office 16.0.20527.20150 x64；VMware Windows 11 LTSC 26100、Microsoft 365 Apps x86（此前核验基线 16.0.20527.20112，本轮未重采补丁号）。实际覆盖与限制见验证记录。

- 原生菜单统一图标、间距、选中状态和级联子菜单；调整宽度、颜色及关闭确认对话框跟随明暗主题。管理中心窗口按钮保留键盘与无障碍名称。
- 已保存的本机文件可从标签右键“重命名文件…”。先保存原文件，再关闭、以不覆盖目标的方式重命名并重新打开；操作前明确提示撤销历史会重置。只读、多窗口、远程路径及不支持的格式保留限制。捕获遮罩失败不阻止重命名，遮罩不全局置顶。
- 纳入 Beta 的后台窗格焦点保护与点击、激活、加载模块指纹诊断；这是已知路径的修复和诊断增强，不证明间歇性故障的唯一根因或彻底解决。
- 管理中心“关于与更新”加入爱发电赞助按钮，保留完整分享链接。
- 下载路由加入 OSS 最后兜底；仍要求独立签名、大小与 SHA-256 校验。上传身份和密钥不进入客户端或源码。发行镜像与其他平台使用相同字节。
- PR 规范要求作者至少亲自执行一次适用验证并记录结果；实机资源不足可由维护者审查和补测，AI 描述不能作为验证证据。

同一安装器和 ZIP 包含 x86/x64 原生载荷，中心为 x64。实际验证状态见[本版记录](validation-v0.2.3.md)。32 位 Office 的测试环境为 VMware 虚拟机；其他 Office 构建与显示环境存在覆盖边界。遇到问题请按故障模板提供复现步骤、Office 版本及位数、管理中心导出的脱敏日志，必要时附视频。

感谢 [@lmdbot](https://github.com/lmdbot) 的 PR #6、#7 及验证材料；维护者负责集成、补充修正和最终验收。

## English

Test environments: host Windows 11 26300, Office 16.0.20527.20150 x64; VMware Windows 11 LTSC 26100, Microsoft 365 Apps x86 (previously verified baseline 16.0.20527.20112, patch number not resampled). See the validation record for actual coverage and limits.

- Native menus share consistent icons, spacing, selection states and cascading submenus. Width, color and close-confirmation dialogs follow the light/dark theme. Management-center window buttons retain keyboard support and accessible names.
- Use **Rename file…** in a saved local document's tab menu. The add-in saves the original, closes it, renames without replacing an existing destination, and reopens it. A warning explains that undo history resets. Read-only files, multiple windows, remote paths and unsupported formats remain restricted. A failed transition capture does not block renaming; its cover is not globally topmost.
- Incorporates the Beta background-pane focus protection and click, activation and loaded-module fingerprint diagnostics. These changes address known paths and improve diagnosis; they do not establish the sole cause or complete resolution of intermittent problems.
- **About & updates** includes a **Sponsor on Afdian** button with the complete sharing URL.
- Adds OSS as the final download fallback, preserving independent signature, size and SHA-256 checks. Upload credentials stay outside client assets and source. Mirrored assets use the same bytes as the other platforms.
- PR authors should personally run at least one relevant check and report the result. Maintainers may supplement unavailable runtime coverage; AI-generated descriptions are not validation evidence.

The identical EXE and ZIP contain both native architectures; the center is x64. See the [version validation record](validation-v0.2.3.md) for actual status. The 32-bit Office environment is a VMware virtual machine, with limits across other Office builds and display setups. Report problems using the issue template with reproduction steps, Office version and bitness, exported redacted diagnostics, and a video when useful.

Thanks to [@lmdbot](https://github.com/lmdbot) for PRs #6 and #7 and the supplied evidence. Maintainers handle integration, additional fixes and final acceptance.
