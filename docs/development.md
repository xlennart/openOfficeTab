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
| `src/window-coordinator.cpp` | 同组窗口消息观察、延迟协调、临时遮蔽及原生关闭选择 |
| `src/theme.cpp`、`src/theme.h` | Office / Windows 主题解析与调色板 |
| `tests/frame-close-test.cpp` | 关闭选择、记忆和原生关闭分流回归 |
| `src/tabs.cpp`、`src/tabs.h` | ActiveX 占位、原生标签栏绘制与输入 |
| `src/model.h`、`src/com.h` | 状态模型、COM 接口与辅助对象 |
| `src/features.cpp`、`src/layout.h` | 诊断、宽度设置、组移动和纯宽度计算 |
| `scripts/` | 构建、安装、卸载、打包和桌面上下文桥接 |
| `installer/Setup.cs` | 按用户安装、卸载、加载诊断与修复的 WinForms 引导程序 |
| `tests/fixtures/` | 清除个人元数据与绝对路径的空白 Office 示例 |
| `tests/appearance-test.cpp` | 无 Office、无可见窗口的生产 GDI 绘制与标签边缘输入回归 |
| `src/window-layout.h`、`tests/window-state-test.cpp` | 实际屏幕矩形、恢复保护、窗格归属及 OLE 界面所有权 / 控件销毁回归 |

每个 Office 编辑窗口拥有自己的 CTP 占位与标签渲染窗口。插件只调整自有控件对应的窗格标题框；文档仍由 Office 管理。新建使用 `FileNewDefault`，Word 窗口激活事件提前初始化，周期刷新补齐遗漏事件。所有 Office COM 访问都保持在宿主 STA 内。

OLE 占位声明 `OLEMISC_NOUIACTIVATE`，不请求替换 Office 的菜单或工具栏；独立原生标签子窗口仍处理键盘输入。窗格压缩验证自身 OLE 祖先、宿主根窗口与窄条高度；不按通用类名隐藏其他 Office 工具栏。窗口消息回调在控件析构前解绑，避免最后一次 Release 后的 DestroyWindow 消息再次释放对象。

