openOfficetab 0.1.3 增加朝内容区开口的选中标签、浅灰融合外观和单标签拖动调宽。

- 默认浅灰背景 `#EDEDED`，白色选中标签朝文档内容区开口；保留独立白色面板样式。
- 强调线可选顶部、底部或无，与两种整体样式自由组合；Word 蓝色、Excel 绿色、PowerPoint 橙红色。
- 拖动标签右边缘单独调整宽度（80–360 DIP）；按 Esc 或丢失鼠标捕获取消。统一设置宽度会清除单标签调整。
- 外观切换与宽度调整只重绘插件控件，不重建 Office 窗格。
- 设置入口：标签后的 `…` → **标签外观**。

x64 构建、安装包载荷校验和生产绘制 / 输入代码离屏检查通过；Word、PowerPoint 已验证普通文件启动加载与稳定布局。Excel 本机原有窗口保留在 0.1.2，尚未执行 0.1.3 宿主内交互验证；新版实际观感与真实鼠标操作仍需人工检查，详见 [验证记录](https://github.com/xlennart/openOfficetab/blob/v0.1.3/docs/validation.md)。公开截图保留已审阅的旧款独立面板。

安装：保存文档并退出 Word、Excel、PowerPoint，运行 `openOfficetab-0.1.3-setup-x64.exe`，点击“安装 / 更新”；或下载 ZIP 完整解压后运行 `Install.cmd`。安装按当前用户注册，无需管理员权限。安装器未做代码签名，附 `SHA256SUMS.txt` 供核对。
