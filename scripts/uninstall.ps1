param([switch]$RemoveSettings,[ValidateSet('auto','zh-CN','en')][string]$Language='auto')
$ErrorActionPreference='Stop'
if($env:OPENOFFICETAB_SETUP_UTF8){[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false);$OutputEncoding=[Console]::OutputEncoding}
. (Join-Path $PSScriptRoot 'localization.ps1')
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Language=$Language;RemoveSettings=[bool]$RemoveSettings}){return}
. (Join-Path $PSScriptRoot 'desktop-install.ps1')
. (Join-Path $PSScriptRoot 'office-architecture.ps1')
Initialize-OotArchitecture
$maintenance=Enter-OotMaintenance
try {
Assert-OotCenterClosed
if(@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue).Count){throw (Get-OotText '请先保存文档并退出三款 Office，再运行卸载。' 'Save your work and fully exit all three Office applications before uninstalling.')}
if([IntPtr]::Size -ne 8){throw (Get-OotText '请通过 Uninstall.cmd 在 64 位 PowerShell 中运行卸载。' 'Use Uninstall.cmd to uninstall from 64-bit PowerShell.')}
$installRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab'
$bin=Join-Path $installRoot 'bin'
Assert-OotNoReparse $installRoot
foreach($architecture in 'x64','x86'){
    # Installations use hash-versioned directories so a loaded DLL is never
    # overwritten. Unregister one installed binary for each architecture.
    $dll=[OpenOfficeTab.Desktop.Api]::RegisteredDll($architecture)
    if($dll -and (-not $dll.StartsWith($bin+'\',[StringComparison]::OrdinalIgnoreCase) -or $dll.Substring($bin.Length+1) -notmatch ('^\d+\.\d+\.\d+(-[0-9a-f]{12})?\\'+$architecture+'\\openOfficetab\.dll$'))){throw (Get-OotText '插件注册路径不属于本安装目录，卸载已停止。' 'The registered add-in is outside this installation. Uninstallation stopped.')}
    if($dll -and -not(Test-Path -LiteralPath $dll)){$dll=$null}
    if(-not $dll -and (Test-Path -LiteralPath $bin)){
        $dll=Get-ChildItem -LiteralPath $bin -Filter 'openOfficetab.dll' -File -Recurse |
            Where-Object { $_.FullName.Substring($bin.Length+1) -match ('^\d+\.\d+\.\d+(-[0-9a-f]{12})?\\'+$architecture+'\\openOfficetab\.dll$') } |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if($dll){
        if([OpenOfficeTab.Desktop.Api]::ExecutableArchitecture($dll) -ne $architecture){throw (Get-OotText '插件文件位数异常，卸载已停止。' 'The add-in file architecture is invalid. Uninstallation stopped.')}
        $regsvr=Join-Path $env:WINDIR ($(if($architecture -eq 'x64'){'System32'}else{'SysWOW64'})+'\regsvr32.exe')
        $process=Start-Process -FilePath $regsvr -ArgumentList @('/s','/u',('"'+$dll+'"')) -WindowStyle Hidden -Wait -PassThru
        if($process.ExitCode -ne 0){throw (Get-OotText "卸载注册失败：$($process.ExitCode)。" "Unregistration failed with exit code $($process.ExitCode).")}
    }
}
# Delete only our explicitly resolved bin directory, after validating its identity.
$expected=[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'openOfficetab\bin'))
if(Test-Path -LiteralPath $bin){$resolved=(Resolve-Path -LiteralPath $bin).Path;if($resolved -ne $expected){throw (Get-OotText '卸载路径校验失败。' 'Uninstall directory identity check failed.')};Remove-Item -LiteralPath $resolved -Recurse -Force}
if($RemoveSettings){foreach($name in 'settings.ini','documents.ini','runtime.log'){$file=Join-Path $installRoot $name;if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file -Force}}}
Remove-OotUpdateTask
Remove-OotDesktop
foreach($name in 'cache','updates'){$path=Join-Path $installRoot $name;if(Test-Path -LiteralPath $path){$resolved=(Resolve-Path -LiteralPath $path).Path;if([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($installRoot)){throw 'Uninstall cache identity check failed.'};Remove-Item -LiteralPath $resolved -Recurse -Force}}
Write-Host (Get-OotText 'openOfficeTab 已卸载。默认保留设置和日志；使用 -RemoveSettings 可清除它们。' 'openOfficeTab uninstalled. Settings and logs are kept by default; use -RemoveSettings to remove them.')
} finally {$maintenance.ReleaseMutex();$maintenance.Dispose()}
