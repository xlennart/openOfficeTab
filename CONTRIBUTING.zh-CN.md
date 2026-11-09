# 参与 openOfficeTab 开发

[English](CONTRIBUTING.md)

## 提交 PR

1. Fork 仓库，从当前 `main` 创建分支。每条 PR 聚焦一项改动，并列出有重叠或依赖关系的 PR。
2. 说明问题、用户能看到的变化和已执行的检查。如有相关 issue，请附链接。还在开发时可先开草稿 PR。
3. 测试和截图使用构造的 Office 示例。提交及公开附件应排除个人路径、文档、凭据、原始日志和本地工作区文件。

## 提供验证证据

- **原生代码**。运行 `pwsh -File scripts/test-native.ps1 -Architecture x64`，再以 `-Architecture x86` 复跑。[Build 工作流](.github/workflows/build.yml) 会构建两种 Office 载荷并检查统一安装包。
- **界面改动**。遵循[界面设计规范](design.md#简体中文)，附上受影响区域的前后截图。同步简体中文、英文、可访问名称和受影响的文档。检查跟随 Windows、键盘操作和适用的浅色、深色及高对比度状态。涉及窗口布局时，标明默认和最小尺寸及显示缩放比例。
- **Office 行为**。记录 Office 应用、版本和位数。涉及保存、重命名、覆盖或删除文件时，用可丢弃的示例覆盖成功、取消、失败和目标已存在的场景。说明未执行的检查，分开记录离线测试、预览和 Office 实测结果。

## 审阅与合并

- 维护者在合并前审查最新改动和对应 CI 结果。Fork 的工作流等待批准时，先检查提交代码及工作流变更，再批准运行。
- Office 行为改动由维护者在宿主机 x64 Office 和虚拟机 x86 Office 中验证受影响的路径，按适用范围覆盖 Word、Excel 和 PowerPoint。文档改动检查文字、链接及翻译。记录验证缺口，解决合并阻塞后再合并。
- 在审阅中约定重叠 PR 的合并顺序。后合并的 PR 同步到合并后的 `main`，并针对组合后的代码复跑受影响的检查。

构建与测试步骤见[开发说明](docs/development.md)和[实机测试流程](docs/runtime-test-workflow.md)。
