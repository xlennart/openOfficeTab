param([string[]]$HostNames=@('Word','Excel','PowerPoint'),[string]$ExpectedVersion='0.1.6')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
$taskApis=@()
foreach($taskHost in $HostNames){
 if($taskHost -notin @('Word','Excel','PowerPoint')){throw 'Unsupported test host.'}
 $taskFile=if($taskHost -eq 'Word'){'word-demo.docx'}elseif($taskHost -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
 $taskPath=Join-Path $taskRoot ('tests\fixtures\'+$taskFile)
 if($taskHost -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($taskHost);$taskDoc=$taskApp.Presentations.Item(1)}else{$taskDoc=[OfficeRot]::AttachFile($taskPath);$taskApp=$taskDoc.Application}
 $taskCount=if($taskHost -eq 'Word'){$taskApp.Documents.Count}elseif($taskHost -eq 'Excel'){$taskApp.Workbooks.Count}else{$taskApp.Presentations.Count}
 if($taskCount -ne 1 -or $taskDoc.FullName -ne $taskPath -or -not $taskDoc.Saved){throw 'Unique saved public fixtures required; user files preserved.'}
 $taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object;$s=$taskApi.GetState()|ConvertFrom-Json
 if($s.version -ne $ExpectedVersion -or -not $s.group){throw 'Current candidate with single-taskbar enabled required.'}
 $taskApis+=,$taskApi
}
if($taskApis.Count -lt 2){throw 'At least two Office processes required.'}
try{
 $taskApis[0].ToggleGroup()
 Start-Sleep -Milliseconds 1800
 foreach($api in $taskApis){$s=$api.GetState()|ConvertFrom-Json;if($s.group){throw 'Idle disable synchronization failed.'}}
 # Change a second process, not the original writer, to catch stale toggles.
 $taskApis[1].ToggleGroup()
 Start-Sleep -Milliseconds 1800
 foreach($api in $taskApis){$s=$api.GetState()|ConvertFrom-Json;if(-not $s.group -or $s.diagnostics.settingsWritePending){throw 'Idle enable synchronization or persistence failed.'}}
 [pscustomobject]@{Hosts=$HostNames;IdleDisableSync=$true;EnableFromSecondProcess=$true;WritePending=$false}|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot 'artifacts\taskbar-sync-v016.json')
}finally{
 $taskApis[0].SyncSettings()|Out-Null
 if(-not ($taskApis[0].GetState()|ConvertFrom-Json).group){$taskApis[0].ToggleGroup()}
}
