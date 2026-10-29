param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName='Word')
$ErrorActionPreference='Stop'
if($env:CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY){throw 'Run outside the development process context.'}
$root=Split-Path $PSScriptRoot -Parent
$log=Join-Path $root "artifacts/lifecycle-$HostName-child.log"
$errors=Join-Path $root "artifacts/lifecycle-$HostName-child.error"
$script=Join-Path $PSScriptRoot 'lifecycle-runtime.ps1'
# Check exit after the automation client releases all of its COM wrappers.
$start=New-Object Diagnostics.ProcessStartInfo
$start.FileName="$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$start.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$script+'" -HostName '+$HostName+' -DeferProcessExitCheck'
$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
$child=New-Object Diagnostics.Process;$child.StartInfo=$start;[void]$child.Start()
$output=$child.StandardOutput.ReadToEndAsync();$errorOutput=$child.StandardError.ReadToEndAsync()
if(-not $child.WaitForExit(180000)){
    [IO.File]::WriteAllText($errors,'Lifecycle automation timed out after 180 seconds. This run did not pass. Office documents are retained.')
    try{$child.Kill()}catch{}
    throw "Lifecycle automation timed out: $HostName; Office documents retained."
}
[IO.File]::WriteAllText($log,$output.GetAwaiter().GetResult());[IO.File]::WriteAllText($errors,$errorOutput.GetAwaiter().GetResult())
if($child.ExitCode -ne 0){throw "Lifecycle child failed: $HostName; inspect $errors"}
$resultFile=Join-Path $root "artifacts/lifecycle-$HostName.json"
$result=Get-Content -LiteralPath $resultFile -Raw|ConvertFrom-Json
$deadline=[DateTime]::UtcNow.AddSeconds(8)
do{
 $process=Get-Process -Id $result.ProcessId -ErrorAction SilentlyContinue
 $same=$process -and $process.StartTime.ToUniversalTime().Ticks -eq ([datetime]$result.ProcessStartUtc).ToUniversalTime().Ticks
 if(-not $same){break};Start-Sleep -Milliseconds 100
}while([DateTime]::UtcNow -lt $deadline)
$result.LastDocumentExitsApplication=(-not $same);$result.Passed=(-not $same)
$result|Add-Member -NotePropertyName ProcessExitedAfterHarness -NotePropertyValue (-not $same) -Force
$result|ConvertTo-Json|Set-Content -LiteralPath $resultFile -Encoding UTF8
if($same){throw "Office process remains after automation has exited: $HostName"}
$result
