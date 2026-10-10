# openOfficetab 0.1.1 · Windows x64

为 Word、Excel、PowerPoint 添加原生文档标签。免费，MIT 开源。

## 安装

保存文档并退出三款 Office，将 ZIP 完整解压，双击 `Install.cmd`。按当前 Windows 用户注册，无需管理员权限。重新打开 Office 文档后，标签栏出现在功能区下方。

也可以从 GitHub Release 下载 EXE 安装程序，点击“安装 / 更新”。两个入口安装相同的 DLL。安装不会自动关闭 Office 窗口。

## 使用

- 点击切换，拖动排序，点击 × 或中键关闭。
- 标签后的 + 新建，向下箭头列出文档，… 打开设置。
- 右键“在独立窗口中显示”，将当前窗口移出标签组；原文件和未保存内容保留。
- 在独立窗口的 openOfficetab 功能区点击“加入标签组”恢复。
- 保存全部标签和批量关闭排除独立窗口，关闭时保留 Office 原生保存提示。

## 卸载

保存文档并退出 Office，双击 `Uninstall.cmd`，或在 EXE 安装程序中点击“卸载插件”。默认保留设置和日志。需要清除设置时，在解压目录运行：

```powershell
powershell -NoProfile -File scripts/uninstall.ps1 -RemoveSettings
```

设置和日志位于 `%LOCALAPPDATA%\openOfficetab`。遇到未显示标签的情况，先重开 Office，再核对 COM 加载项及标签显隐设置。

已验证 Windows 11、Office Home 2024 x64（16.0.20430.20146），200% 缩放。不支持 Office 网页版或 macOS。其他 Office 版本、x86、ARM64 尚未验证。安装程序尚未做代码签名；Release 提供 SHA256SUMS 校验文件，源码可自行构建。

完整说明、截图、源码和问题反馈：[xlennart/openOfficetab](https://github.com/xlennart/openOfficetab)。
