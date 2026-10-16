param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName,[switch]$CloseForRestart,[switch]$VerifyRestart,[switch]$UseExistingBlank,[switch]$ContinueAfterRecovery,[switch]$KeepCreatedBlank,[string]$ExpectedVersion='0.1.6')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
$taskFile=if($HostName -eq 'Word'){'word-demo.docx'}elseif($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$taskPath=Join-Path $taskRoot ('tests\fixtures\'+$taskFile)
$taskReady=$false;$taskDeadline=[DateTime]::UtcNow.AddSeconds(10)
do{
 try{
  if($HostName -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($HostName);$taskFixture=$taskApp.Presentations.Item(1)}else{$taskFixture=[OfficeRot]::AttachFile($taskPath);$taskApp=$taskFixture.Application}
  $taskReady=$null -ne $taskApp -and $null -ne $taskFixture -and $null -ne $taskApp.COMAddIns.Item('openOfficetab.Connect').Object
 }catch{$taskReady=$false}
 if(-not $taskReady){Start-Sleep -Milliseconds 200}
}while(-not $taskReady -and [DateTime]::UtcNow -lt $taskDeadline)
if(-not $taskReady){throw 'Fixture or add-in is not ready; no document operations performed.'}
$taskCount=if($HostName -eq 'Word'){$taskApp.Documents.Count}elseif($HostName -eq 'Excel'){$taskApp.Workbooks.Count}else{$taskApp.Presentations.Count}
if($UseExistingBlank -and $HostName -ne 'PowerPoint'){throw 'Existing-blank continuation is only implemented for PowerPoint.'}
if($taskCount -ne $(if($UseExistingBlank){2}else{1}) -or $taskFixture.FullName -ne $taskPath -or -not $taskFixture.Saved -or $taskApp.ProtectedViewWindows.Count -ne 0){throw 'Unique saved public fixture required; user documents preserved.'}
$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
function Require([bool]$Passed,[string]$Message){if(-not $Passed){throw $Message}}
function State {
 $deadline=[DateTime]::UtcNow.AddSeconds(5)
 do{
  try{$taskApi.Refresh()|Out-Null;return ($taskApi.GetState()|ConvertFrom-Json)}catch{
   if($_.Exception.HResult -notin @(-2147418111,-2147417846) -or [DateTime]::UtcNow -ge $deadline){throw}
   Start-Sleep -Milliseconds 200
  }
 }while($true)
}
function Wait-Views([int]$Count){
 $deadline=[DateTime]::UtcNow.AddSeconds(6)
 do{$s=State;if($s.views.Count -eq $Count -and $s.paneMetrics.Count -eq $Count){return $s};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $deadline)
 throw 'Expected views did not settle.'
}
function Check-Grouped {
 $s=State
 Require $s.group 'Single-taskbar mode was lost.'
 Require (@($s.views|Where-Object taskbarRemoved).Count -eq 1) 'Inactive taskbar entry was not removed.'
 Require (-not ($s.views|Where-Object id -eq $s.activeId).taskbarRemoved) 'Active taskbar entry missing.'
 return $s
}
$taskInitial=State
Require ($taskInitial.version -eq $ExpectedVersion) 'Old DLL loaded; test stopped.'
if($ContinueAfterRecovery){Require ($taskInitial.diagnostics.lastOperation -eq 'recovery.completed' -and $taskInitial.group) 'Previous separate recovery did not complete or retain mode.'}
if($VerifyRestart){Require $taskInitial.group 'Mode was not retained after restarting the application.'}
if(-not $taskInitial.group){$taskApi.ToggleGroup()}
Require (State).group 'Enabling single-taskbar mode failed.'
if(-not $UseExistingBlank){$taskApi.NewDocument()};$s=Wait-Views 2
if($HostName -eq 'Word'){$taskNew=$taskApp.ActiveDocument}elseif($HostName -eq 'Excel'){$taskNew=$taskApp.ActiveWorkbook}elseif($UseExistingBlank){$taskNew=$taskApp.Presentations.Item(2)}else{$taskNew=$taskApp.ActivePresentation}
Require ($taskNew.Path -eq '') 'Created document was unexpected; preserved.'
$taskNewId=if($UseExistingBlank){($s.views|Where-Object {$_.title -eq $taskNew.Name}).id}else{$s.activeId}
Require ($null -ne $taskNewId -and @($taskNewId).Count -eq 1) 'Unique created blank document required.'
$taskFixtureId=($s.views|Where-Object id -ne $taskNewId).id
Check-Grouped|Out-Null
foreach($id in @($taskFixtureId,$taskNewId,$taskFixtureId,$taskNewId)){
 $taskApi.Activate([long]$id);Start-Sleep -Milliseconds 800;$s=Check-Grouped
 Require ($s.activeId -eq $id) ("Tab activation failed: requested="+$id+" actual="+$s.activeId+" live="+($s.views.id -join ","))
 foreach($v in $s.views){Require ([OfficeLayoutProbe]::RibbonVisible([long]$v.hwnd)) 'Office ribbon disappeared.'}
}
Require ($taskApi.SetIndependent($taskNewId,$true)) 'Detach rejected.'
Require (@((State).views|Where-Object taskbarRemoved).Count -eq 0) 'Independent window has no taskbar entry.'
Require ($taskApi.SetIndependent($taskNewId,$false)) 'Rejoin rejected.'
Check-Grouped|Out-Null
$taskApi.RestoreWindows();Check-Grouped|Out-Null
if(-not $ContinueAfterRecovery){Require ($taskApi.Recover()) 'Pane recovery failed.'}
Check-Grouped|Out-Null
$taskApi.ToggleGroup();Require (-not (State).group) 'Explicit opt-out failed.'
Require (@((State).views|Where-Object taskbarRemoved).Count -eq 0) 'Opt-out did not restore taskbar entries.'
$taskApi.ToggleGroup();Check-Grouped|Out-Null
try{
 $taskApi.SetDark($true);$s=State
 Require ($s.diagnostics.dark -and $s.diagnostics.barBackground -eq '#262626' -and $s.diagnostics.surroundBackground -eq '#262626') 'Dark surfaces differ.'
}finally{$taskApi.SetDark([bool]$taskInitial.diagnostics.dark)}
# Only the blank document created above may be closed without a save prompt.
$taskBlank=if($HostName -eq 'Word'){$taskNew.Content.Text.Trim().Length -eq 0}elseif($HostName -eq 'Excel'){@($taskNew.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2}).Count -eq 0}else{$taskNew.Slides.Count -eq 0 -or ($taskNew.Slides.Count -eq 1 -and @($taskNew.Slides.Item(1).Shapes|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0)}
Require ($taskBlank -and $taskNew.Path -eq '') 'Created document changed; preserved.'
if($HostName -eq 'PowerPoint'){$taskNew.Saved=-1}
if(-not $KeepCreatedBlank){Require ($taskApi.Close($taskNewId)) 'Blank test document close failed.';$s=Wait-Views 1}else{$s=State}
Require $s.group 'Mode lost after document operations.'
$taskResult=[pscustomobject]@{Host=$HostName;Version=$s.version;AfterRestart=[bool]$VerifyRestart;ModeRetained=$true;TaskbarCallsFollowActive=$true;Switches=4;IndependentEntryRestored=$true;RecoveryPreservesMode=$true;DarkSurfacesEqual=$true;VisualAcceptance='User review pending'}
if($CloseForRestart){
 Require (-not $KeepCreatedBlank) 'Restart cleanup requires closing the created blank first.'
 Require ($taskFixture.FullName -eq $taskPath -and $taskFixture.Saved) 'Fixture changed; preserved.'
 Require ($taskApi.Close($s.activeId)) 'Last saved fixture close failed.'
}
$taskResult|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot ('artifacts\single-taskbar-'+$HostName+$(if($VerifyRestart){'-restart'}else{'-initial'})+'.json'))