接口约定参考微软的 [OnUIActivate 说明](https://learn.microsoft.com/en-us/windows/win32/api/oleidl/nf-oleidl-ioleinplacesite-onuiactivate) 与 [WINDOWPLACEMENT 坐标说明](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement)。前者会要求容器让出原有文档界面，后者区分工作区还原坐标与屏幕坐标。

同组标签切换使用点击来源的当前屏幕矩形，区分 WINDOWPLACEMENT 的还原矩形；最大化仍使用原生放置状态。只有目标仍保持插件应用的布局时才恢复原布局，保护后续手动调整。单任务栏设置保持可选，三宿主的标签尺寸跟随不依赖此设置。

## 验证

```powershell
python tests/check-fixtures.py
powershell -NoProfile -File tests/desktop-install-runtime.ps1 -OpenFixtures
powershell -NoProfile -File tests/new-document-runtime.ps1
powershell -NoProfile -File tests/independent-window-runtime.ps1
```

先安装插件，再在普通桌面 PowerShell 环境运行 Office 测试。运行测试期间请不要操作示例窗口。新建与独立窗口脚本要求该实例只有一个指定示例文件，否则停止，以保留其他文档。它们仅关闭自己创建的测试文档；失败时保留需要检查的窗口。测试输出放在 `artifacts/`，可能包含文档标题、窗口句柄及本机路径，不应直接公开上传。

测试脚本只从运行对象表附着已有示例；不会为了找不到文件的情况创建新的 Office 自动化实例。

CI 检查示例文件、编译源码、原生绘制 / 输入 / 窗口状态回归及安装包载荷，不包含 Office UI 或实机测试。[office-events.csv](office-events.csv) 是开发期核对的三宿主事件声明；[manual-test-plan.csv](manual-test-plan.csv) 是更广的人工测试计划，标注“未执行”的项不能视作通过。

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

0.1.4 的原生测试额外覆盖 OLE 界面所有权、控件销毁、窄条归属、当前矩形跟随与恢复保护。以下实机脚本只使用指定空白示例：

```powershell
powershell -NoProfile -File tests/word-window-runtime.ps1
powershell -NoProfile -File tests/ribbon-runtime.ps1 -HostName Excel
powershell -NoProfile -File tests/ribbon-runtime.ps1 -HostName PowerPoint
```

默认要求对应宿主没有既有进程。`ribbon-runtime.ps1 -UseExistingFixture` 只接受唯一、已保存、路径匹配的项目示例；两类脚本都保留最后两个空白窗口供人工检查，仅关闭自己刚创建且确认空白的中间文档，不退出整个应用。它们用原生窗口接口设置半屏尺寸，不模拟 Windows 边缘吸附或鼠标手感。

## 提交与发布

修改源码后执行相应构建及运行验证；仅文档、安装器或打包变更不必重复所有 Office 流程。发布前检查源码、图片和 ZIP 不含个人路径、账号、日志或未审阅的截图。

手动创建版本标签，上传 `delivery/` 中 EXE、ZIP、SHA256SUMS；CI 仅生成构建产物，不自动发布。版本号当前同步维护于源码、安装脚本、安装器、打包默认值和 README。

## 0.1.5 存储与同步

`src/storage.cpp` 提供带命名互斥的读改写、UTF-16 原子替换和文件偏好 / 最近关闭的 schema；`src/settings.cpp` 按加载基线合并变化键；`src/persistence.cpp` 负责确认关闭记录、重开、首次保存继承和延迟写入重试。测试可注入存储目录，不接触用户配置。`documents.ini` 含路径历史，只编码不加密，禁止提交到仓库或附在公共诊断中。

`GetRecentlyClosed` 开发接口返回本机路径，不能用于公开诊断。`GetDiagnostics` 只返回计数和设置写入状态。`ReopenClosed` 索引从 0 开始；菜单捕获路径快照，避免列表并发更新后选错文件。

原生测试新增两个隐藏子进程竞争写入、陈旧设置增量合并、取消关闭 / 隐藏视图排除、重开偏好和首次保存继承。功能区边缘补绘只在验证自有占位和完整 Ribbon 后启用，3 DIP、高对比度关闭，命中穿透，卸载销毁。

0.1.5 实机回归：依次运行 `persistence-runtime.ps1 -HostName Word|Excel|PowerPoint`（每次填一个宿主名），再运行 `first-save-runtime.ps1` 的同名宿主检查，最后运行 `settings-sync-runtime.ps1`。只接受唯一已保存的公开空白示例；保存测试仅写入 `artifacts/` 中唯一命名的空白副本，不退出含私人文档的宿主。实机回归不会由 CI 执行。

## 0.1.6 窗口协调与主题

窗口消息回调只记录意图，几何同步在宿主空闲时合并执行；移动和尺寸循环期间暂停完整 COM 枚举，避免布局重入。操作开始前对同组后台窗口临时应用可恢复的透明遮蔽，结束后先同步几何及层级，再恢复原有分层属性。文档顶层窗口不做 SetParent，也不重新打开文件。

新增开发接口 `SetThemeMode`（DISPID 42）和 `SetFrameCloseAction`（DISPID 43），保留既有接口编号。单任务栏、主题和关闭偏好按键增量合并并跨实例同步。原生测试由 `scripts/test-native.ps1` 统一运行；实机消息回归见 `tests/window-mask-runtime.ps1`，只接受指定空白示例，输出留在忽略的 `artifacts/`。运行边界及人工验收见 [验证记录](validation.md)。

## 0.1.7 中英文界面

`src/localization.h` 集中维护中英文 UI 词条。设置 `language` 的取值为 0 跟随 Windows、1 简体中文、2 英文；缺省或损坏值回到 0。自动模式使用 [GetUserDefaultUILanguage](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getuserdefaultuilanguage)，中文主语言 ID 使用简体中文，其余回退英文。它不读取地区格式，也不改动 Office 或系统语言。

新增 `SetLanguage`（DISPID 44），既有编号保留；`GetLabel`（45）和 `RibbonLoaded`（46）服务 Ribbon 回调。语言更新通过 [IRibbonUI.Invalidate](https://learn.microsoft.com/en-us/office/vba/api/office.iribbonui.invalidate) 刷新已有功能区缓存，不重建文档窗格。语言同其他全局偏好按键合并并同步，临时失败下次空闲重试。文档名、路径、内容及原生 Office 保存流程不做翻译。

安装器的语言选择独立于插件设置；`scripts/localization.ps1` 提供安装 / 卸载 / 诊断输出的选择，`-Language auto|zh-CN|en` 会通过桌面桥接显式传递。Windows PowerShell 5.1 执行的含中文脚本使用 UTF-8 BOM。安装包附英文 `README.md` 与中文 `README.zh-CN.md`。

运行 `python tests/check-localization.py` 和 `scripts/test-native.ps1` 检查词条、动态标签和持久化。打包后运行 `tests/installer-localization-test.ps1`，不执行安装操作。`tests/language-runtime.ps1` 只附着三个唯一且已保存的公开空白示例，验证实际载荷、三模式同步、回调和窗口稳定性，最后恢复原设置；运行输出禁止直接公开上传。
