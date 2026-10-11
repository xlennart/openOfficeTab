# 开发说明

## 构建与打包

环境：Windows、Visual Studio Build Tools（C++ 与 Windows SDK）、PowerShell。首个发布版本使用 MSVC 14.44 与 Windows SDK 10.0.26100；默认 x64。

```powershell
pwsh -File scripts/build.ps1 -Architecture x64
pwsh -File scripts/package.ps1
```

`package.ps1` 将当前 DLL 和安装脚本组成最小 ZIP，再编译 `installer/Setup.cs`，将 ZIP 嵌入 EXE。它会通过安装程序的 `--verify` 入口校验嵌入 DLL 与构建输出一致。发布资产均位于 `delivery/`，不进入 Git。

安装程序不需要管理员权限。PowerShell 安装脚本通过当前用户 COM 注册与三个 Office 的 Addins 项启用加载；DLL 使用版本与内容校验值命名目录，已有相同 DLL 会校验并复用。由 Codex 打包环境运行时，`desktop-context.ps1` 通过桌面用户的 WMI 进程提供程序执行，避免私有 HKCU/AppData 视图；它校验调用用户身份并等待结果。

## 模块

| 路径 | 职责 |
| :--- | :--- |
| `src/addin.cpp` | COM 类工厂、加载项生命周期、Ribbon、注册、诊断 API |
| `src/session.cpp` | 三宿主适配、事件和增量刷新、窗格生命周期、文档命令、窗口协调 |
| `src/tabs.cpp`、`src/tabs.h` | ActiveX 占位、原生标签栏绘制与输入 |
| `src/model.h`、`src/com.h` | 状态模型、COM 接口与辅助对象 |
| `src/features.cpp`、`src/layout.h` | 诊断、宽度设置、组移动和纯宽度计算 |
| `scripts/` | 构建、安装、卸载、打包和桌面上下文桥接 |
| `installer/Setup.cs` | 按用户安装、卸载、加载诊断与修复的 WinForms 引导程序 |
| `tests/fixtures/` | 清除个人元数据与绝对路径的空白 Office 示例 |
| `tests/appearance-test.cpp` | 无 Office、无可见窗口的生产 GDI 绘制与标签边缘输入回归 |

每个 Office 编辑窗口拥有自己的 CTP 占位与标签渲染窗口。插件只调整自有控件对应的窗格标题框；文档仍由 Office 管理。新建使用 `FileNewDefault`，Word 窗口激活事件提前初始化，周期刷新补齐遗漏事件。所有 Office COM 访问都保持在宿主 STA 内。

## 验证

```powershell
python tests/check-fixtures.py
powershell -NoProfile -File tests/desktop-install-runtime.ps1 -OpenFixtures
powershell -NoProfile -File tests/new-document-runtime.ps1
powershell -NoProfile -File tests/independent-window-runtime.ps1
```

先安装插件，再在普通桌面 PowerShell 环境运行 Office 测试。运行测试期间请不要操作示例窗口。新建与独立窗口脚本要求该实例只有一个指定示例文件，否则停止，以保留其他文档。它们仅关闭自己创建的测试文档；失败时保留需要检查的窗口。测试输出放在 `artifacts/`，可能包含文档标题、窗口句柄及本机路径，不应直接公开上传。

测试脚本只从运行对象表附着已有示例；不会为了找不到文件的情况创建新的 Office 自动化实例。

CI 仅检查示例文件、编译源码和安装包载荷，不包含 Office UI 或实机测试。[office-events.csv](office-events.csv) 是开发期核对的三宿主事件声明；[manual-test-plan.csv](manual-test-plan.csv) 是更广的人工测试计划，标注“未执行”的项不能视作通过。

0.1.2 增加原生宽度单元测试（CI 也执行），以及三宿主生命周期回归：

```powershell
pwsh -File scripts/test-native.ps1
powershell -NoProfile -File tests/lifecycle-runtime.ps1 -HostName Word
powershell -NoProfile -File tests/lifecycle-runtime.ps1 -HostName Excel
powershell -NoProfile -File tests/lifecycle-runtime.ps1 -HostName PowerPoint
```

生命周期测试要求对应 Office 没有现有进程，再通过文件关联打开指定空白示例，并校验唯一文件、完整路径和已保存状态。测试生成的文字只写入自有临时文档，结束前仅丢弃这些测试编辑；失败保留窗口。不要在运行期间操作窗口。PowerPoint 的原生未保存提示先于 BeforeClose 事件出现，脚本的取消回归临时标记测试文稿为已保存，再恢复未保存状态；真实保存提示的按钮选择仍须人工验证。

PowerPoint 关闭请求在宿主消息循环内执行，关闭事件和后续枚举确认完成后才继续批量处理或退出。`Close` 对 PowerPoint 返回请求是否接受，不能直接作为关闭完成或用户取消的同步判断；用 `GetState` 的视图数及 `diagnostics.closePending` 等待完成。

`GetDiagnostics` 和安装器诊断采用字段白名单，不输出文档标题、路径、内容、账号、句柄和原始日志。`GetState` 是本地开发接口，仍包含标题与窗口布局信息，不能作为公开诊断上传。

## 提交与发布

修改源码后执行相应构建及运行验证；仅文档、安装器或打包变更不必重复所有 Office 流程。发布前检查源码、图片和 ZIP 不含个人路径、账号、日志或未审阅的截图。

手动创建版本标签，上传 `delivery/` 中 EXE、ZIP、SHA256SUMS；CI 仅生成构建产物，不自动发布。版本号当前同步维护于源码、安装脚本、安装器、打包默认值和 README。
