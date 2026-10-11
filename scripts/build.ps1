param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vsPath){throw '请安装 Visual Studio Build Tools 的 C++ 编译工具与 Windows SDK。'}
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$buildDir=Join-Path $taskRoot "build\$Architecture"
$outputDir=Join-Path $taskRoot "dist\$Architecture"
New-Item -ItemType Directory -Force $buildDir,$outputDir | Out-Null
$sources=Get-ChildItem (Join-Path $taskRoot 'src') -Filter '*.cpp' | ForEach-Object {'"'+$_.FullName+'"'}
$batch=@"
@echo off
call "$vcvars" $Architecture >nul
if errorlevel 1 exit /b 1
cd /d "$buildDir"
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /LD $($sources -join ' ') /link /OUT:"$outputDir\openOfficetab.dll" /DEF:"$taskRoot\src\exports.def" ole32.lib oleaut32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib advapi32.lib uuid.lib
"@
$batchPath=Join-Path $buildDir 'build.cmd'
[IO.File]::WriteAllText($batchPath,$batch,[Text.UTF8Encoding]::new($false))
& $env:ComSpec /d /c $batchPath
if($LASTEXITCODE -ne 0){throw "编译失败：$LASTEXITCODE"}
Get-FileHash (Join-Path $outputDir 'openOfficetab.dll') -Algorithm SHA256
