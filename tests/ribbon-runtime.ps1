param([ValidateSet('Excel','PowerPoint')][string]$HostName,[string]$ExpectedVersion='0.1.4',[int]$Cycles=3,[switch]$UseExistingFixture)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
$taskProcess=if($HostName -eq 'Excel'){'EXCEL'}else{'POWERPNT'}
if((Get-Process $taskProcess -ErrorAction SilentlyContinue) -and -not $UseExistingFixture){throw 'Host already running; existing documents preserved.'}
. (Join-Path $PSScriptRoot 'window-probe.ps1')
$taskName=if($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$taskFixturePath=Join-Path $taskRoot ('tests\fixtures\'+$taskName)
if(-not $UseExistingFixture){Start-Process -FilePath $taskFixturePath -WindowStyle Normal}
$taskDeadline=[DateTime]::UtcNow.AddSeconds(15)
do{try{if($HostName -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($HostName);$taskFixture=$taskApp.Presentations.Item(1)}else{$taskFixture=[OfficeRot]::AttachFile($taskFixturePath);$taskApp=$taskFixture.Application};$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object;$taskState=$taskApi.GetState()|ConvertFrom-Json}catch{};if(-not $taskState){Start-Sleep -Milliseconds 150}}while(-not $taskState -and [DateTime]::UtcNow -lt $taskDeadline)
if(-not $taskState -or $taskState.version -ne $ExpectedVersion){throw 'Expected add-in version unavailable.'}
$taskGuardDeadline=[DateTime]::UtcNow.AddSeconds(6)
do{$taskCount=if($HostName -eq 'Excel'){$taskApp.Workbooks.Count}else{$taskApp.Presentations.Count};$taskGuard=$taskCount -eq 1 -and $taskFixture.FullName -eq $taskFixturePath -and [bool]$taskFixture.Saved;if(-not $taskGuard){Start-Sleep -Milliseconds 100}}while(-not $taskGuard -and [DateTime]::UtcNow -lt $taskGuardDeadline)
if(-not $taskGuard){throw 'Fixture guard failed.'}
function Wait-RibbonPanes([int]$Count){
 $limit=[DateTime]::UtcNow.AddSeconds(6)
 do{$taskApi.Refresh()|Out-Null;$s=$taskApi.GetState()|ConvertFrom-Json
  if($s.views.Count -eq $Count -and $s.paneMetrics.Count -eq $Count -and @($s.paneMetrics|Where-Object {-not $_.displayVisible -or -not $_.matchesReservedRect}).Count -eq 0){return $s};Start-Sleep -Milliseconds 100
 }while([DateTime]::UtcNow -lt $limit)
 throw 'Pane layout did not stabilize.'
}
Wait-RibbonPanes 1|Out-Null
$taskChecks=0
for($taskCycle=0;$taskCycle -lt $Cycles;$taskCycle++){
 $taskApi.NewDocument();$s=Wait-RibbonPanes 2
 $taskCreated=if($HostName -eq 'Excel'){$taskApp.ActiveWorkbook}else{$taskApp.ActivePresentation}
 if($taskCreated.Path -ne '' -or $taskCreated.FullName -eq $taskFixturePath){throw 'Unexpected created document; preserved.'}
 $taskBlank=if($HostName -eq 'Excel'){$taskCreated.Worksheets.Count -gt 0 -and @($taskCreated.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2}).Count -eq 0}else{$taskCreated.Slides.Count -eq 0 -or ($taskCreated.Slides.Count -eq 1 -and @($taskCreated.Slides.Item(1).Shapes|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0)}
 if(-not $taskBlank){throw 'Created document has content; preserved.'}
 foreach($v in $s.views){if(-not [OfficeLayoutProbe]::RibbonVisible([long]$v.hwnd)){throw 'Ribbon hidden after native new document.'};$taskChecks++}
 if($HostName -eq 'Excel'){$taskCreated.Close($false)}else{$taskCreated.Saved=-1;$taskCreated.Close()}
 Wait-RibbonPanes 1|Out-Null
}
$taskApi.NewDocument();$s=Wait-RibbonPanes 2
$taskNew=if($HostName -eq 'Excel'){$taskApp.ActiveWorkbook}else{$taskApp.ActivePresentation}
if($taskNew.Path -ne '' -or $taskNew.FullName -eq $taskFixturePath){throw 'Unexpected new document; preserved.'}
$taskTargetId=$s.activeId;$taskTarget=($s.views|Where-Object id -eq $taskTargetId);$taskSource=($s.views|Where-Object id -ne $taskTargetId)
$taskSourceWindow=$taskFixture.Windows.Item(1);$taskTargetWindow=$taskNew.Windows.Item(1)
$taskSlot=[OfficeLayoutProbe]::WorkArea([long]$taskSource.hwnd);$taskSlot.Right=$taskSlot.Left+[int](($taskSlot.Right-$taskSlot.Left)/2)
$taskPreviousGroup=$s.group;if($taskPreviousGroup){$taskApi.ToggleGroup()}
$taskSwitches=0
for($taskSwitch=0;$taskSwitch -lt 6;$taskSwitch++){
 $taskTargetWindow.WindowState=if($HostName -eq 'Excel'){-4137}else{3}
 $taskSourceWindow.Activate();$taskSourceWindow.WindowState=if($HostName -eq 'Excel'){-4143}else{1};[OfficeLayoutProbe]::Place([long]$taskSource.hwnd,$taskSlot)
 $taskApi.Refresh()|Out-Null;$taskApi.Activate($taskTargetId);$s=Wait-RibbonPanes 2
 if($s.activeId -ne $taskTargetId -or -not [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds([long]$taskSource.hwnd),[OfficeLayoutProbe]::Bounds([long]$taskTarget.hwnd))){throw 'Half-width-to-maximized switch failed.'}
 $taskApi.Activate($taskSource.id);$s=Wait-RibbonPanes 2
 if($s.activeId -ne $taskSource.id -or -not [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds([long]$taskSource.hwnd),[OfficeLayoutProbe]::Bounds([long]$taskTarget.hwnd))){throw 'Reverse tab switch failed.'}
 foreach($v in $s.views){if(-not [OfficeLayoutProbe]::RibbonVisible([long]$v.hwnd)){throw 'Ribbon hidden after tab switch.'};$taskChecks++};$taskSwitches+=2
}
if($taskPreviousGroup){$taskApi.ToggleGroup()}
$taskSourceWindow.WindowState=if($HostName -eq 'Excel'){-4137}else{3}
$taskResult=[pscustomobject]@{Host=$HostName;Version=$taskState.version;NativeNewDocumentCycles=$Cycles+1;RibbonVisibleChecks=$taskChecks;TabSwitches=$taskSwitches;LayoutsStable=$true;BlankWindowsLeftOpen=2;VisualAcceptance='User verification pending'}
$taskResult|ConvertTo-Json|Set-Content (Join-Path $taskRoot ('artifacts\ribbon-runtime-v014-'+$HostName+'.json')) -Encoding utf8
$taskResult|ConvertTo-Json
