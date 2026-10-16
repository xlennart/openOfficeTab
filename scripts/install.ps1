param([ValidateSet('x64','x86')][string]$Architecture='x64',[switch]$AllowSideBySideWhileRunning)
$ErrorActionPreference='Stop'
if($env:OPENOFFICETAB_SETUP_UTF8){[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false);$OutputEncoding=[Console]::OutputEncoding}
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Architecture=$Architecture;AllowSideBySideWhileRunning=[bool]$AllowSideBySideWhileRunning}){return}
$taskRoot=Split-Path $PSScriptRoot -Parent
$running=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue)
if($running.Count){
    if(-not $AllowSideBySideWhileRunning){throw '请先保存文档并退出 Word、Excel、PowerPoint，再运行安装。安装不会关闭你的 Office 窗口。'}
}
$platform=(Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Office\ClickToRun\Configuration' -ErrorAction SilentlyContinue).Platform
if($platform -and $platform -ne $Architecture){throw "本机 Office 为 $platform，请使用对应安装包。"}
if([IntPtr]::Size -ne 8){throw '请通过 Install.cmd 在 64 位 PowerShell 中运行安装。'}
$source=Join-Path $taskRoot "dist\$Architecture\openOfficetab.dll"
if(-not (Test-Path -LiteralPath $source)){throw "找不到插件：$source"}
$sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$buildHash=$sourceHash.Substring(0,12).ToLowerInvariant()
$destination=Join-Path $env:LOCALAPPDATA "openOfficetab\bin\0.1.6-$buildHash\$Architecture"
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
if($process.ExitCode -ne 0){throw "插件注册失败，退出码 $($process.ExitCode)。"}
$results=foreach($app in 'Word','Excel','PowerPoint'){
    $key="HKCU:\Software\Microsoft\Office\$app\Addins\openOfficetab.Connect"
    $entry=Get-ItemProperty -LiteralPath $key
    if($entry.LoadBehavior -ne 3){throw "$app 自动加载注册未通过。"}
    [pscustomobject]@{Application=$app;LoadBehavior=$entry.LoadBehavior}
}
Write-Host "openOfficetab 0.1.6 已安装到：$installed"
$results | Format-Table
Write-Host '重新打开 Office 后，标签会出现在功能区下方。可从 openOfficetab 功能区隐藏、设置或恢复窗口。'
