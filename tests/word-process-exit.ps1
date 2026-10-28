param([string]$ResultFile='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $ResultFile){$ResultFile=Join-Path $root 'artifacts/word-frame-close-runtime.json'}
$ResultFile=[IO.Path]::GetFullPath($ResultFile)
$artifactRoot=[IO.Path]::GetFullPath((Join-Path $root 'artifacts'))+[IO.Path]::DirectorySeparatorChar
if(-not $ResultFile.StartsWith($artifactRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Only an owned local artifact result may be updated'}
. (Join-Path $root 'scripts/desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{ResultFile=$ResultFile}){return}
$result=Get-Content -LiteralPath $ResultFile -Raw|ConvertFrom-Json
if(-not $result.Passed -or -not $result.LastCaptionCloseRequested -or $result.ProcessId -le 0 -or -not $result.ProcessStartUtc){throw 'Expected a completed owned Word caption-close fixture result'}
$watch=[Diagnostics.Stopwatch]::StartNew()
do{
 $process=Get-Process -Id $result.ProcessId -ErrorAction SilentlyContinue
 $same=$process -and $process.ProcessName -eq 'WINWORD' -and $process.StartTime.ToUniversalTime().ToString('o') -eq $result.ProcessStartUtc
 if(-not $same){break};Start-Sleep -Milliseconds 100
}while($watch.ElapsedMilliseconds -lt 8000)
$result|Add-Member -NotePropertyName ProcessExitedAfterHarness -NotePropertyValue (-not $same) -Force
$result|Add-Member -NotePropertyName ExitCheckMilliseconds -NotePropertyValue $watch.ElapsedMilliseconds -Force
if($same){$result.Passed=$false}
$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $ResultFile -Encoding UTF8
$result|Select-Object Passed,ProcessId,ProcessExitedAfterHarness,ExitCheckMilliseconds|ConvertTo-Json
if($same){throw 'Owned Word process remains; retained for diagnosis. No termination attempted.'}
