param([ValidateSet('x86','x64')][string]$Architecture='x64',[string]$Root=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
foreach($name in 'Word','Excel','PowerPoint'){
 $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $Root 'tests/save-runtime.ps1')+'"'),'-HostName',$name,'-Architecture',$Architecture)
 $start=[Diagnostics.ProcessStartInfo]::new()
 $start.FileName=Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
 $start.Arguments=$arguments -join ' '
 $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardError=$true
 $child=[Diagnostics.Process]::Start($start)
 $errorTask=$child.StandardError.ReadToEndAsync()
 if(-not $child.WaitForExit(90000)){throw 'Save client timed out'};$errorTask.GetAwaiter().GetResult()|Set-Content (Join-Path $Root ('artifacts/save-'+$name+'.err'));if($child.ExitCode -ne 0){throw ('Native save test failed: '+$name)}
 $file=Join-Path $Root ('artifacts/save-runtime-'+$name+'.json')
 $report=Get-Content $file -Raw|ConvertFrom-Json
 $until=[DateTime]::UtcNow.AddSeconds(30)
 do{Start-Sleep -Milliseconds 100;$process=Get-Process -Id $report.OfficePid -ErrorAction SilentlyContinue
  if($process -and $process.StartTime.ToUniversalTime().Ticks -ne ([datetime]$report.OfficeStartUtc).ToUniversalTime().Ticks){$process=$null}
 }while($process -and [DateTime]::UtcNow -lt $until)
 if($process){throw 'Saved fixture closure left Office running.'}
 if((Get-FileHash $report.FixtureFile).Hash -eq $report.BeforeSha256){throw 'The saved file did not change.'}
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $archive=[IO.Compression.ZipFile]::OpenRead($report.FixtureFile)
 $found=$false
 try{foreach($entry in $archive.Entries){if($entry.FullName -match '^(word/document|xl/(sharedStrings|worksheets/sheet1)|ppt/slides/slide1)\.xml$'){$reader=[IO.StreamReader]::new($entry.Open());try{if($reader.ReadToEnd().Contains('OOT_SAVE_QA')){$found=$true}}finally{$reader.Dispose()}}}}finally{$archive.Dispose()}
 if(-not $found){throw 'Saved Office XML does not contain the generated marker.'}
 $report|Add-Member NativeSavePersisted $true
 $report|Add-Member Passed $true
 $report|Add-Member ProcessExited $true
 $report|ConvertTo-Json|Set-Content $file -Encoding UTF8
 $resolved=(Resolve-Path $report.FixtureDirectory).Path
 if([IO.Path]::GetDirectoryName($resolved) -ne (Join-Path $Root 'artifacts') -or -not [IO.Path]::GetFileName($resolved).StartsWith('save-fixture-')){throw 'Save cleanup identity mismatch'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
'Native save and process exit passed in all three hosts.'
