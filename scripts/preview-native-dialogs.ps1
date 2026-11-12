param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build.ps1') -Architecture x64}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'C++ build tools are required.'}
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$objects=Get-ChildItem (Join-Path $root 'src') -Filter '*.cpp' | ForEach-Object {'"'+(Join-Path $root ('build\x64\'+$_.BaseName+'.obj'))+'"'}
New-Item -ItemType Directory -Force (Join-Path $root 'artifacts') | Out-Null
$batch=@"
@echo off
call "$vc" x64 >nul
if errorlevel 1 exit /b 1
cd /d "$root"
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /MT /DUNICODE /D_UNICODE tests\native-dialog-preview.cpp $($objects -join ' ') /Fo:build\x64\native-dialog-preview.obj /Fe:build\x64\native-dialog-preview.exe /link ole32.lib oleaut32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib advapi32.lib uuid.lib
if errorlevel 1 exit /b 1
build\x64\native-dialog-preview.exe
"@
$path=Join-Path $root 'build\preview-native-dialogs.cmd'
[IO.File]::WriteAllText($path,$batch,[Text.UTF8Encoding]::new($false))
& $env:ComSpec /d /c $path
if($LASTEXITCODE -ne 0){throw 'Native UI preview failed.'}
