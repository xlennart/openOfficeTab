# 32/64 位 Office：0.2.2

[English](office-x86.en.md)

0.2.2 面向 **64 位 Windows 上的 32 位 Office**，同时保留 64 位 Office 支持。管理中心和 EXE 仍为 x64，安装器包含 x86、x64 两套原生 DLL，自动选择与 Office 位数一致的载荷。32 位 Windows 和原生 ARM64 Office 不在本阶段支持范围内。

Word、Excel、PowerPoint 均有两种架构的真实桌面验证。实际范围、载荷身份与未覆盖边界见 [版本验证记录](validation-v0.2.2.md)；离线检查单独列示。

32 位测试使用 VMware 虚拟机：Windows 11 企业版 LTSC 26100（64 位），Microsoft 365 Apps 16.0.20527.20112（32 位）。64 位 Office 在宿主机测试。此环境不覆盖所有 Office 版本；如有问题，请按故障报告模板提供复现步骤、Office 版本与位数、导出的脱敏诊断日志，必要时附视频。

## 架构选择规则

- 根据 Click-to-Run、MSI 安装根及 App Paths，在相应注册表视图中定位现存 Word、Excel、PowerPoint，读取程序 PE 头确认位数。
- 不根据 Windows 位数或目录名称推断 Office 位数。缺失、无法读取、不支持或相互冲突的证据均阻止安装。
- `Install.cmd` 默认自动选择；显式传入 `install.ps1 -Architecture x86` 或 `x64` 仍须匹配实际 Office。
- 使用对应注册工具和 COM 注册表视图。安装后的管理中心保留两套载荷，支持后续修复及 Office 位数迁移。
- 保留共享设置、各宿主启停偏好和主动选择的更新策略。维护前需退出 Office，安装器不会关闭文档。
- 现有签名更新协议及 x64 EXE 名称描述 Windows 安装器，旧 x64 客户端仍可接收统一安装器；RSA 验签、安装包哈希及受限 HTTPS 路由保持有效。

## 构建及离线验证

```powershell
./scripts/build.ps1 -Architecture x86
./scripts/build.ps1 -Architecture x64
./scripts/test-native.ps1 -Architecture x86
./scripts/test-native.ps1 -Architecture x64
./tests/native-payload-test.ps1
./scripts/build-desktop.ps1 -SkipNative
powershell.exe -NoProfile -File ./tests/desktop-service-test.ps1
./scripts/package.ps1 -SkipBuild -OutputDirectory ./artifacts/candidate-v0.2.2
./tests/desktop-package-test.ps1 -DeliveryDirectory ./artifacts/candidate-v0.2.2
```

DLL 校验器分别运行 x86、x64 进程，加载对应 DLL，检查五个无修饰 COM 导出，并调用类工厂与卸载寿命检查；不会注册加载项或启动 Office。`--verify` 保留原有 x64 哈希输出；`--verify-payloads <report.json>` 核对两套内嵌 DLL 身份。

原生日志的数值 `bits` 表示发出记录的进程位数，服务日志表示服务自身位数；导出诊断另行报告检测到的 Office 架构。这些信息不能替代实际加载 DLL 的验收。

## 实机验证与后续更新

32 位 Office 在 VMware 桌面测试，64 位在宿主机测试。0.2.2 记录覆盖对应注册与实际加载 DLL 哈希、标签生命周期、分组与跨窗拖放命令、原生取消事件、布局恢复、语言同步、脱敏日志、更新弹窗确定/×及维护往返。x86 客机另测了大体量 Word、Excel 样本。

拖放命令通过不表示人工拖动流畅度通过；取消事件不替代全部真实保存提示按钮组合。Office 位数迁移、32 位 Windows、ARM64 及其他 Office 版本未全部实测。

后续更新同时构建两种 DLL，统一安装器保留双载荷，并重复两端桌面验证。虚拟机测试完成后正常关机，再同步 GitHub/CNB 的同一源码提交、标签和附件，核对匿名下载、哈希与更新签名。保留可复用的大文件样本，清理临时副本。
