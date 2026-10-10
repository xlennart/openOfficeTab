openOfficetab 首次公开发布，为 Windows 桌面版 Word、Excel、PowerPoint 增加原生文档标签。

### 功能

- 点击切换、拖动排序、新建、保存、右键批量管理、标签锁定。
- 文档可移出标签组，保留原窗口与未保存编辑；可随时加回。
- 三款应用分别使用蓝、绿、橙红强调色，圆角标签和面板。
- 减少新建时重复布局；修正开发环境隔离导致普通桌面安装不可见的问题。

### 下载与安装

| 文件 | 用途 |
| :--- | :--- |
| `openOfficetab-0.1.1-setup-x64.exe` | 双击后选择安装 / 更新或卸载 |
| `openOfficetab-0.1.1-win-x64.zip` | 完整解压后运行 Install.cmd / Uninstall.cmd |
| `SHA256SUMS.txt` | EXE 和 ZIP 的 SHA-256 校验值 |

请先保存文档并退出三款 Office。安装按当前用户注册，无需管理员权限；安装程序不会替你关闭文档。安装程序未做代码签名，源码与构建步骤均在仓库中。

### 验证与限制

实机验证：Windows 11、Office Home 2024 x64，16.0.20430.20146，200% 缩放。验证了三宿主加载、标签布局、新建和独立窗口流程；安装器嵌入载荷校验已通过，按钮 UI 交互尚未独立自动验收。x86、ARM64、其他 Office 版本尚未验证。不支持网页版和 macOS。

完整兼容性和验证边界见 [README](https://github.com/xlennart/openOfficetab#兼容性) 和 [验证记录](https://github.com/xlennart/openOfficetab/blob/main/docs/validation.md)。
