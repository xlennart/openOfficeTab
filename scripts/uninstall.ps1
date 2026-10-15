param([switch]$RemoveSettings)
$ErrorActionPreference='Stop'
if($env:OPENOFFICETAB_SETUP_UTF8){[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false);$OutputEncoding=[Console]::OutputEncoding}
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{RemoveSettings=[bool]$RemoveSettings}){return}
if(@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue).Count){throw '请先保存文档并退出三款 Office，再运行卸载。'}
if([IntPtr]::Size -ne 8){throw '请通过 Uninstall.cmd 在 64 位 PowerShell 中运行卸载。'}
$installRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab'
$bin=Join-Path $installRoot 'bin'
foreach($architecture in 'x64','x86'){
    # Installations use hash-versioned directories so a loaded DLL is never
    # overwritten. Unregister one installed binary for each architecture.
    $dll=$null
    if(Test-Path -LiteralPath $bin){
        $dll=Get-ChildItem -LiteralPath $bin -Filter 'openOfficetab.dll' -File -Recurse |
            Where-Object { $_.FullName.Substring($bin.Length+1) -match ('^0\.1\.[012](-[0-9a-f]{12})?\\'+$architecture+'\\openOfficetab\.dll$') } |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if($dll){
        $regsvr=Join-Path $env:WINDIR ($(if($architecture -eq 'x64'){'System32'}else{'SysWOW64'})+'\regsvr32.exe')
        $process=Start-Process -FilePath $regsvr -ArgumentList @('/s','/u',('"'+$dll+'"')) -WindowStyle Hidden -Wait -PassThru
        if($process.ExitCode -ne 0){throw "卸载注册失败：$($process.ExitCode)。"}
    }
}
# Delete only our explicitly resolved bin directory, after validating its identity.
$expected=[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'openOfficetab\bin'))
if(Test-Path -LiteralPath $bin){$resolved=(Resolve-Path -LiteralPath $bin).Path;if($resolved -ne $expected){throw '卸载路径校验失败。'};Remove-Item -LiteralPath $resolved -Recurse -Force}
if($RemoveSettings){foreach($name in 'settings.ini','documents.ini','runtime.log'){$file=Join-Path $installRoot $name;if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file -Force}}}
Write-Host 'openOfficetab 已卸载。默认保留设置和日志；使用 -RemoveSettings 可清除它们。'
