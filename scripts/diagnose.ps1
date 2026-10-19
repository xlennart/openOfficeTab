param([switch]$Repair,[ValidateSet('auto','zh-CN','en')][string]$Language='auto')
$ErrorActionPreference='Stop'
if($env:OPENOFFICETAB_SETUP_UTF8){[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false);$OutputEncoding=[Console]::OutputEncoding}
. (Join-Path $PSScriptRoot 'localization.ps1')
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Language=$Language;Repair=[bool]$Repair}){return}
if($Repair){
    & (Join-Path $PSScriptRoot 'install.ps1') -Architecture x64 -Language $Language
    Write-Output (Get-OotText '注册已修复，请重新打开 Office。文档、设置和 Office 安全策略保留。' 'Registration repaired. Reopen Office. Documents, settings, and Office security policy are preserved.')
    return
}
$taskRoot=Split-Path $PSScriptRoot -Parent
$source=Join-Path $taskRoot 'dist\x64\openOfficetab.dll'
$registered=Get-ItemProperty -LiteralPath 'HKCU:\Software\Classes\CLSID\{65F17021-3B6B-4C5E-B54B-512806431710}\InprocServer32' -ErrorAction SilentlyContinue
$installed=if($registered){$registered.'(default)'}else{''}
$exists=$installed -and (Test-Path -LiteralPath $installed -PathType Leaf)
$matches=$exists -and ((Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $installed).Hash)
$hosts=foreach($hostName in 'Word','Excel','PowerPoint'){
    $entry=Get-ItemProperty -LiteralPath "HKCU:\Software\Microsoft\Office\$hostName\Addins\openOfficetab.Connect" -ErrorAction SilentlyContinue
    [pscustomobject]@{Host=$hostName;Registered=[bool]$entry;LoadBehavior=if($entry){$entry.LoadBehavior}else{0}}
}
[pscustomobject]@{Version='0.1.8';Scope='Current desktop user';DllExists=[bool]$exists;MatchesThisInstaller=[bool]$matches;OfficeProcessCount=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue).Count;Hosts=@($hosts);Privacy='No account names, document names, paths, content or raw logs'} | ConvertTo-Json -Depth 4
Write-Output (Get-OotText '若已加载但标签隐藏：在 Office 功能区选择“恢复标签栏与窗口”。' 'If loaded but tabs are hidden, select Recover tabs and windows on the openOfficetab ribbon.')
Write-Output (Get-OotText '若注册缺失或 DLL 不匹配：保存并关闭 Office，然后点击安装器的“修复注册”。' 'If registration is missing or the DLL does not match, save and exit Office, then select Repair registration in the installer.')
Write-Output (Get-OotText '受保护视图或组织策略可能限制加载；保留 Office 的原生安全提示。' 'Protected View or organizational policy can restrict loading. Keep Office security prompts enabled.')
