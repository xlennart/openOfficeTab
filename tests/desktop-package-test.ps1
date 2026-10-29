param([string]$Version='0.2.2',[string]$DeliveryDirectory)
$ErrorActionPreference='Stop'
$ootRoot=Split-Path $PSScriptRoot -Parent
$ootDelivery=if($DeliveryDirectory){[IO.Path]::GetFullPath($DeliveryDirectory)}else{Join-Path $ootRoot 'delivery'}
$ootSetup=Join-Path $ootDelivery ("openOfficetab-$Version-setup-x64.exe")
$ootZip=Join-Path $ootDelivery ("openOfficetab-$Version-win-x64.zip")
$ootOutput=Join-Path $ootRoot ('artifacts\package-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $ootOutput | Out-Null
$ootReport=Join-Path $ootOutput 'embedded.json'
$ootProcess=Start-Process -FilePath $ootSetup -ArgumentList @('--verify-payloads',('"'+$ootReport+'"')) -WindowStyle Hidden -Wait -PassThru
if($ootProcess.ExitCode -ne 0){throw 'Tauri payload self-verification failed.'}
Add-Type -Path (Join-Path $ootRoot 'build/desktop-service/openOfficetab.Service.dll')
$payloads=@(Get-Content -LiteralPath $ootReport -Raw|ConvertFrom-Json|ForEach-Object {foreach($item in $_){$item}})
if($payloads.Count -ne 2){throw 'Expected both Office payloads.'}
foreach($architecture in 'x86','x64'){
    $file=Join-Path $ootRoot "dist/$architecture/openOfficetab.dll"
    $entry=@($payloads|Where-Object path -eq "dist/$architecture/openOfficetab.dll")
    if($entry.Count -ne 1 -or $entry[0].sha256 -ne (Get-FileHash -LiteralPath $file).Hash -or [OpenOfficeTab.Desktop.Api]::ExecutableArchitecture($file) -ne $architecture){throw 'Embedded architecture/hash mismatch.'}
}
if([OpenOfficeTab.Desktop.Api]::ExecutableArchitecture($ootSetup) -ne 'x64'){throw 'The unified Windows installer must remain x64.'}
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$ootArchive=[IO.Compression.ZipFile]::OpenRead($ootZip)
try {
    foreach($name in 'app/openOfficetab.Center.exe','service/openOfficetab.Service.dll','scripts/install.ps1','scripts/uninstall.ps1','scripts/desktop-service.ps1','scripts/desktop-install.ps1','dist/x64/openOfficetab.dll','dist/x86/openOfficetab.dll','scripts/office-architecture.ps1','LICENSE','THIRD-PARTY-NOTICES.txt','README.md','README.zh-CN.md'){
        if(-not $ootArchive.GetEntry($name)){throw "Missing package entry: $name"}
    }
    foreach($entry in $ootArchive.Entries){
        if($entry.FullName.Contains('\') -or $entry.FullName -match '(^/|(^|/)\.\.(/|$))'){throw 'Unsafe ZIP path.'}
        if($entry.FullName -match '(^|/)(AGENTS\.md|\.aoci(/|$)|aoci(?:\.[^/]*)?\.txt$)'){throw 'Private governance asset in public package.'}
    }
    foreach($architecture in 'x86','x64'){
        $entry=$ootArchive.GetEntry("dist/$architecture/openOfficetab.dll");$stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
        try{$actual=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}
        if($actual -ne (Get-FileHash -LiteralPath (Join-Path $ootRoot "dist/$architecture/openOfficetab.dll")).Hash){throw 'ZIP payload differs from the verified native build.'}
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
