param([ValidateSet('x64','x86')][string]$Architecture='x64',[switch]$AllowSideBySideWhileRunning,[ValidateSet('auto','zh-CN','en')][string]$Language='auto')
$ErrorActionPreference='Stop'
if($env:OPENOFFICETAB_SETUP_UTF8){[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false);$OutputEncoding=[Console]::OutputEncoding}
. (Join-Path $PSScriptRoot 'localization.ps1')
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Language=$Language;Architecture=$Architecture;AllowSideBySideWhileRunning=[bool]$AllowSideBySideWhileRunning}){return}
$taskRoot=Split-Path $PSScriptRoot -Parent
$running=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue)
if($running.Count){
    if(-not $AllowSideBySideWhileRunning){throw (Get-OotText '请先保存文档并退出 Word、Excel、PowerPoint，再运行安装。安装不会关闭你的 Office 窗口。' 'Save your work and fully exit Word, Excel, and PowerPoint before installing. The installer will not close Office windows.')}
}
$platform=(Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Office\ClickToRun\Configuration' -ErrorAction SilentlyContinue).Platform
if($platform -and $platform -ne $Architecture){throw (Get-OotText "本机 Office 为 $platform，请使用对应安装包。" "Office on this computer is $platform. Use the matching installer.")}
if([IntPtr]::Size -ne 8){throw (Get-OotText '请通过 Install.cmd 在 64 位 PowerShell 中运行安装。' 'Use Install.cmd to install from 64-bit PowerShell.')}
$source=Join-Path $taskRoot "dist\$Architecture\openOfficetab.dll"
if(-not (Test-Path -LiteralPath $source)){throw (Get-OotText "找不到插件：$source" "Add-in not found: $source")}
$sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$buildHash=$sourceHash.Substring(0,12).ToLowerInvariant()
$destination=Join-Path $env:LOCALAPPDATA "openOfficetab\bin\0.1.9-$buildHash\$Architecture"
New-Item -ItemType Directory -Force $destination | Out-Null
$installed=Join-Path $destination 'openOfficetab.dll'
if(Test-Path -LiteralPath $installed){
    if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $sourceHash){throw 'Existing version directory contains a different DLL; installation stopped.'}
}else{
    Copy-Item -LiteralPath $source -Destination $installed
    Unblock-File -LiteralPath $installed
}
$regsvr=Join-Path $env:WINDIR ($(if($Architecture -eq 'x64'){'System32'}else{'SysWOW64'})+'\regsvr32.exe')
$process=Start-Process -FilePath $regsvr -ArgumentList @('/s','/n','/i:user',('"'+$installed+'"')) -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0){throw (Get-OotText "插件注册失败，退出码 $($process.ExitCode)。" "Add-in registration failed with exit code $($process.ExitCode).")}
$results=foreach($app in 'Word','Excel','PowerPoint'){
    $key="HKCU:\Software\Microsoft\Office\$app\Addins\openOfficetab.Connect"
    $entry=Get-ItemProperty -LiteralPath $key
    if($entry.LoadBehavior -ne 3){throw (Get-OotText "$app 自动加载注册未通过。" "Automatic loading registration could not be verified for $app.")}
    [pscustomobject]@{Application=$app;LoadBehavior=$entry.LoadBehavior}
}
Write-Host (Get-OotText "openOfficetab 0.1.9 已安装到：$installed" "openOfficetab 0.1.9 installed to: $installed")
$results | Format-Table
Write-Host (Get-OotText '重新打开 Office 后，标签会出现在功能区下方。可从 openOfficetab 功能区隐藏、设置或恢复窗口。' 'Reopen Office to see tabs below the ribbon. Use the openOfficetab ribbon to configure, hide, or recover tabs and windows.')
