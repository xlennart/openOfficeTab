$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
$taskApis=@();$taskBefore=@()
foreach($taskHost in 'Word','Excel','PowerPoint'){
 if($taskHost -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($taskHost);if($taskApp.Presentations.Count -ne 1 -or $taskApp.Presentations.Item(1).FullName -ne (Join-Path $taskRoot 'tests\fixtures\powerpoint-demo.pptx')){throw 'PPT fixture guard failed.'}}
 else{$taskFile=if($taskHost -eq 'Word'){'word-demo.docx'}else{'excel-demo.xlsx'};$taskApp=([OfficeRot]::AttachFile((Join-Path $taskRoot ('tests\fixtures\'+$taskFile)))).Application}
 $taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object;$s=$taskApi.GetState()|ConvertFrom-Json
 if($s.version -ne '0.1.5' -or $s.views.Count -ne 1){throw 'Expected unique fixture and current version.'}
 $taskApis+=,$taskApi;$taskBefore+=,$s
}
$taskOriginal=$taskBefore[0].diagnostics
try{
 $taskApis[0].SetBarColor('#E8EAED')|Out-Null
 $taskApis[0].SetTabWidth(160,0)|Out-Null
 $taskApis[0].SetAppearance(0,1)|Out-Null
 # GetState does not force settings synchronization. Exercise idle timers.
 Start-Sleep -Milliseconds 1800
 $taskResult=@()
 for($i=0;$i -lt 3;$i++){
  $s=$taskApis[$i].GetState()|ConvertFrom-Json;$d=$s.diagnostics
  if($d.barColor -ne '#E8EAED' -or $d.tabWidth -ne 160 -or $d.accentPosition -ne 0){throw 'Idle settings sync failed.'}
  if($s.paneMetrics[0].controlHwnd -ne $taskBefore[$i].paneMetrics[0].controlHwnd){throw 'Synchronization recreated control.'}
  $taskResult+=[pscustomobject]@{Host=$s.host;LiveAppearanceColorWidthSync=$true;ControlHandleStable=$true}
 }
 $taskResult|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot 'artifacts\settings-sync-runtime-v015.json')
}finally{
 $taskApis[0].SetBarColor($(if($taskOriginal.customBarColor){$taskOriginal.barColor}else{''}))|Out-Null
 $taskApis[0].SetTabWidth($taskOriginal.tabWidth,$taskOriginal.widthMode)|Out-Null
 $taskApis[0].SetAppearance($taskOriginal.accentPosition,$taskOriginal.barStyle)|Out-Null
}
