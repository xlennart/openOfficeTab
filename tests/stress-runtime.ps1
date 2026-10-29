param([ValidateSet('Word','Excel')][string]$HostName='Word',[string]$FixtureDirectory,[ValidateSet('x86','x64')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $root 'scripts/office-architecture.ps1')
$Language='auto'
if((Resolve-OotInstallArchitecture $Architecture) -ne $Architecture){throw 'Unexpected Office architecture.'}
if(-not $FixtureDirectory){$FixtureDirectory=Join-Path $root 'artifacts/stress-fixtures-v1'}
$processName=if($HostName -eq 'Word'){'WINWORD'}else{'EXCEL'}
if(Get-Process $processName -ErrorAction SilentlyContinue){throw 'Close this Office host before the isolated stress test.'}
$large=Join-Path $FixtureDirectory $(if($HostName -eq 'Word'){'Word-Long-A.docx'}else{'Excel-Large-A.xlsx'})
$small=Join-Path $root $(if($HostName -eq 'Word'){'tests/fixtures/word-demo.docx'}else{'tests/fixtures/excel-demo.xlsx'})
$before=(Get-FileHash -LiteralPath $large).Hash
$watch=[Diagnostics.Stopwatch]::StartNew()
Start-Process -FilePath $large|Out-Null
$deadline=[DateTime]::UtcNow.AddSeconds(120)
do {try{$app=[OfficeRot]::Attach($HostName);$api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$state=$api.GetState()|ConvertFrom-Json;if($state.views.Count -eq 1 -and $state.paneMetrics[0].displayVisible){break}}catch{};Start-Sleep -Milliseconds 200}while([DateTime]::UtcNow -lt $deadline)
if(-not $state -or $state.views.Count -ne 1){throw 'Large document did not initialize its native tab.'}
$openedSeconds=$watch.Elapsed.TotalSeconds
$officeProcess=Get-Process $processName
if(@($officeProcess).Count -ne 1){throw 'Ambiguous Office process identity.'}
$native=[OpenOfficeTab.Desktop.Api]::RegisteredDll($Architecture)
if(-not([OpenOfficeTab.Desktop.Api]::ModulePaths($officeProcess.Id) -contains $native)){throw 'The registered native DLL is not loaded.'}
$largeId=$state.views[0].id
if($HostName -eq 'Word'){$documents=$app.Documents;$smallDocument=$documents.Open($small)}else{$documents=$app.Workbooks;$smallDocument=$documents.Open($small)}
Start-Sleep -Seconds 1
function Wait-Selection($id,$path){
 $until=[DateTime]::UtcNow.AddSeconds(20)
 do {
  $s=$api.GetState()|ConvertFrom-Json
  $active=if($HostName -eq 'Word'){$app.ActiveDocument}else{$app.ActiveWorkbook}
  if($s.activeId -eq $id -and $active.FullName -eq $path){return}
  Start-Sleep -Milliseconds 100
 }while([DateTime]::UtcNow -lt $until)
 throw 'Native tab selection and displayed document disagree.'
}
$state=$api.GetState()|ConvertFrom-Json
if($state.views.Count -ne 2){throw 'The second document did not receive a tab.'}
$smallId=($state.views|Where-Object id -ne $largeId).id
foreach($iteration in 1..3){$api.Activate($largeId)|Out-Null;Wait-Selection $largeId $large;$api.Activate($smallId)|Out-Null;Wait-Selection $smallId $small}
$memory=(Get-Process -Id $officeProcess.Id).WorkingSet64
if(-not $api.Close($smallId)){throw 'Small test document close was rejected.'}
Start-Sleep -Seconds 1
if(-not $api.Close($largeId)){throw 'Large test document close was rejected.'}
if((Get-FileHash -LiteralPath $large).Hash -ne $before){throw 'Stress fixture was modified.'}
@{Host=$HostName;Architecture=$Architecture;OfficePid=$officeProcess.Id;OfficeStartUtc=$officeProcess.StartTime.ToUniversalTime().ToString('o');Bytes=(Get-Item $large).Length;FixtureSha256=$before;LoadedSha256=(Get-FileHash $native).Hash;OpenSeconds=[Math]::Round($openedSeconds,2);WorkingSetBytes=$memory;SelectionRounds=3;NativeSelectionMatchesContent=$true;FixtureUnchanged=$true;Passed=$true;Boundary='Command-driven selection; does not measure manual drag smoothness.'}|ConvertTo-Json|Set-Content (Join-Path $root "artifacts/stress-runtime-$HostName.json") -Encoding UTF8
