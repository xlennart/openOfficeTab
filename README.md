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

当前版本为 0.1.4，提供源码、EXE 安装程序和 ZIP 安装包。修复与验证范围见 [验证记录](docs/validation.md)。

## 看看效果

### Word · 蓝色强调

独立标签栏（保留样式）：

![Word 文档标签栏](docs/images/word.png)

融合标签栏（`#F0F0F0`）与插件功能区：

![Word 融合标签栏与插件功能区](docs/images/word-integrated.png)

### Excel · 绿色强调

独立标签栏（保留样式）：

![Excel 工作簿标签栏](docs/images/excel.png)

融合标签栏（`#F0F0F0`）与插件功能区：

![Excel 融合标签栏与插件功能区](docs/images/excel-integrated.png)

### PowerPoint · 橙红色强调

独立标签栏（保留样式）：

![PowerPoint 演示文稿标签栏](docs/images/powerpoint.png)

融合标签栏（`#F0F0F0`）与插件功能区：

![PowerPoint 融合标签栏与插件功能区](docs/images/powerpoint-integrated.png)

截图来自空白示例文件，由使用者截取、审阅后提供；已避开账号、头像、私人路径及正文。
每款应用同时保留“独立标签栏”和“背景融合”截图，方便比较两种样式；新版默认融合底色为 `#F0F0F0`。

## 它能做什么

| 功能 | 操作与行为 |
| :--- | :--- |
| 文档标签 | 点击切换；未保存状态显示 `*`；拖动调整顺序 |
| 新建与列表 | 标签后的 `+` 新建，向下箭头列出文档，`…` 打开设置 |
| 快速关闭 | 点击 `×` 或中键；可设置双击关闭；最后文档关闭后退出应用，取消关闭则继续编辑 |
| 右键管理 | 保存、另存为、关闭其他 / 左侧 / 右侧 / 全部标签、打开所在文件夹 |
| 独立窗口 | 右键“在独立窗口中显示”；原窗口、原文件和未保存编辑保留 |
| 多个标签组 | 拖出形成新标签组窗口，拖到另一个标签栏合并；也可通过菜单选择目标组 |
| 标签宽度 | 拖动标签右边缘单独调宽（80–360 DIP）；也可统一设置固定、按标题或压缩 |
| 诊断与恢复 | 查看加载状态、导出脱敏诊断、恢复标签栏与窗口；安装器可检查和修复注册 |
| 防误关 | 锁定标签，避免从标签栏误关闭 |
| 窗口协调 | 同组切换沿用当前窗口的尺寸和位置，适配分屏；独立窗口和其他组不跟随。单任务栏模式仍可选，默认关闭 |
| 外观设置 | 浅灰融合开口标签 / 独立白色圆角面板；强调线顶部 / 底部 / 无；按应用配色、手动深色模式 |

保存全部标签与批量关闭只处理当前标签组中的文档，会排除独立窗口和其他标签组。有其他独立文档、隐藏工作簿或受保护视图时，关闭最后一个可见标签不会替你退出应用。

## 安装

