# 32 位 Office 虚拟机回归 / x86 Office VM regression

## 简体中文

### 固定入口

`scripts/test-office-x86-vm.ps1` 是宿主机入口，`tests/vm/office-runtime-job.ps1` 是客机串行执行器。默认使用项目约定的 VMware 虚拟机；`VMX`、`Vmrun` 和 `CredentialPath` 可显式覆盖。凭据独立保存在桌面用户的 `%USERPROFILE%\AppData\Local\openOfficetab.Development\office-x86.credential.xml`，由当前用户的 Windows DPAPI 加密，目录仅允许该用户和 SYSTEM 访问，不随构建目录清理，不进入 Git、日志、请求清单或发行包。

本机凭据尚未初始化时，只需在真实桌面用户的 PowerShell 中执行一次：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/set-office-vm-credential.ps1
```

用户在系统凭据提示中输入测试虚拟机账号。调用本流程时不提供明文密码参数，不得自动从历史聊天恢复密码。已有加密记录可通过 `-FromCredentialFile` 导入；另一个 Windows 用户或机器需要重新初始化。VMware 的 `vmrun` 接口仍需要在短暂的子进程参数中接收凭据；流程不保存或回显该完整命令行，错误输出会脱敏。

### 每轮执行

先完成两架构构建、管理中心和统一包，使用唯一 jobId。预检可以与随后 Run 使用同一个 jobId；一旦有运行结果，重测必须使用新 jobId。

```powershell
$job = 'beta2-x86-20261031-01'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-office-x86-vm.ps1 -Action Preflight -JobId $job
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-office-x86-vm.ps1 -Action Run -JobId $job -DeliveryDirectory delivery/beta-0.2.3-beta.2
```

预检核对 VMware Tools、已解锁的交互桌面、session、DPI、现有 Office/管理中心进程及睡眠策略。开机遇到 Windows 更新时保存启动画面并停止测试，等待更新自然完成；登录或解锁由用户手动完成。Tools 成功、API 能连接或旧结果文件均不算 Office 验收。不能强制重置虚拟机。

Run 会验证 EXE、ZIP、QA 包及两种 DLL 哈希，按实际 Office PE 位数选择 x86，验证统一安装、三宿主生命周期、真实保存及进程退出后的 XML 内容、中英文及自动语言、管理中心入口与退出，以及修复、保留设置卸载和重装。运行由固定执行器串行处理并持有互斥锁；唤醒请求覆盖整个任务，在 finally 中释放，不改写电源计划。

宿主机负责同一 x64 管理中心的完整 UI/尺寸矩阵。客机入口检查排除 Tauri 的隐藏调整窗口，不冒充完整界面回归；COM/原生消息测试不证明人工鼠标拖动手感，未保存取消事件也不代表每个保存对话框按钮。

### 结果、复测和收尾

本机证据位于 `artifacts/runtime-vm/<jobId>/`。`request.json` 绑定版本及输入哈希；`result.json` 列出每阶段结果、PID、启动时间和退出码；`evidence-index.json` 为非空结果文件列出长度与 SHA-256。宿主机逐文件取回并验证，不依赖容易失败的客机打包，也不尝试下载空文件。

```powershell
# 取回中断任务已完成的结果，不重新执行测试
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-office-x86-vm.ps1 -Action Collect -JobId $job

# 仅恢复该轮明确拥有且未编辑的公开测试文件，原失败结果不改为通过
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-office-x86-vm.ps1 -Action RecoverFixtures -JobId $job

# 对已经收集且没有 Office/管理中心残留的任务清理临时副本
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-office-x86-vm.ps1 -Action Cleanup -JobId $job

