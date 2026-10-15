param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
$taskName=if($HostName -eq 'Word'){'word-demo.docx'}elseif($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$taskFixturePath=Join-Path $taskRoot ('tests\fixtures\'+$taskName)
if($HostName -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($HostName)}else{$taskApp=([OfficeRot]::AttachFile($taskFixturePath)).Application}
$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
$taskExistingCount=if($HostName -eq 'Word'){$taskApp.Documents.Count}elseif($HostName -eq 'Excel'){$taskApp.Workbooks.Count}else{$taskApp.Presentations.Count}
if($taskExistingCount -eq 1){
 $taskOnly=if($HostName -eq 'Word'){$taskApp.Documents.Item(1)}elseif($HostName -eq 'Excel'){$taskApp.Workbooks.Item(1)}else{$taskApp.Presentations.Item(1)}
 if($taskOnly.FullName -ne $taskFixturePath -or -not [bool]$taskOnly.Saved){throw 'Only fixture must be saved; preserved.'}
 $taskApi.NewDocument();Start-Sleep -Milliseconds 200
}
$taskDocuments=if($HostName -eq 'Word'){@($taskApp.Documents)}elseif($HostName -eq 'Excel'){@($taskApp.Workbooks)}else{@($taskApp.Presentations)}
if($taskDocuments.Count -ne 2 -or @($taskDocuments|Where-Object FullName -eq $taskFixturePath).Count -ne 1){throw 'Expected only public fixture and blank sentinel; preserved.'}
$taskBlank=$taskDocuments|Where-Object Path -eq ''
if(@($taskBlank).Count -ne 1){throw 'Unique unsaved blank required.'}
$taskEmpty=if($HostName -eq 'Word'){$taskBlank.Content.Text.Trim().Length -eq 0}elseif($HostName -eq 'Excel'){@($taskBlank.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2}).Count -eq 0}else{@($taskBlank.Slides|ForEach-Object {$_.Shapes}|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0}
if(-not $taskEmpty){throw 'Sentinel has content; preserved.'}
$taskBlank.Windows.Item(1).Activate();$taskApi.Refresh()|Out-Null
$s=$taskApi.GetState()|ConvertFrom-Json
if($s.version -ne '0.1.5'){throw 'Expected version missing.'}
$taskId=$s.activeId
if(-not $taskApi.SetViewWidth($taskId,176) -or -not $taskApi.SetIndependent($taskId,$true)){throw 'Setup failed.'}
$taskExtension=[IO.Path]::GetExtension($taskFixturePath)
$taskPaths=@((Join-Path $taskRoot ('artifacts\first-save-'+[guid]::NewGuid().ToString('N')+$taskExtension)),(Join-Path $taskRoot ('artifacts\save-as-'+[guid]::NewGuid().ToString('N')+$taskExtension)))
foreach($taskPath in $taskPaths){
 if(Test-Path -LiteralPath $taskPath){throw 'Unexpected existing file; preserved.'}
 if($HostName -eq 'Word'){[object]$taskSavePath=[string]$taskPath;[object]$taskFormat=[int]12;$taskBlank.SaveAs2([ref]$taskSavePath,[ref]$taskFormat)}elseif($HostName -eq 'Excel'){$taskBlank.SaveAs($taskPath,51)}else{$taskBlank.SaveAs($taskPath,24)}
 if($taskBlank.FullName -ne $taskPath -or -not [bool]$taskBlank.Saved){throw 'Save not confirmed.'}
 $taskApi.Refresh()|Out-Null
 if(-not $taskApi.Close($taskId)){throw 'Close rejected.'}
 Start-Sleep -Milliseconds 150
 $taskFiles=(ConvertFrom-Json -InputObject ($taskApi.GetRecentlyClosed()));$taskIndex=[array]::IndexOf($taskFiles,$taskPath)
 if($taskIndex -lt 0 -or -not $taskApi.ReopenClosed($taskIndex)){throw 'Saved path not reopenable.'}
 $taskApi.Refresh()|Out-Null;$s=$taskApi.GetState()|ConvertFrom-Json;$taskId=$s.activeId;$taskView=$s.views|Where-Object id -eq $taskId
 if($taskView.manualTabWidth -ne 176 -or -not $taskView.independent){throw 'First save / Save As preference lost.'}
 $taskBlank=if($HostName -eq 'Word'){$taskApp.ActiveDocument}elseif($HostName -eq 'Excel'){$taskApp.ActiveWorkbook}else{$taskApp.ActivePresentation}
 if($taskBlank.FullName -ne $taskPath){throw 'Unexpected reopened file; preserved.'}
}
if(-not $taskApi.ResetFileSettings($taskId)){throw 'Test file reset failed.'}
if(-not [bool]$taskBlank.Saved -or $taskBlank.FullName -ne $taskPaths[1]){throw 'Modified test file preserved.'}
if(-not $taskApi.Close($taskId)){throw 'Test file close failed.'}
[pscustomobject]@{Host=$HostName;FirstSaveAndSaveAsInherited=$true;ReopenedWidth=176;ReopenedIndependent=$true;OnlyPublicFixtureLeftOpen=$true}|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot ('artifacts\first-save-runtime-v015-'+$HostName+'.json'))
