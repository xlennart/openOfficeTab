# 0.2.3 本地复核 / Local acceptance

本指南用于本版验收。示例使用独立副本，可修改或重命名。 / Acceptance guide for this version. Samples are separate copies and may be edited or renamed.

## 简体中文

1. **标签与菜单**：Word、Excel、PowerPoint 各已打开 A/B 两份文件。点击 A/B 标签切换，右键标签及点击标签栏的“…”；检查图标、间距、级联菜单和文字是否协调。
2. **关闭对话框**：同组有两份文件时，点击 Office 窗口右上角 ×。检查简化后的文档图标与圆角复选框，勾选再取消勾选，按 Esc 或点“取消”，两份文件都应保留。选择“关闭当前”只应关当前一份；“关闭本组全部”会关本组两份。未保存更改仍由 Office 询问。
3. **记住选择**：勾选“记住我的选择”后选“关闭当前”；下一次窗口关闭应沿用选择。可从“…”→“点击窗口关闭按钮时”恢复“每次询问”。
4. **重命名文件**：在任意 A 文件中输入一小段测试内容，右键该标签→“重命名文件”，输入新名称（不用输入扩展名）。确认后应以新名字重开，内容保留、旧名字消失。撤销历史会重置，对话框会先提示。另试“取消”和已存在的 B 文件名；取消应保留原文件，重名不能覆盖 B。
5. **宽度、颜色和语言**：“…”→“默认标签宽度…”及“标签外观”中打开颜色入口，检查新对话框后取消；在“语言 / Language”切到 English，检查菜单、关闭对话框与管理中心，再恢复原语言。
6. **赞助入口**：管理中心→“关于与更新”→“爱发电赞助”，应打开完整爱发电地址。OSS 为下载最后兜底，只有其他线路不可用时才使用，不必反复检查更新来测试它。

先看 2、4、6 即可覆盖本版最明显的变化。反馈时说明应用、步骤及实际结果，有画面问题可附截图。样式调整没有重跑全套实机回归；完整验证状态见[验证记录](validation-v0.2.3.md)。

## English

1. **Tabs and menus**: two A/B copies are open in each Office host. Switch tabs, right-click a tab and open **…**. Review icons, spacing, cascades and text.
2. **Close dialog**: with two documents grouped, click the Office window's ×. Review the document glyph and rounded checkbox. Toggle it, then press Esc or **Cancel**; both files should remain open. **Close current** closes one; **Close group** closes both. Office still asks about unsaved changes.
3. **Remember choice**: check the option and choose **Close current**. The next window close should reuse the choice. Restore **Ask every time** under **… → When clicking the window close button**.
4. **Rename file**: add test content to an A file. Right-click its tab, choose **Rename file…**, and enter a new stem without the extension. The file should reopen with its new name and preserved content; the old name should disappear. The dialog warns that undo history resets. Also try cancelling and an existing B filename; cancellation preserves the original and duplicates cannot overwrite B.
5. **Appearance and language**: open **Default tab width…** and the color dialog under **Tab appearance**, review and cancel. Switch to English under **Language**, inspect native menus/dialogs and the center, then restore the original language.
6. **Sponsorship**: in **About & updates**, choose **Sponsor on Afdian**. It should open the complete sharing URL. OSS is the final download fallback; it is used only when other routes fail, so repeated update checks are unnecessary.

Start with 2, 4 and 6 for the most visible changes. Report the host, steps and observed behavior, with screenshots for visual issues. The cosmetic change did not trigger another full runtime matrix; see the [validation record](validation-v0.2.3.md).

