$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-install.ps1')
$sandbox=Join-Path $root ('artifacts/uninstall-worker-'+[guid]::NewGuid().ToString('N'))
$originalAppData=$env:LOCALAPPDATA
$children=@()
try {
    $env:LOCALAPPDATA=$sandbox
    foreach($relative in 'openOfficetab/cache/test/scripts','unrelated/scripts'){New-Item -ItemType Directory -Path (Join-Path $sandbox $relative) -Force|Out-Null}
    $owned=Join-Path $sandbox 'openOfficetab/cache/test/scripts/desktop-service.ps1'
    $other=Join-Path $sandbox 'unrelated/scripts/desktop-service.ps1'
    'Start-Sleep -Seconds 2'|Set-Content -LiteralPath $owned
    'Start-Sleep -Seconds 20'|Set-Content -LiteralPath $other
    # Rust PathBuf accepts a slash within its joined script component.
    foreach($script in $owned.Replace('\scripts\','\scripts/'),$other){$children+=Start-Process -FilePath powershell.exe -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$script+'"')) -WindowStyle Hidden -PassThru}
    $timer=[Diagnostics.Stopwatch]::StartNew()
    Assert-OotCenterClosed
    if($timer.Elapsed.TotalSeconds -lt 1 -or -not $children[0].HasExited){throw 'Own service worker was not awaited.'}
    if($children[1].HasExited -or $timer.Elapsed.TotalSeconds -gt 10){throw 'An unrelated script was included in maintenance.'}
    'Uninstall worker wait and unrelated-process exclusion passed.'
} finally {
    $env:LOCALAPPDATA=$originalAppData
    foreach($child in $children){if(-not $child.HasExited){$child.Kill();$child.WaitForExit()};$child.Dispose()}
    $resolved=(Resolve-Path -LiteralPath $sandbox).Path
    if([IO.Path]::GetDirectoryName($resolved) -ne (Join-Path $root 'artifacts')){throw 'Test cleanup identity mismatch.'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
