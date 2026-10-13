param([string]$ExpectedVersion='0.1.4',[int]$NewDocumentCycles=8)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
if(Get-Process WINWORD -ErrorAction SilentlyContinue){throw 'Word is already running; preserve existing documents.'}
. (Join-Path $PSScriptRoot 'window-probe.ps1')
$taskFixturePath=Join-Path $taskRoot 'tests\fixtures\word-demo.docx'
Start-Process -FilePath $taskFixturePath -WindowStyle Normal
$taskDeadline=[DateTime]::UtcNow.AddSeconds(15)
do{try{$taskFixture=[OfficeRot]::AttachFile($taskFixturePath);$taskApp=$taskFixture.Application;$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object;$taskState=$taskApi.GetState()|ConvertFrom-Json}catch{};if(-not $taskState){Start-Sleep -Milliseconds 150}}while(-not $taskState -and [DateTime]::UtcNow -lt $taskDeadline)
if(-not $taskState -or $taskState.version -ne $ExpectedVersion){throw 'Expected add-in version did not load.'}
if($taskApp.Documents.Count -ne 1 -or $taskFixture.FullName -ne $taskFixturePath -or -not $taskFixture.Saved -or $taskFixture.Content.Text.Trim().Length){throw 'Fixture guard failed; all documents preserved.'}
function Wait-WordState([int]$Count){
 $limit=[DateTime]::UtcNow.AddSeconds(6)
 do{
  $taskApi.Refresh()|Out-Null;$s=$taskApi.GetState()|ConvertFrom-Json
  $ok=$s.views.Count -eq $Count -and $s.paneMetrics.Count -eq $Count -and @($s.paneMetrics|Where-Object {-not $_.matchesReservedRect -or -not $_.displayVisible}).Count -eq 0
  if($ok){return $s};Start-Sleep -Milliseconds 100
 }while([DateTime]::UtcNow -lt $limit)
 throw 'Pane state did not stabilize.'
}
function Assert-BlankTestDocument($Document){if($Document.FullName -eq $taskFixturePath -or $Document.Path -ne '' -or $Document.Content.Text.Trim().Length){throw 'Unexpected/changed test document; preserve it.'}}
$taskBefore=Wait-WordState 1;$taskOriginalGroup=$taskBefore.group
if($taskOriginalGroup){$taskApi.ToggleGroup()}
$taskRibbonChecks=0
for($taskCycle=0;$taskCycle -lt $NewDocumentCycles;$taskCycle++){
 $taskApi.NewDocument();$taskAfter=Wait-WordState 2;$taskNew=$taskApp.ActiveDocument;Assert-BlankTestDocument $taskNew
 foreach($v in $taskAfter.views){if(-not [OfficeLayoutProbe]::RibbonVisible([long]$v.hwnd)){throw 'Ribbon hidden after native new document.'};$taskRibbonChecks++}
 if($taskCycle -lt $NewDocumentCycles-1){$taskNew.Close(0);Wait-WordState 1|Out-Null}
}
$taskSourceWindow=$taskFixture.Windows.Item(1);$taskTargetWindow=$taskNew.Windows.Item(1)
$taskSourceHwnd=[long]$taskSourceWindow.Hwnd;$taskTargetHwnd=[long]$taskTargetWindow.Hwnd
$taskSourceId=($taskAfter.views|Where-Object hwnd -eq $taskSourceHwnd).id;$taskTargetId=($taskAfter.views|Where-Object hwnd -eq $taskTargetHwnd).id
$taskSlot=[OfficeLayoutProbe]::WorkArea($taskSourceHwnd);$taskSlot.Right=$taskSlot.Left+[int](($taskSlot.Right-$taskSlot.Left)/2)
$taskSwitches=0
for($taskSwitch=0;$taskSwitch -lt 6;$taskSwitch++){
 $taskTargetWindow.WindowState=1
 $taskSourceWindow.Activate();$taskSourceWindow.WindowState=0;[OfficeLayoutProbe]::Place($taskSourceHwnd,$taskSlot)
 $taskApi.Refresh()|Out-Null;$taskApi.Activate($taskTargetId)
 $s=Wait-WordState 2
 if($s.activeId -ne $taskTargetId -or -not [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds($taskSourceHwnd),[OfficeLayoutProbe]::Bounds($taskTargetHwnd))){throw 'Narrow-to-maximized tab switch failed.'}
 $taskApi.Activate($taskSourceId);$s=Wait-WordState 2
 if($s.activeId -ne $taskSourceId -or -not [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds($taskSourceHwnd),[OfficeLayoutProbe]::Bounds($taskTargetHwnd))){throw 'Reverse tab switch failed.'}
 foreach($v in $s.views){if(-not [OfficeLayoutProbe]::RibbonVisible([long]$v.hwnd)){throw 'Ribbon hidden after tab switch.'};$taskRibbonChecks++}
 $taskSwitches+=2
}
# Preserve any edited test window and leave both blank tabs open for user review.
Assert-BlankTestDocument $taskNew
if($taskFixture.FullName -ne $taskFixturePath -or -not $taskFixture.Saved){throw 'Fixture changed; preserve all windows.'}
if($taskOriginalGroup){$taskApi.ToggleGroup()}
$taskApp.ActiveWindow.WindowState=1
$taskResult=[pscustomobject]@{Version=$taskState.version;NativeNewDocumentCycles=$NewDocumentCycles;RibbonVisibleChecks=$taskRibbonChecks;HalfScreenAndMaximizedTabSwitches=$taskSwitches;DefaultFollowTested=$true;LayoutsStable=$true;BlankWindowsLeftForReview=2;ActualWindowsSnapAndMouseAcceptance='User verification pending'}
$taskResult|ConvertTo-Json|Set-Content (Join-Path $taskRoot 'artifacts\word-window-runtime-v014.json') -Encoding UTF8
$taskResult|ConvertTo-Json
