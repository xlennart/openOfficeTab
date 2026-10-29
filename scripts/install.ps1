param([ValidateSet('auto','x64','x86')][string]$Architecture='auto',[switch]$AllowSideBySideWhileRunning,[ValidateSet('auto','zh-CN','en')][string]$Language='auto',[string]$HostSelection='Word,Excel,PowerPoint',[switch]$DesktopShortcut,[string]$CenterSource)
$ErrorActionPreference='Stop'
if($env:OPENOFFICETAB_SETUP_UTF8){[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false);$OutputEncoding=[Console]::OutputEncoding}
. (Join-Path $PSScriptRoot 'localization.ps1')
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Language=$Language;Architecture=$Architecture;AllowSideBySideWhileRunning=[bool]$AllowSideBySideWhileRunning;HostSelection=$HostSelection;DesktopShortcut=[bool]$DesktopShortcut;CenterSource=$CenterSource}){return}
. (Join-Path $PSScriptRoot 'desktop-install.ps1')
. (Join-Path $PSScriptRoot 'office-architecture.ps1')
$selected=if($HostSelection -eq 'None'){@()}else{@($HostSelection -split ',')}
if(@($selected | Where-Object {$_ -notin @('Word','Excel','PowerPoint')}).Count){throw 'Select supported Office applications.'}
$maintenance=Enter-OotMaintenance
try {
$taskRoot=Split-Path $PSScriptRoot -Parent
$running=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue)
if($running.Count){
    if(-not $AllowSideBySideWhileRunning){throw (Get-OotText '请先保存文档并退出 Word、Excel、PowerPoint，再运行安装。安装不会关闭你的 Office 窗口。' 'Save your work and fully exit Word, Excel, and PowerPoint before installing. The installer will not close Office windows.')}
}
$Architecture=Resolve-OotInstallArchitecture $Architecture
if([IntPtr]::Size -ne 8){throw (Get-OotText '请通过 Install.cmd 在 64 位 PowerShell 中运行安装。' 'Use Install.cmd to install from 64-bit PowerShell.')}
$source=Join-Path $taskRoot "dist\$Architecture\openOfficetab.dll"
if(-not (Test-Path -LiteralPath $source)){throw (Get-OotText "找不到插件：$source" "Add-in not found: $source")}
if([OpenOfficeTab.Desktop.Api]::ExecutableArchitecture($source) -ne $Architecture){throw (Get-OotText '插件文件位数与 Office 不匹配，未执行注册。' 'The add-in file architecture does not match Office. Registration was not performed.')}
$sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$buildHash=$sourceHash.Substring(0,12).ToLowerInvariant()
$destination=Join-Path $env:LOCALAPPDATA "openOfficetab\bin\0.2.2-$buildHash\$Architecture"
Assert-OotNoReparse (Join-Path $env:LOCALAPPDATA 'openOfficetab')
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
    Set-ItemProperty -LiteralPath $key -Name LoadBehavior -Value $(if($app -in $selected){3}else{0}) -Type DWord
    $entry=Get-ItemProperty -LiteralPath $key
    if($entry.LoadBehavior -ne $(if($app -in $selected){3}else{0})){throw (Get-OotText "$app 自动加载注册未通过。" "Automatic loading registration could not be verified for $app.")}
    [pscustomobject]@{Application=$app;LoadBehavior=$entry.LoadBehavior}
}
Install-OotDesktop -TaskRoot $taskRoot -SourceDll $source -Architecture $Architecture -CenterSource $CenterSource -DesktopShortcut:$DesktopShortcut
Write-Host (Get-OotText "openOfficeTab 0.2.2 已安装到：$installed" "openOfficeTab 0.2.2 installed to: $installed")
$results | Format-Table
Write-Host (Get-OotText '重新打开 Office 后，标签会出现在功能区下方。可从 openOfficeTab 功能区隐藏、设置或恢复窗口。' 'Reopen Office to see tabs below the ribbon. Use the openOfficeTab ribbon to configure, hide, or recover tabs and windows.')
} finally {$maintenance.ReleaseMutex();$maintenance.Dispose()}
