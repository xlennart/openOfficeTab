<div align="center">

# openOfficetab

**让 Word、Excel、PowerPoint 像浏览器一样管理文档。**

免费 · 开源 · 原生 C++ · Windows 桌面 Office

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Windows x64](https://img.shields.io/badge/platform-Windows%20x64-0078D4.svg)](#兼容性)
[![Build](https://github.com/xlennart/openOfficetab/actions/workflows/build.yml/badge.svg)](https://github.com/xlennart/openOfficetab/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/xlennart/openOfficetab)](https://github.com/xlennart/openOfficetab/releases/latest)

[下载安装](https://github.com/xlennart/openOfficetab/releases/latest) · [使用指南](#使用指南) · [反馈问题](https://github.com/xlennart/openOfficetab/issues) · [开发说明](docs/development.md)

</div>

---

openOfficetab 为桌面 Office 的功能区下方增加一条文档标签栏：点击切换、拖动排序、右键管理，并能将某个文档移出标签组，继续用独立窗口编辑。它保留 Office 原有文档对象和保存提示，不需要把文件转换成其他格式。

## 看看效果

### Word · 蓝色强调

![Word 文档标签栏](docs/images/word.png)

### Excel · 绿色强调

![Excel 工作簿标签栏](docs/images/excel.png)

### PowerPoint · 橙红色强调

![PowerPoint 演示文稿标签栏](docs/images/powerpoint.png)

截图来自空白示例文件，由使用者截取、审阅后提供；已避开账号、头像、私人路径及正文。

## 它能做什么

| 功能 | 操作与行为 |
| :--- | :--- |
| 文档标签 | 点击切换；未保存状态显示 `*`；拖动调整顺序 |
| 新建与列表 | 标签后的 `+` 新建，向下箭头列出文档，`…` 打开设置 |
| 快速关闭 | 点击 `×` 或中键；可设置双击关闭；保留 Office 原生保存提示 |
| 右键管理 | 保存、另存为、关闭其他 / 左侧 / 右侧 / 全部标签、打开所在文件夹 |
| 独立窗口 | 右键“在独立窗口中显示”；原窗口、原文件和未保存编辑保留 |
| 防误关 | 锁定标签，避免从标签栏误关闭 |
| 窗口协调 | 可选窗口跟随与单任务栏模式，默认关闭，可恢复 |
| 外观设置 | 圆角标签和面板；按应用配色；手动深色模式、顶部 / 底部位置、单标签自动隐藏 |

保存全部标签与批量关闭只处理标签组中的文档，会排除已移出的独立窗口。

## 安装

1. 从 [Releases](https://github.com/xlennart/openOfficetab/releases/latest) 下载 `openOfficetab-0.1.1-setup-x64.exe`。
2. 保存文档并退出 Word、Excel、PowerPoint。
3. 双击安装程序，点击 **安装 / 更新**。
4. 重新打开 Office 文档，标签栏会出现在功能区下方。

安装按当前 Windows 用户注册，无需管理员权限。安装程序不会替你关闭 Office 窗口。

也可下载 `openOfficetab-0.1.1-win-x64.zip`，完整解压后运行 `Install.cmd`。ZIP 包与 EXE 安装相同的插件；源码可从仓库获取。

Release 附有 `SHA256SUMS.txt`。安装程序尚未做代码签名，可先核对文件校验值，或自行从源码构建。

```powershell
Get-FileHash .\openOfficetab-0.1.1-setup-x64.exe -Algorithm SHA256
```

### 卸载

保存文档并退出三款 Office，再运行同一个安装程序，点击 **卸载插件**；或从解压包运行 `Uninstall.cmd`。默认保留设置和日志。

需要一并清除设置时，可在 ZIP 解压目录执行：

```powershell
powershell -NoProfile -File .\scripts\uninstall.ps1 -RemoveSettings
```

## 使用指南

### 让一个文档使用独立窗口

右键该文档的标签，选择 **在独立窗口中显示**。该窗口会隐藏标签栏、退出窗口跟随，并恢复独立任务栏入口；文档不会被关闭或重新打开。

想恢复标签时，在该窗口的 **openOfficetab** 功能区点击 **加入标签组**。

这项选择仅对本次会话中的当前窗口生效。关闭后重新打开文件，默认重新加入标签组。若同一文件有多个编辑窗口，移出的是所选窗口。

### 找到设置和恢复入口

点击标签后的 **…**，或在 **openOfficetab** 功能区打开 **标签设置**。功能区也提供显隐、刷新、窗口跟随和恢复入口。

标签锁定只防止从标签栏误关，不改变文件权限；深色标签需要手动选择。

## 兼容性

| 项目 | 当前状态 |
| :--- | :--- |
| 已实机验证 | Windows 11、Office Home 2024 x64、Word / Excel / PowerPoint 16.0.20430.20146 |
| 已验证缩放 | 200% 缩放；水平栏被本机 Office 限制为至少 80 物理像素，约 40 DIP |
| 发布架构 | x64；按 **Office 的位数**匹配，而非仅看 Windows 位数 |
| x86 / 其他 Office 版本 | 构建脚本预留 x86；尚未做运行验证，不包含 x86 安装包 |
| Office 网页版 / macOS | 不支持；这是 Windows 原生 COM 加载项 |

目前是可使用的早期版本，不承诺覆盖成熟商用标签插件的全部功能。尚未提供 Office 主题自动跟随、全局快捷键和完整辅助技术语义。验证方法与边界见 [验证记录](docs/validation.md)。

## 常见问题

**双击文件却没有标签栏？**

先保存并退出 Office，重新安装后再打开文档；然后在 COM 加载项列表检查 `openOfficetab` 是否加载。若插件已加载，可从功能区检查显隐及“单标签时自动隐藏”设置。Office 安全模式、受保护视图或组织策略可能限制加载项；请在普通编辑窗口核对，不需要修改 Office 安全设置。

从 Codex 开发环境执行安装时，脚本会交给独立桌面进程完成注册，避免开发环境中的注册表与 AppData 隔离造成安装只对测试窗口生效。

**配置和日志存在哪里？**

位于 `%LOCALAPPDATA%\openOfficetab`。配置由三款应用共享，并在各实例启动时读取。问题反馈建议只提供相关错误行，避免公开真实文件名和路径。

**修改设置后其他窗口没变化？**

不同 Office 应用实例目前不会自动同步加载后的设置。保存文档后重开对应实例即可读取新设置。

## 开发与参与

安装 Visual Studio Build Tools 的 C++ 工具和 Windows SDK 后：

```powershell
pwsh -File .\scripts\build.ps1 -Architecture x64
pwsh -File .\scripts\package.ps1
```

构建产生 `dist/x64/openOfficetab.dll`；打包产生 EXE、ZIP 和校验文件。EXE 使用 Windows 自带 .NET Framework 编译器生成。GitHub Actions 会编译并校验安装包；Office 的实际运行测试需要本机安装桌面 Office。

源码、测试与发布流程见 [开发说明](docs/development.md)；与 Office Tab 的差别及建议见 [功能对比与开发优先级](docs/comparison.md)。欢迎通过 Issues 提交可复现的问题或功能建议；请注明 Windows、Office 版本、位数及缩放比例。

## 许可证

[MIT](LICENSE)。openOfficetab 独立实现，不包含对标软件的源码或二进制，与 Microsoft、ExtendOffice 等厂商无隶属关系。Office 及其他产品名称归各自权利人所有。
