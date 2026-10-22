# Excel 跨工作簿公式引用

## 场景

[Issue #2](https://github.com/xlennart/openOfficetab/issues/2#issuecomment-6346200616) 描述：同时打开三个工作簿，在第三个工作簿输入公式，通过标签依次选取前两个工作簿的 A1 相加。Excel 原生支持这一操作；普通浏览时能够切换标签，并不能证明公式选取也兼容。

原实现统一调用 `Window.Activate`，随后执行对齐、原生激活和刷新。标签鼠标按下还会捕获鼠标。公式输入期间，Excel 保留一个公式编辑所有者，目标工作簿作为引用来源显示；此时普通对象模型激活、捕获输入或再强制协调焦点，与 Excel 原生窗口切换的行为不同。

调查最初怀疑对象模型调用被拒绝。外部自动化在编辑状态下确实出现过 `RPC_E_CALL_REJECTED`，但这不能单独证明同线程插件的 `Window.Activate` 也被拒绝。因此修复不依赖重试该调用，而是保留 Excel 的原生编辑 / 引用选取流程。

## 实现

- 通过 Excel SDK 声明的 `LPenHelper(xlGetFmlaInfo)` 只读取当前模式：Ready / Enter / Edit / Point。只在目标 Excel 窗口所属 UI 线程调用；不读写公式文本，不在线程池轮询。
- 回调库从 Excel 可执行文件所在目录解析，不搜索项目目录，不捆绑额外 XLL。SDK ABI 来源为微软公开的 [xlcall.h](https://github.com/microsoftarchive/msdn-code-gallery-microsoft/blob/master/Microsoft%20Office%20Developer%20Documentation%20Team/Excel%202010%20Writing%20Asynchronous%20User-Defined%20Functions/%5BC%2B%2B%5D-Excel%202010%20Writing%20Asynchronous%20User-Defined%20Functions/C%2B%2B/xlcall.h)。模式查询不可用时返回未知，不把未知伪装成 Ready。
- 编辑期间标签不请求焦点或鼠标捕获，使用目标原生窗口的前台激活，让 Excel 自己维护公式所有者、引用选取和键盘输入。不模拟回车或退出编辑来完成切换。
- 暂停完整枚举、窗格更新和窗口布局协调；已有标签继续绘制。提交或取消后，正常的事件 / 空闲刷新恢复同步。`Ready` 查询失败时也保留现有视图。
- 本次改动以 Excel 宿主为边界，Word、PowerPoint 不进入公式编辑路径。

## 验收方法

`tests/excel-reference-runtime.ps1` 只使用三个自有测试文件，发送真实输入点击标签及单元格，并检查生成的两个外部引用和数值结果；不是通过 COM 直接写入目标公式来代替交互。还覆盖取消输入、普通切换和之后通过“＋”新建的可见功能区内容。运行记录留在忽略的 `artifacts/`，不上传文档标题、路径和原始日志。

窗口消息定向发送不足以完整模拟 Excel 的鼠标按键状态与编辑消息循环，因此诊断阶段的此类实验不作为交互通过的证据。正式结果与载荷校验见 [验证记录](validation.md)。
