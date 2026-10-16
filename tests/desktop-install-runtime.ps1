param([switch]$OpenFixtures,[string]$ExpectedVersion='0.1.6')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path (Join-Path $taskRoot 'artifacts') -Force | Out-Null
if($env:CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY){throw 'Run this verification outside the development process context.'}
. (Join-Path $PSScriptRoot 'common.ps1')
$registration=foreach($hostName in 'Word','Excel','PowerPoint'){
    $entry=Get-ItemProperty -LiteralPath "HKCU:\Software\Microsoft\Office\$hostName\Addins\openOfficetab.Connect"
    if($entry.LoadBehavior -ne 3){throw "Startup registration failed: $hostName"}
    [pscustomobject]@{Host=$hostName;LoadBehavior=$entry.LoadBehavior}
}
$dllPath=(Get-ItemProperty -LiteralPath 'HKCU:\Software\Classes\CLSID\{65F17021-3B6B-4C5E-B54B-512806431710}\InprocServer32').'(default)'
$sourceHash=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'dist\x64\openOfficetab.dll')).Hash
$installedHash=(Get-FileHash -LiteralPath $dllPath).Hash
if($installedHash -ne $sourceHash){throw 'Installed DLL hash differs from delivered DLL.'}
$results=@()
if($OpenFixtures){
    $fixtures=@{Word='word-demo.docx';Excel='excel-demo.xlsx';PowerPoint='powerpoint-demo.pptx'}
    # Excel may defer registering its automation object until focus leaves
    # its frame. Open all public fixtures before polling the running table.
    foreach($hostName in 'Word','Excel','PowerPoint'){
        $fixturePath=Join-Path $taskRoot ('tests\fixtures\'+$fixtures[$hostName])
        Start-Process -FilePath $fixturePath -WindowStyle Normal
        Start-Sleep -Milliseconds 500
    }
    foreach($hostName in 'Word','Excel','PowerPoint'){
        $fixturePath=Join-Path $taskRoot ('tests\fixtures\'+$fixtures[$hostName])
        $deadline=[DateTime]::UtcNow.AddSeconds(20)
        $ready=$false
        do {
            Start-Sleep -Milliseconds 250
            try{
                if($hostName -eq 'PowerPoint'){$app=[OfficeRot]::Attach($hostName)}
                else{$document=[OfficeRot]::AttachFile($fixturePath);$app=$document.Application}
                $addin=$app.COMAddIns.Item('openOfficetab.Connect')
                if(-not $addin.Connect){continue}
                $state=$addin.Object.GetState() | ConvertFrom-Json
                $ready=$state.version -eq $ExpectedVersion -and $state.tabCount -gt 0 -and @($state.paneMetrics | Where-Object displayVisible).Count -gt 0 -and @($state.paneMetrics | Where-Object {-not $_.matchesReservedRect}).Count -eq 0
            }catch{}
        }while(-not $ready -and [DateTime]::UtcNow -lt $deadline)
        if(-not $ready){throw "Normal file-open startup did not show tabs: $hostName"}
        $results += [pscustomobject]@{Host=$hostName;Connected=$addin.Connect;Version=$state.version;TabCount=$state.tabCount;DisplayVisible=$ready;AllLayoutsMatch=(@($state.paneMetrics | Where-Object {-not $_.matchesReservedRect}).Count -eq 0)}
    }
}
$result=[pscustomobject]@{Passed=$true;InstalledDll=$dllPath;SHA256=$installedHash;Registrations=@($registration);NormalFileOpen=@($results)}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskRoot 'artifacts\desktop-install-runtime.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 5
