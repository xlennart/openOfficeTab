# openOfficeTab 界面设计 / Interface design

[简体中文](#简体中文) · [English](#english)

## 简体中文

### 设计基准

| 界面 | 对照基准 |
| --- | --- |
| Office 标签栏、菜单和弹窗 | 以当前 Word、Excel 和 PowerPoint 窗口中的界面为准，沿用融合开口与独立面板两种标签样式。配色和主题见 [标签绘制](src/tabs.cpp)和[主题解析](src/theme.h)。 |
| 管理中心和安装器 | [已确认的样式](desktop/src/design.css)定义品牌色、字号、圆角和页面层级，[桌面适配](desktop/src/shell.css)负责窗口控件、边界和窄屏布局。调整设计基准需经维护者审阅，并更新[来源记录](desktop/design-provenance.json)。 |
| 更新窗口 | 延续管理中心的视觉层级，按检查、下载和安装阶段显示状态。正文独立滚动，主要操作和关闭入口按状态保留。见[更新窗口样式](desktop/src/update.css)。 |

### 布局与交互

- 用标题、间距、分组和清楚的按钮文案建立层级。每个状态突出一个主要操作。字段名称与取值范围各显示一次，补充说明优先放在可用键盘访问的信息提示里。
- Office 内沿用 Word 蓝、Excel 绿、PowerPoint 橙作为宿主强调色。浅色与深色随现有主题偏好切换，高对比度采用系统颜色。勾选、禁用、焦点与危险状态同时用文字、图标或轮廓表达。
- 右键菜单保留原生级联、勾选与禁用状态。常用命令放在一级菜单，相关的移动或批量操作归入子菜单。自绘控件须保留可访问名称、键盘导航和高对比度支持。
- 弹窗采用一致的标题、内容和底部操作区。关闭文档、覆盖或删除文件等操作写明后果，保留安全的默认选项和取消路径。默认焦点、Enter 和 Esc 的结果应与按钮语义一致。
- 沿用共享语言偏好，支持跟随 Windows、简体中文和 English。同步控件名称、动态状态和错误文案。文档名、路径及内容按原样显示。文档中的操作步骤使用对应语言的实际控件名称。

### 界面验收

- PR 作者优先在自己可用的环境中亲自验证一次受影响界面，附实际结果和前后对照，说明未覆盖项。缺少环境时与维护者约定补测；完整实机矩阵可由维护者承担。最终记录标注测试提交、程序及载荷哈希、应用、语言、主题、窗口尺寸和显示缩放，原生菜单和弹窗在实际 Office 中检查相关宿主及键盘操作。
- 以当前配置的默认和最小尺寸验收管理中心与更新窗口。当前管理中心为 920 × 700 和 420 × 480，更新窗口为 560 × 530 和 360 × 400，均为逻辑像素。检查换行、边界、窗口按钮与底部操作区。
- 分开记录离线检查、预览和实机证据。AI 生成的说明、模拟测试和离屏预览不能替代实际程序验收。缺少完整环境不自动要求草稿；代码审查及必要验收通过后再合并、发版。见[贡献指南](CONTRIBUTING.zh-CN.md)。

## English

### Design references

| Surface | Reference |
| --- | --- |
| Office tabs, menus, and dialogs | Use the current UI inside Word, Excel, and PowerPoint as the baseline. Carry forward the blended open-edge and separate panel tab styles. See [tab rendering](src/tabs.cpp) and [theme resolution](src/theme.h) for colors and theme behavior. |
| Management center and installer | The [approved styles](desktop/src/design.css) define the palette, typography, corner radii, and hierarchy. The [desktop shell](desktop/src/shell.css) adapts window controls, edges, and narrow layouts. Have a maintainer review baseline changes and update the [provenance record](desktop/design-provenance.json). |
| Update window | Follow the management center's hierarchy and show the current check, download, or install state. Let the body scroll independently and keep the appropriate main action and close control available for each state. See the [update window styles](desktop/src/update.css). |

### Layout and interaction

- Use headings, spacing, grouping, and clear action labels to establish hierarchy. Emphasize one primary action per state. Show each field label and allowed range once. Put supplementary guidance in a keyboard-accessible info tip where possible.
- In Office, use Word blue, Excel green, and PowerPoint orange as host accents. Respect the existing light and dark theme preference and use system colors in high contrast. Pair color with text, icons, or outlines to convey checked, disabled, focused, and destructive states.
- Preserve native cascading menus, check marks, and disabled states. Keep frequent commands at the top level and group related move or batch commands in submenus. Custom-drawn controls must retain accessible names, keyboard navigation, and high-contrast support.
- Use a consistent heading, body, and action area in dialogs. Explain the consequences of closing documents, overwriting, or deleting files, and provide a safe default and cancellation path. Align default focus, Enter, and Esc with the action labels.
- Use the shared language preference for Follow Windows, Simplified Chinese, and English. Localize control names, dynamic states, and errors. Display document names, paths, and content as provided. Use the actual localized control names in instructions.

### UI acceptance

- Authors should first personally check affected surfaces in an available environment, attach actual results and before/after captures, and explain gaps. Agree on maintainer testing when environments are unavailable; maintainers may provide the full runtime matrix. Final records identify the tested commit, program and payload hashes, app, language, theme, window size, and display scale. Check native menus and dialogs inside Office, covering relevant hosts and keyboard interaction.
- Check the management center and update window at the configured default and minimum sizes. The current management center uses 920 × 700 and 420 × 480, and the update window uses 560 × 530 and 360 × 400, all in logical pixels. Check wrapping, control bounds, window buttons, and the bottom action area.
- Report offline checks, previews, and real runtime evidence separately. AI-generated descriptions, mocks, and off-screen previews do not replace actual application acceptance. Missing the full environment alone does not require draft status; merge and release after code review and required acceptance. See the [contribution guide](CONTRIBUTING.md).
