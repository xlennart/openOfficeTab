param([string]$Version='0.2.1')
$ErrorActionPreference='Stop'
$ootRoot=Split-Path $PSScriptRoot -Parent
$ootDelivery=Join-Path $ootRoot 'delivery'
$ootSetup=Join-Path $ootDelivery ("openOfficetab-$Version-setup-x64.exe")
$ootZip=Join-Path $ootDelivery ("openOfficetab-$Version-win-x64.zip")
$ootOutput=Join-Path $ootRoot ('artifacts\package-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $ootOutput | Out-Null
$ootReport=Join-Path $ootOutput 'embedded.sha256'
$ootProcess=Start-Process -FilePath $ootSetup -ArgumentList @('--verify',('"'+$ootReport+'"')) -WindowStyle Hidden -Wait -PassThru
if($ootProcess.ExitCode -ne 0 -or (Get-Content -LiteralPath $ootReport -Raw).Trim() -ne (Get-FileHash (Join-Path $ootRoot 'dist\x64\openOfficetab.dll')).Hash){throw 'Tauri embedded DLL hash mismatch.'}
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$ootArchive=[IO.Compression.ZipFile]::OpenRead($ootZip)
try {
    foreach($name in 'app/openOfficetab.Center.exe','service/openOfficetab.Service.dll','scripts/install.ps1','scripts/uninstall.ps1','scripts/desktop-service.ps1','scripts/desktop-install.ps1','dist/x64/openOfficetab.dll','LICENSE','THIRD-PARTY-NOTICES.txt','README.md','README.zh-CN.md'){
        if(-not $ootArchive.GetEntry($name)){throw "Missing package entry: $name"}
    }
    foreach($entry in $ootArchive.Entries){
        if($entry.FullName.Contains('\') -or $entry.FullName -match '(^/|(^|/)\.\.(/|$))'){throw 'Unsafe ZIP path.'}
        if($entry.FullName -match '(^|/)(AGENTS\.md|\.aoci(/|$)|aoci(?:\.[^/]*)?\.txt$)'){throw 'Private governance asset in public package.'}
    }
    $ootEntry=$ootArchive.GetEntry('app/openOfficetab.Center.exe');$ootStream=$ootEntry.Open();$ootSha=[Security.Cryptography.SHA256]::Create()
    try {$ootCenterHash=([BitConverter]::ToString($ootSha.ComputeHash($ootStream))).Replace('-','')}finally{$ootStream.Dispose();$ootSha.Dispose()}
    if($ootCenterHash -ne (Get-FileHash $ootSetup).Hash){throw 'Setup and installed center do not contain identical code.'}
}finally{$ootArchive.Dispose()}
foreach($line in Get-Content (Join-Path $ootDelivery 'SHA256SUMS.txt')){
    if($line -notmatch '^([0-9a-f]{64})  ([^\\/]+)$'){throw 'Invalid checksum line.'}
    if((Get-FileHash (Join-Path $ootDelivery $Matches[2])).Hash -ne $Matches[1]){throw 'Delivery checksum mismatch.'}
}
Write-Output 'Tauri package payload, self-verification, ZIP paths, license entries and checksums passed. No install performed.'
