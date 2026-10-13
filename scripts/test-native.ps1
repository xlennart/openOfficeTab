$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vsPath){throw 'C++ build tools are required.'}
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$testBuild=Join-Path $taskRoot 'build\native-tests'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null
$batch=@"
@echo off
call "$vcvars" x64 >nul
if errorlevel 1 exit /b 1
cd /d "$testBuild"
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /MT "$taskRoot\tests\layout-test.cpp" /Fe:"$testBuild\layout-test.exe"
if errorlevel 1 exit /b 1
"$testBuild\layout-test.exe"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /MT /DUNICODE /D_UNICODE "$taskRoot\tests\appearance-test.cpp" "$taskRoot\src\addin.cpp" "$taskRoot\src\session.cpp" "$taskRoot\src\features.cpp" "$taskRoot\src\tabs.cpp" /Fe:"$testBuild\appearance-test.exe" /link ole32.lib oleaut32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib advapi32.lib uuid.lib
if errorlevel 1 exit /b 1
"$testBuild\appearance-test.exe"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /MT /DUNICODE /D_UNICODE "$taskRoot\tests\window-state-test.cpp" "$taskRoot\src\addin.cpp" "$taskRoot\src\session.cpp" "$taskRoot\src\features.cpp" "$taskRoot\src\tabs.cpp" /Fe:"$testBuild\window-state-test.exe" /link ole32.lib oleaut32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib advapi32.lib uuid.lib
if errorlevel 1 exit /b 1
"$testBuild\window-state-test.exe"
"@
$batchPath=Join-Path $testBuild 'test.cmd'
[IO.File]::WriteAllText($batchPath,$batch,[Text.UTF8Encoding]::new($false))
& $env:ComSpec /d /c $batchPath
if($LASTEXITCODE -ne 0){throw 'Native tests failed.'}
