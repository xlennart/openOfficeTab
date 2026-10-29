param([switch]$UpdateLock,[switch]$SkipNative)
$ErrorActionPreference='Stop'
$ootRoot=Split-Path $PSScriptRoot -Parent
$ootDesktop=Join-Path $ootRoot 'desktop'
foreach($architecture in 'x64','x86'){if(-not $SkipNative){& (Join-Path $PSScriptRoot 'build.ps1') -Architecture $architecture;if($LASTEXITCODE -ne 0){throw 'Native add-in build failed.'}};if(-not(Test-Path -LiteralPath (Join-Path $ootRoot "dist/$architecture/openOfficetab.dll"))){throw "Build the $architecture payload first."}}
$ootServiceOutput=Join-Path $ootRoot 'build\desktop-service'
New-Item -ItemType Directory -Force $ootServiceOutput | Out-Null
$ootCompiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$ootServiceSources=@(Get-ChildItem -LiteralPath (Join-Path $ootDesktop 'service') -Filter '*.cs' -File | ForEach-Object FullName)
& $ootCompiler /nologo /codepage:65001 /target:library /optimize+ ("/out:"+(Join-Path $ootServiceOutput 'openOfficetab.Service.dll')) ("/resource:"+(Join-Path $ootDesktop 'service\update-public-key.xml')+',openOfficetab.UpdatePublicKey') ("/resource:"+(Join-Path $ootDesktop 'service\update-sources.json')+',openOfficetab.UpdateSources') ("/resource:"+(Join-Path $ootRoot 'desktop/src/messages.json')+',openOfficetab.DesktopMessages') /reference:System.Web.Extensions.dll $ootServiceSources
if($LASTEXITCODE -ne 0){throw 'Local service build failed.'}
$env:CARGO_TARGET_DIR=Join-Path $ootRoot 'build\tauri-target'
$ootCargo=Join-Path $ootRoot 'build\toolchains\cargo\bin\cargo.exe'
if(Test-Path -LiteralPath $ootCargo){$env:CARGO_HOME=Join-Path $ootRoot 'build\toolchains\cargo';$env:RUSTUP_HOME=Join-Path $ootRoot 'build\toolchains\rustup'}
else{$ootCargo=(Get-Command cargo.exe -ErrorAction Stop).Source}
Push-Location $ootDesktop
try {
    & npm.cmd ci
    if($LASTEXITCODE -ne 0){throw 'Frontend dependency restore failed.'}
    & npm.cmd run build
    if($LASTEXITCODE -ne 0){throw 'Frontend build failed.'}
} finally {Pop-Location}
$ootVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$ootVsPath=& $ootVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $ootVsPath){throw 'Visual Studio C++ Build Tools and Windows SDK are required.'}
$ootVcvars=Join-Path $ootVsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$ootBuildDir=Join-Path $ootRoot 'build\tauri'
New-Item -ItemType Directory -Force $ootBuildDir | Out-Null
$ootBatch=@"
@echo off
call "$ootVcvars" x64 >nul
if errorlevel 1 exit /b 1
"$ootCargo" build --release $(if(-not $UpdateLock){'--locked'}) --manifest-path "$ootDesktop\src-tauri\Cargo.toml"
"@
$ootBatchPath=Join-Path $ootBuildDir 'build.cmd'
[IO.File]::WriteAllText($ootBatchPath,$ootBatch,[Text.ASCIIEncoding]::new())
& $env:ComSpec /d /c $ootBatchPath
if($LASTEXITCODE -ne 0){throw "Tauri build failed: $LASTEXITCODE"}
$ootOutput=Join-Path $ootRoot 'dist\desktop'
New-Item -ItemType Directory -Force $ootOutput | Out-Null
$ootExecutable=Join-Path $ootOutput 'openOfficetab.Center.exe'
Copy-Item -LiteralPath (Join-Path $env:CARGO_TARGET_DIR 'release\openofficetab-center.exe') -Destination $ootExecutable
Copy-Item -LiteralPath (Join-Path $ootDesktop 'dist\THIRD-PARTY-NOTICES.txt') -Destination $ootOutput
Copy-Item -LiteralPath (Join-Path $ootRoot 'LICENSE') -Destination $ootOutput
Get-FileHash -LiteralPath $ootExecutable -Algorithm SHA256
Get-Item -LiteralPath $ootExecutable | Select-Object FullName,Length