1. 从 [Releases](https://github.com/xlennart/openOfficetab/releases/latest) 下载 `openOfficetab-0.1.4-setup-x64.exe`。
2. 保存文档并退出 Word、Excel、PowerPoint。
3. 双击安装程序，点击 **安装 / 更新**。
4. 重新打开 Office 文档，标签栏会出现在功能区下方。

安装按当前 Windows 用户注册，无需管理员权限。安装程序不会替你关闭 Office 窗口。

也可下载 `openOfficetab-0.1.4-win-x64.zip`，完整解压后运行 `Install.cmd`。ZIP 包与 EXE 安装相同的插件；源码可从仓库获取。

Release 附有 `SHA256SUMS.txt`。安装程序尚未做代码签名，可先核对文件校验值，或自行从源码构建。

```powershell
Get-FileHash .\openOfficetab-0.1.4-setup-x64.exe -Algorithm SHA256
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

想恢复标签时，在该窗口的 **openOfficetab** 功能区点击 **加入 / 移动到标签组**，选择目标组。功能区和标签菜单也提供 **新建为独立窗口**。

这项选择仅对本次会话中的当前窗口生效。关闭后重新打开文件，默认重新加入标签组。若同一文件有多个编辑窗口，移出的是所选窗口。

### 拆分与合并标签组

把标签拖到标签栏外，可形成保留标签栏的新窗口；把它拖到另一窗口的标签栏，可加入该组。右键 **移到新标签组窗口** 或 **移动到标签组…** 也可完成同样的操作。纯独立窗口隐藏标签栏，需要用功能区加回。

分组只在同一款 Office 的同一应用实例内生效，目前不跨进程迁移，也不在重启后保存组成员。

### 调节标签宽度

把鼠标移到标签的右边缘，出现横向调整指针后左右拖动，即可单独调整该标签的宽度。拖动边缘不切换或关闭文档，也不会移动标签组；按 Esc 取消本次调整。单标签宽度随当前编辑窗口保留，关闭文件后不保存到文件中。

使用统一的“标签宽度…”设置会清除当前会话的单标签调整，并重新应用统一宽度规则。

### 分屏时切换文档

把 Office 窗口放到屏幕一侧后，从这个窗口的标签栏切换文档，目标文档沿用当前窗口的实际位置和尺寸，不再沿用另一个文件的最大化状态。通过任务栏或 Windows 窗口切换不自动修改布局；移出的独立窗口和其他标签组保留各自布局。底层保留 Office 原有文档窗口，不重挂或重新打开文档。

“恢复窗口排列”可以恢复插件调整前的布局；如果你之后手动改变过某个窗口的布局，恢复会保留这次手动调整。

### 选择外观

点击标签后的 `…` → **标签外观**，可以独立选择：

- **融入应用背景**：默认浅灰 `#F0F0F0`，去掉白色面板和面板阴影；选中标签为白色，朝文档内容区开口。放到底部时，开口也朝内容区。
- **独立标签栏**：保留白色圆角面板和封闭的灰色选中标签。
- **强调线**：顶部、底部、无。Word 为蓝色，Excel 为绿色，PowerPoint 为橙红色。

默认使用浅灰融合样式和顶部强调线。这些外观选择会保存到当前用户设置；已运行的其他 Office 实例在重启后读取。切换外观只重绘插件控件，不重建 Office 窗格。深色颜色通过现有的“深色标签”选项选择，不会自动检测 Office 主题。

在 **… → 标签外观 → 标签栏颜色…** 输入 `#RRGGBB` 或用调色板选择颜色；自定义底色也可用于独立面板。点击 **恢复默认颜色**，融合样式恢复 `#F0F0F0`，独立面板恢复白色。取消对话框不改变设置；深色模式和系统高对比度优先于自定义颜色。

### 统一宽度规则

在 **… → 标签宽度…** 或功能区的 **标签宽度** 中设置 80–360。数值按显示缩放适配；标题长度与压缩模式将该数值作为上限。取消对话框不改变设置。

### 诊断和恢复

功能区提供 **加载诊断**、**导出脱敏诊断**、**恢复标签栏与窗口**。恢复会重建插件自己的标签栏、显示标签并关闭窗口跟随和单标签自动隐藏，保留文件、编辑、组成员和顺序。

插件完全没有加载时，用 EXE 安装器的 **加载诊断** 查看注册状态；保存并退出 Office 后再点 **修复注册**。诊断导出不包含文件名、路径、正文、账号或原始日志，修复不修改 Office 安全策略。

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

目前是可使用的早期版本，不承诺覆盖成熟商用标签插件的全部功能。尚未提供 Office 主题自动跟随、全局快捷键和完整辅助技术语义。验证方法与边界见 [0.1.2 验证记录](docs/validation-v0.1.2.md) 和 [首版验证记录](docs/validation.md)。

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
