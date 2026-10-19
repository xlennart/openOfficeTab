## English

Fixes a new-document initialization path that could leave Word's ribbon and title-bar controls blank after clicking the tab bar's **+** button.

- Queue the native new-document command after the tab's mouse message returns, preserving the intended tab group or independent-window choice.
- Defer Word pane creation and window coordination until activation callbacks return and expanded ribbon content is ready. Collapsed ribbons are not forced open.
- Check the actual visible NetUI content, rather than treating a visible ribbon frame as proof that its controls are present.

The functional fix passed 24 Word cycles and 12 cycles each in Excel and PowerPoint. The release rebuild updates version strings to 0.1.8; these runtime checks used the same functional changes before that version-only rebuild. Native regression tests and installer payload checks are also recorded in the [validation record](https://github.com/xlennart/openOfficetab/blob/v0.1.8/docs/validation.md).

**Known limit:** PowerPoint crashed once during test teardown when closing an owned test presentation after disconnecting the add-in. The cause remains unconfirmed. No further crash was recorded during the subsequent 12-cycle saved-test-document run; complete unload/exit behavior is not claimed as verified. The PowerPoint new-document test saved only verified blank test presentations to its own test directory before closing them, so it does not validate unsaved-close prompts or cancellation.

Download the x64 EXE installer or ZIP below. Save your work and fully exit Word, Excel, and PowerPoint before installing. Installation is per user; settings are preserved. The installer is unsigned; verify it with `SHA256SUMS.txt`.

## 简体中文

修复点击标签栏 **＋** 新建文档后，Word 功能区和标题栏控件可能整片空白的初始化路径。

- 新建原生命令进入消息队列，在标签鼠标消息返回后执行，保留来源标签组及独立窗口选择。
- Word 激活回调结束、展开的功能区内容就绪后，再创建窗格和协调窗口；不会强制展开折叠的功能区。
- 回归检查改为核对实际可见的 NetUI 内容，不再仅凭功能区外框可见就判定通过。

功能修改版本通过 Word 24 轮、Excel 与 PowerPoint 各 12 轮新建及切换检查。发布重建仅将版本标识更新为 0.1.8；上述实机检查使用的是改号前、功能修改相同的载荷。原生回归与安装器载荷校验详见 [验证记录](https://github.com/xlennart/openOfficetab/blob/v0.1.8/docs/validation.md)。

**已知边界：** PowerPoint 测试收尾在断开加载项后关闭自有测试文稿时曾崩溃一次，原因尚未确认。后续 12 轮已保存测试文稿检查没有新增崩溃记录，不代表完整卸载、退出路径均已验证。PowerPoint 新建测试仅将确认无内容的新文稿保存到自有测试目录再关闭，未验收未保存关闭提示或取消分支。

下载下方 x64 EXE 安装器或 ZIP 包。安装前保存文件并完全退出三款 Office。按当前用户安装，保留原有设置；安装器未签名，附 `SHA256SUMS.txt` 校验值。
