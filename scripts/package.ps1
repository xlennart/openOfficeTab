param([string]$Version='0.2.1',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$ootRoot=Split-Path $PSScriptRoot -Parent
if($Version -ne '0.2.1'){throw 'Update the native, service and Tauri versions together before packaging a different version.'}
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-desktop.ps1')}
$ootCenter=Join-Path $ootRoot 'dist\desktop\openOfficetab.Center.exe'
$ootDll=Join-Path $ootRoot 'dist\x64\openOfficetab.dll'
if(-not(Test-Path -LiteralPath $ootCenter)){throw 'Build the management center first.'}
$ootDelivery=Join-Path $ootRoot 'delivery'
$ootStage=Join-Path $ootRoot ('build\package-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $ootDelivery,$ootStage,(Join-Path $ootStage 'scripts'),(Join-Path $ootStage 'app'),(Join-Path $ootStage 'service'),(Join-Path $ootStage 'dist\x64') | Out-Null
foreach($file in 'Install.cmd','Uninstall.cmd','LICENSE'){Copy-Item -LiteralPath (Join-Path $ootRoot $file) -Destination $ootStage}
foreach($file in 'install.ps1','uninstall.ps1','diagnose.ps1','desktop-context.ps1','desktop-install.ps1','desktop-service.ps1','localization.ps1'){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $ootStage 'scripts')}
Copy-Item -LiteralPath $ootCenter -Destination (Join-Path $ootStage 'app\openOfficetab.Center.exe')
Copy-Item -LiteralPath $ootDll -Destination (Join-Path $ootStage 'dist\x64\openOfficetab.dll')
Copy-Item -LiteralPath (Join-Path $ootRoot 'build\desktop-service\openOfficetab.Service.dll') -Destination (Join-Path $ootStage 'service')
Copy-Item -LiteralPath (Join-Path $ootRoot 'dist\desktop\THIRD-PARTY-NOTICES.txt') -Destination $ootStage
foreach($pair in @(@('installation.en.md','README.md','(installation.md)','(README.zh-CN.md)'),@('installation.md','README.zh-CN.md','(installation.en.md)','(README.md)'))){
    $text=(Get-Content -LiteralPath (Join-Path $ootRoot ('docs\'+$pair[0])) -Raw -Encoding UTF8).Replace($pair[2],$pair[3])
    [IO.File]::WriteAllText((Join-Path $ootStage $pair[1]),$text,[Text.UTF8Encoding]::new($false))
}
$ootSetup=Join-Path $ootDelivery ('openOfficetab-'+$Version+'-setup-x64.exe')
Copy-Item -LiteralPath $ootCenter -Destination $ootSetup
$ootReport=Join-Path $ootStage 'embedded.sha256'
$ootVerify=Start-Process -FilePath $ootSetup -ArgumentList @('--verify',('"'+$ootReport+'"')) -WindowStyle Hidden -Wait -PassThru
if($ootVerify.ExitCode -ne 0 -or (Get-Content -LiteralPath $ootReport -Raw).Trim() -ne (Get-FileHash -LiteralPath $ootDll).Hash){throw 'Embedded payload verification failed.'}
Remove-Item -LiteralPath $ootReport
$ootZip=Join-Path $ootDelivery ('openOfficetab-'+$Version+'-win-x64.zip')
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
if(Test-Path -LiteralPath $ootZip){Remove-Item -LiteralPath $ootZip}
$ootArchive=[IO.Compression.ZipFile]::Open($ootZip,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($file in Get-ChildItem -LiteralPath $ootStage -File -Recurse){$relative=$file.FullName.Substring($ootStage.Length+1).Replace('\','/');[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($ootArchive,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal)}}finally{$ootArchive.Dispose()}
$ootPrivateKey=Join-Path $ootRoot 'build\signing\update-private-key.xml'
if(Test-Path -LiteralPath $ootPrivateKey){
    $ootMetadata=[ordered]@{schemaVersion=1;protocol='openofficetab-desktop-v1';architecture='x64';version=$Version;tag=('v'+$Version);name=[IO.Path]::GetFileName($ootSetup);url=('https://github.com/xlennart/openOfficetab/releases/download/v'+$Version+'/'+[IO.Path]::GetFileName($ootSetup));size=(Get-Item -LiteralPath $ootSetup).Length;sha256=(Get-FileHash -LiteralPath $ootSetup).Hash.ToLowerInvariant()}
    $ootPayload=[Text.Encoding]::UTF8.GetBytes(($ootMetadata|ConvertTo-Json -Compress))
    $ootSigner=[Security.Cryptography.RSACryptoServiceProvider]::new();$ootSigner.PersistKeyInCsp=$false
    try{$ootSigner.FromXmlString([IO.File]::ReadAllText($ootPrivateKey));$ootSignature=$ootSigner.SignData($ootPayload,[Security.Cryptography.CryptoConfig]::MapNameToOID('SHA256'))}finally{$ootSigner.Dispose()}
    $ootEnvelope=@{payload=[Convert]::ToBase64String($ootPayload);signature=[Convert]::ToBase64String($ootSignature)}|ConvertTo-Json -Compress
    [IO.File]::WriteAllText((Join-Path $ootDelivery 'openofficetab-update.json'),$ootEnvelope,[Text.UTF8Encoding]::new($false))
}
$ootSums=foreach($file in $ootSetup,$ootZip){'{0}  {1}' -f (Get-FileHash -LiteralPath $file).Hash.ToLowerInvariant(),[IO.Path]::GetFileName($file)}
[IO.File]::WriteAllLines((Join-Path $ootDelivery 'SHA256SUMS.txt'),$ootSums,[Text.Encoding]::ASCII)
$resolved=(Resolve-Path -LiteralPath $ootStage).Path
if([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath((Join-Path $ootRoot 'build')) -or [IO.Path]::GetFileName($resolved) -notmatch '^package-[0-9a-f]{32}$'){throw 'Package cleanup identity check failed.'}
Remove-Item -LiteralPath $resolved -Recurse -Force
Write-Output $ootSetup
Write-Output $ootZip
