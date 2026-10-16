param([string]$Version='0.1.6')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if($Version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid version.'}
$dll=Join-Path $taskRoot 'dist\x64\openOfficetab.dll'
if(-not (Test-Path -LiteralPath $dll)){throw 'Build the x64 DLL first.'}
$delivery=Join-Path $taskRoot 'delivery'
$stage=Join-Path $taskRoot ('build\package-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $delivery,$stage,(Join-Path $stage 'scripts'),(Join-Path $stage 'dist\x64') -Force | Out-Null
foreach($file in 'Install.cmd','Uninstall.cmd','LICENSE'){Copy-Item -LiteralPath (Join-Path $taskRoot $file) -Destination (Join-Path $stage $file)}
Copy-Item -LiteralPath (Join-Path $taskRoot 'docs\installation.md') -Destination (Join-Path $stage 'README.md')
foreach($file in 'install.ps1','uninstall.ps1','desktop-context.ps1','diagnose.ps1'){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $stage ('scripts\'+$file))}
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage 'dist\x64\openOfficetab.dll')
$zip=Join-Path $delivery "openOfficetab-$Version-win-x64.zip"
# ZipArchive produces forward-slash entry names for a portable payload.
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip -Force}
$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($file in Get-ChildItem -LiteralPath $stage -File -Recurse){$entryName=$file.FullName.Substring($stage.Length+1).Replace('\','/');[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$entryName,[IO.Compression.CompressionLevel]::Optimal)|Out-Null}}finally{$archive.Dispose()}
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(-not (Test-Path -LiteralPath $compiler)){throw 'The .NET Framework C# compiler is required to build Setup.'}
$setup=Join-Path $delivery "openOfficetab-$Version-setup-x64.exe"
& $compiler /nologo /codepage:65001 /target:winexe /platform:x64 /optimize+ ("/out:$setup") ("/resource:$zip,openOfficetab.Payload.zip") /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll (Join-Path $taskRoot 'installer\Setup.cs')
if($LASTEXITCODE -ne 0){throw 'Setup compilation failed.'}
$report=Join-Path $stage 'embedded-dll.sha256'
$process=Start-Process -FilePath $setup -ArgumentList @('--verify',('"'+$report+'"')) -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0 -or (Get-Content -LiteralPath $report -Raw).Trim() -ne (Get-FileHash -LiteralPath $dll).Hash){throw 'Embedded installer DLL verification failed.'}
$checksums=foreach($file in $setup,$zip){'{0}  {1}' -f (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant(),(Split-Path $file -Leaf)}
[IO.File]::WriteAllLines((Join-Path $delivery 'SHA256SUMS.txt'),$checksums,[Text.Encoding]::ASCII)
Write-Output $setup
Write-Output $zip
Write-Output (Join-Path $delivery 'SHA256SUMS.txt')
$resolvedStage=(Resolve-Path -LiteralPath $stage).Path
$expectedParent=[IO.Path]::GetFullPath((Join-Path $taskRoot 'build'))
if([IO.Path]::GetDirectoryName($resolvedStage) -ne $expectedParent -or -not ([IO.Path]::GetFileName($resolvedStage).StartsWith('package-'))){throw 'Package work directory identity check failed.'}
Remove-Item -LiteralPath $resolvedStage -Recurse -Force