# 仅正常关机；存在 Office/管理中心进程时拒绝
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-office-x86-vm.ps1 -Action Shutdown -JobId $job
```

Run 通过后自动取回证据、清理本轮客机目录并正常关机；使用 `-KeepVMRunning` 可暂留客机供连续任务使用。失败时保留现场，先 Collect，再检查本轮所有权明确的进程和文件；不能把关闭窗口等同于进程退出，也不能为维护而结束未知进程。保留固定执行器和本机受保护凭据，只删除本轮临时目录。

### 已固化的故障处理

| 现象 | 处理 |
| --- | --- |
| 构建清理后找不到旧凭据 | 使用用户开发目录的持久加密记录，预检先验证；不在 `build/credentials` 寻找长期入口。 |
| Windows 更新或登录界面 | 阻塞 GUI 测试；等待更新、用户登录后重新 Preflight，不盲发按键。 |
| ZIP 没有保留空目录 | 执行器解压后明确创建 `package/artifacts`。 |
| VMware 拒绝覆盖已有文件 | 固定执行器先核对归属和哈希；任务输入仅替换本轮路径，下载使用唯一临时文件再原子替换。 |
| 文件不存在返回非零退出码 | 将明确的不存在视为预检状态，区分真实传输失败；同一接口失败最多两次。 |
| 保存测试文件正常锁定 | Office 退出后再核对哈希和 XML；PID 必须同时匹配启动时间。 |
| 重复读取到旧结果 | 唯一 jobId、请求/结果身份匹配、输入与证据哈希；不覆盖已有运行证据。 |

## English

### Stable entry points

Use `scripts/test-office-x86-vm.ps1` on the host and `tests/vm/office-runtime-job.ps1` as the fixed serial guest executor. The default targets the project's VMware x86 Office VM; `VMX`, `Vmrun` and `CredentialPath` can be overridden. Credentials live outside the repository/build tree at `%USERPROFILE%\AppData\Local\openOfficetab.Development\office-x86.credential.xml`, protected by the desktop user's DPAPI and an ACL for that user and SYSTEM. They never enter logs, manifests, Git or release packages.

Run `scripts/set-office-vm-credential.ps1` once from the real desktop user's PowerShell and enter the VM account in the credential prompt. An existing encrypted record can be imported with `-FromCredentialFile`. Do not pass plaintext passwords to these scripts or automatically recover them from chat history. Another user/machine must initialize its own record. VMware's `vmrun` interface still receives credentials in a short-lived child-process argument; the workflow never saves/echoes that complete command line and redacts errors.

### Run and coverage

Build/package both Office architectures first. Use the commands above with a unique jobId; Preflight and its following Run may share an ID, but retests need a fresh one after runtime evidence exists.

Preflight checks VMware Tools, the unlocked interactive desktop, session, DPI, existing Office/center processes and sleep policy. A Windows-update screen blocks testing until completion; the user must log in/unlock manually. Tools connectivity and old result files are not Office acceptance. Never force-reset the VM.

Run binds hashes for the unified EXE, ZIP, QA overlay and both DLLs, checks actual Office PE architecture, and covers unified installation, all three Office hosts, lifecycle, native save plus post-exit XML verification, Chinese/English/Follow Windows, center entry/exit, repair, settings-preserving uninstall and reinstall. A mutex serializes guest jobs. A job-scoped wake request is released in finally without changing the power plan.

Run the full shared x64 center UI/size matrix on the host. Guest center entry checks exclude Tauri's hidden resize window. API/native-message tests do not establish physical drag smoothness or every save-dialog button.

### Evidence and cleanup

Evidence is stored at `artifacts/runtime-vm/<jobId>/`. Requests bind release and input identities; results retain stage outcomes, PIDs, start times and exit codes. The nonempty-file evidence index supplies lengths and SHA-256 values verified after download. Collection avoids guest archive creation and empty-file transfers.

Use Collect to retrieve completed interrupted work without rerunning it. RecoverFixtures closes only this job's unique, saved public fixtures and preserves the failed result. On success, Run collects and verifies evidence, removes only its guest run directory and requests normal shutdown; `-KeepVMRunning` leaves the VM available. Failure retains the scene. Collect first, inspect owned processes/files, then Cleanup and Shutdown when safe. Window closure and process exit are separate checks. Never terminate unknown processes. Preserve the fixed executor and protected credential record.

The reusable workflow explicitly creates output directories after extraction, handles VMware's no-overwrite behavior and negative existence status, uses atomic host downloads, rejects stale job identities, and binds process checks to both PID and start time. Retry the same failing interface at most twice, preserve the failure, and choose a supported driver or report a real blocker.
