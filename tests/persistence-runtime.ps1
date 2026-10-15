param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName,[string]$ExpectedVersion='0.1.5')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
$taskName=if($HostName -eq 'Word'){'word-demo.docx'}elseif($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$taskPath=Join-Path $taskRoot ('tests\fixtures\'+$taskName)
if($HostName -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($HostName);$taskFixture=$taskApp.Presentations.Item(1)}else{$taskFixture=[OfficeRot]::AttachFile($taskPath);$taskApp=$taskFixture.Application}
$taskCount=if($HostName -eq 'Word'){$taskApp.Documents.Count}elseif($HostName -eq 'Excel'){$taskApp.Workbooks.Count}else{$taskApp.Presentations.Count}
if($taskCount -ne 1 -or $taskFixture.FullName -ne $taskPath -or -not [bool]$taskFixture.Saved){throw 'Unique saved public fixture required; existing files preserved.'}
$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
function State { $taskApi.Refresh()|Out-Null;return ($taskApi.GetState()|ConvertFrom-Json) }
function Wait-Views([int]$Count){$limit=[DateTime]::UtcNow.AddSeconds(5);do{$s=State;if($s.views.Count -eq $Count){return $s};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow -lt $limit);throw 'View count did not settle.'}
function Recent-Index { $files=(ConvertFrom-Json -InputObject ($taskApi.GetRecentlyClosed()));for($i=0;$i -lt $files.Count;$i++){if($files[$i] -eq $taskPath){return $i}};throw 'Confirmed close missing from history.' }
function Require([bool]$Passed,[string]$Message){if(-not $Passed){throw $Message}}
$taskInitial=State
Require ($taskInitial.version -eq $ExpectedVersion) 'Expected DLL not loaded.'
$taskInitialId=$taskInitial.activeId
$taskPrefs=$taskInitial.views|Where-Object id -eq $taskInitialId
Require (-not $taskPrefs.independent -and $taskPrefs.manualTabWidth -eq 0) 'Fixture already has file preferences; preserve them.'
$taskOriginal=$taskInitial.diagnostics
$taskApi.NewDocument()
$s=Wait-Views 2
$taskBlank=if($HostName -eq 'Word'){$taskApp.ActiveDocument}elseif($HostName -eq 'Excel'){$taskApp.ActiveWorkbook}else{$taskApp.ActivePresentation}
Require ($taskBlank.Path -eq '' -and $taskBlank.FullName -ne $taskPath) 'Unexpected sentinel document; preserved.'
try {
 Require ($taskApi.SetViewWidth($taskInitialId,222)) 'Manual width rejected.'
 Require ($taskApi.SetTabWidth(180,0)) 'Default width rejected.'
 $s=State;Require (($s.views|Where-Object id -eq $taskInitialId).manualTabWidth -eq 222) 'Default width overwrote file override.'
 Require ($taskApi.SetIndependent($taskInitialId,$true)) 'Independent setting rejected.'
 Require ($taskApi.Close($taskInitialId)) 'Saved fixture close failed.'
 Wait-Views 1|Out-Null
 Require ($taskApi.ReopenClosed((Recent-Index))) 'Reopen failed.'
 $s=Wait-Views 2;$taskReopened=$s.views|Where-Object {$_.id -ne $s.activeId -or $_.independent}|Where-Object manualTabWidth -eq 222|Select-Object -First 1
 Require ($null -ne $taskReopened -and $taskReopened.independent) 'Width or independent preference not restored.'
 $taskId=$taskReopened.id
 Require ($taskApi.ReopenClosed((Recent-Index))) 'Existing-file reopen rejected.'
 Require ((State).views.Count -eq 2) 'Existing-file reopen duplicated document.'
 Require ($taskApi.SetIndependent($taskId,$false)) 'Rejoin failed.'
 Require ($taskApi.Close($taskId)) 'Second fixture close failed.'
 Wait-Views 1|Out-Null
 Require ($taskApi.ReopenClosed((Recent-Index))) 'Grouped reopen failed.'
 $s=Wait-Views 2;$taskId=$s.activeId;$taskView=$s.views|Where-Object id -eq $taskId
 Require (-not $taskView.independent -and $taskView.manualTabWidth -eq 222) 'Joined preference not retained.'
 Require ($taskApi.ResetFileSettings($taskId)) 'File reset failed.'
 Require (((State).views|Where-Object id -eq $taskId).manualTabWidth -eq 0) 'File reset did not remove width.'
 $taskApi.Activate($taskId)
 Require ($taskApi.SetAppearance(1,1)) 'Fused appearance rejected.'
 Start-Sleep -Milliseconds 300
 $s=State;$taskPane=$s.paneMetrics|Where-Object viewId -eq $taskId
 Require ($taskPane.displayVisible -and $taskPane.matchesReservedRect) 'Pane unstable.'
 Require ($taskPane.seamVisible) 'Verified Ribbon seam not mounted.'
 $taskRect=[OfficeLayoutProbe]::Bounds([long]$taskPane.seamHwnd)
 Require (($taskRect.Bottom-$taskRect.Top) -eq [Math]::Max(1,[int](3*$taskPane.dpi/96))) 'Seam thickness incorrect.'
 Require ([OfficeLayoutProbe]::RibbonVisible([long]$taskView.hwnd)) 'Native Ribbon hidden.'
 $taskHandle=$taskPane.controlHwnd
 Require ($taskApi.SetAppearance(1,0)) 'Panel appearance rejected.'
 Start-Sleep -Milliseconds 100
 $s=State;$taskPanel=$s.paneMetrics|Where-Object viewId -eq $taskId
 Require (-not $taskPanel.seamVisible -and $taskPanel.controlHwnd -eq $taskHandle) 'Panel fallback or stable control failed.'
 $taskResult=[pscustomobject]@{Host=$HostName;Version=$ExpectedVersion;SavedCloseReopen=$true;IndependentAndWidthRestored=$true;AlreadyOpenNotDuplicated=$true;RejoinRetained=$true;FileReset=$true;DefaultPreservesOverride=$true;SeamThickness=$taskRect.Bottom-$taskRect.Top;PanelFallback=$true;RibbonVisible=$true;VisualAcceptance='User review pending'}
 $taskResult|ConvertTo-Json|Set-Content (Join-Path $taskRoot ('artifacts\persistence-runtime-v015-'+$HostName+'.json')) -Encoding utf8
 $taskResult|ConvertTo-Json
} finally {
 $taskApi.SetTabWidth($taskOriginal.tabWidth,$taskOriginal.widthMode)|Out-Null
 $taskApi.SetAppearance($taskOriginal.accentPosition,$taskOriginal.barStyle)|Out-Null
 # Keep both blank windows open for review. Never quit the host or clear user history.
}
