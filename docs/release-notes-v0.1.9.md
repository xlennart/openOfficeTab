## English

Fixes selecting cells from other workbooks while entering an Excel formula through the tab bar (#2).

- Preserve Excel's native formula-entry and reference-selection state when switching tabs.
- Avoid taking mouse capture or running ordinary COM activation / window coordination during editing; resume normal updates after Enter or Escape.
- Verify the generated external references and calculated values using real input, including the reported three-workbook `1 + 2 = 3` example.

The release DLL passed six Excel reference-entry cycles, cancellation preserving the original cell value, three normal tab-handler checks, a subsequent `+` new-document check, and 22 actual ribbon-content checks. Native regression and installer checks, plus remaining limits, are documented in the [validation record](https://github.com/xlennart/openOfficetab/blob/v0.1.9/docs/validation.md).

Download the x64 EXE installer or ZIP below. Save your work and fully exit Office before upgrading. Settings are preserved. The installer is unsigned; checksums are provided in `SHA256SUMS.txt`.

## 简体中文

修复 Excel 输入公式时，通过标签切换到其他工作簿选取单元格引用失败的问题（#2）。

- 切换标签时保留 Excel 原生的公式输入与引用选取状态。
- 编辑期间不夺取鼠标捕获，不执行普通 COM 激活和窗口协调；回车提交或 Esc 取消后恢复正常更新。
- 使用真实输入核对生成的外部引用与数值，覆盖反馈中的三个工作簿 `1 + 2 = 3` 场景。

发布 DLL 通过六轮 Excel 跨工作簿引用输入、取消后保留原值、三次普通标签消息处理检查、随后通过“＋”新建的检查，共核对 22 次实际功能区内容。原生回归、安装包校验及验证边界见 [验证记录](https://github.com/xlennart/openOfficetab/blob/v0.1.9/docs/validation.md)。

下载下方 x64 EXE 安装器或 ZIP 包，升级前请保存文件并完全退出 Office。保留原有设置；安装器未签名，附 `SHA256SUMS.txt` 校验值。
