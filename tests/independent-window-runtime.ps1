param([ValidateSet('Word','Excel','PowerPoint')][string[]]$Hosts=@('Word','Excel','PowerPoint'),[string]$OutputPath=(Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts\independent-window-runtime.json'))
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path (Join-Path $taskRoot 'artifacts') -Force | Out-Null
. (Join-Path $PSScriptRoot 'common.ps1')
$taskResults=@()
function Wait-TaskState($TaskApi,[scriptblock]$TaskCheck){
    $taskWatch=[Diagnostics.Stopwatch]::StartNew()
    do{$TaskApi.Refresh()|Out-Null;$taskState=$TaskApi.GetState()|ConvertFrom-Json;if(& $TaskCheck $taskState){return $taskState};Start-Sleep -Milliseconds 30}while($taskWatch.ElapsedMilliseconds -lt 4000)
    throw ('State did not converge: '+($taskState|ConvertTo-Json -Depth 8 -Compress))
}
foreach($taskPair in @(@('Word','word-demo.docx'),@('Excel','excel-demo.xlsx'),@('PowerPoint','powerpoint-demo.pptx'))){
    if($taskPair[0] -notin $Hosts){continue}
    $taskCreated=$null;$taskApi=$null;$taskGroupChanged=$false;$taskBaseId=0;$taskNewId=0
    $taskMarker='openOfficetab independent-window unsaved test'
    try{
        $taskFixturePath=Join-Path $taskRoot ('tests\fixtures\'+$taskPair[1])
        $taskFixture=[OfficeRot]::AttachFile($taskFixturePath)
        if($taskFixture.FullName -ne $taskFixturePath){throw 'Unexpected fixture; preserve all documents.'}
        $taskApp=$taskFixture.Application
        switch($taskPair[0]){'Word'{$taskCollection=$taskApp.Documents};'Excel'{$taskCollection=$taskApp.Workbooks};'PowerPoint'{$taskCollection=$taskApp.Presentations}}
        if($taskCollection.Count -ne 1){throw 'Other documents in this instance; preserved.'}
        $taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
        $taskBefore=Wait-TaskState $taskApi {param($s)$s.views.Count -eq 1 -and $s.paneMetrics[0].displayVisible}
        if($taskBefore.version -ne '0.1.2'){throw 'Wrong add-in version loaded.'}
        $taskBaseId=$taskBefore.views[0].id
        $taskApi.NewDocument()
        $taskAdded=Wait-TaskState $taskApi {param($s)$s.views.Count -eq 2 -and @($s.paneMetrics|Where-Object {$_.width -le 0 -or -not $_.displayVisible}).Count -eq 0}
        $taskNewId=($taskAdded.views|Where-Object id -ne $taskBaseId).id
        $taskNewRoot=($taskAdded.views|Where-Object id -eq $taskNewId).hwnd
        $taskCreated=switch($taskPair[0]){'Word'{$taskApp.ActiveDocument};'Excel'{$taskApp.ActiveWorkbook};'PowerPoint'{$taskApp.ActivePresentation}}
        switch($taskPair[0]){'Word'{$taskCreated.Content.Text=$taskMarker};'Excel'{$taskCreated.Worksheets.Item(1).Range('A1').Value2=$taskMarker};'PowerPoint'{$taskCreated.Slides.Item(1).Shapes.Item(1).TextFrame.TextRange.Text=$taskMarker}}
        if(-not $taskBefore.group){$taskApi.ToggleGroup();$taskGroupChanged=$true}
        $taskApi.Activate($taskBaseId)
        if(-not $taskApi.SetIndependent($taskNewId,$true)){throw 'Detach command failed.'}
        $taskDetached=Wait-TaskState $taskApi {param($s)$v=$s.views|Where-Object id -eq $taskNewId;$p=$s.paneMetrics|Where-Object viewId -eq $taskNewId;$s.tabCount -eq 1 -and $v.independent -and -not $p.officeVisible -and -not $p.displayVisible}
        $taskView=$taskDetached.views|Where-Object id -eq $taskNewId
        if($taskView.hwnd -ne $taskNewRoot -or $taskView.saved -or $taskView.taskbarRemoved -or $taskView.moved){throw 'Document state or independent taskbar behavior changed.'}
        # Neither batch command may touch an independent unsaved document.
        $taskApi.SaveAll();$taskApi.CloseOther($taskBaseId)
        $taskExcluded=Wait-TaskState $taskApi {param($s)$s.views.Count -eq 2 -and ($s.views|Where-Object id -eq $taskNewId).independent}
        if($taskCreated.Saved){throw 'Independent unsaved document was saved by group operation.'}
        $taskApi.Activate($taskNewId)
        if(-not $taskApi.JoinTabs()){throw 'Ribbon rejoin command failed.'}
        $taskRejoined=Wait-TaskState $taskApi {param($s)$v=$s.views|Where-Object id -eq $taskNewId;$p=$s.paneMetrics|Where-Object viewId -eq $taskNewId;$s.tabCount -eq 2 -and -not $v.independent -and $p.officeVisible -and $p.displayVisible -and $p.width -gt 0}
        if(($taskRejoined.views|Where-Object id -eq $taskNewId).hwnd -ne $taskNewRoot){throw 'Rejoin replaced the Office window.'}
        $taskApi.SetIndependent($taskBaseId,$true)|Out-Null;$taskApi.SetIndependent($taskNewId,$true)|Out-Null
        $taskEmpty=Wait-TaskState $taskApi {param($s)$s.tabCount -eq 0 -and @($s.paneMetrics|Where-Object {$_.officeVisible -or $_.displayVisible}).Count -eq 0}
        if(-not $taskApi.JoinTabs()){throw 'Rejoin from empty tab group failed.'}
        $taskApi.SetIndependent($taskBaseId,$false)|Out-Null
        $taskApi.SetIndependent($taskNewId,$true)|Out-Null
        switch($taskPair[0]){'Word'{$taskCreated.Close(0)};'Excel'{$taskCreated.Close($false)};'PowerPoint'{$taskCreated.Close()}}
        $taskCreated=$null
        $taskClosed=Wait-TaskState $taskApi {param($s)$s.views.Count -eq 1 -and $s.tabCount -eq 1 -and $s.views[0].id -eq $taskBaseId}
        $taskResults+=[pscustomobject]@{Host=$taskPair[0];Passed=$true;UnsavedDocumentPreserved=$true;BatchCommandsExcludeIndependent=$true;RejoinWorks=$true;EmptyGroupRecovery=$true;CloseIndependentWorks=$true;Detached=$taskDetached;Rejoined=$taskRejoined;Empty=$taskEmpty;Closed=$taskClosed;MenuVisualAcceptance='User verification required'}
    }catch{$taskResults+=[pscustomobject]@{Host=$taskPair[0];Passed=$false;Error=$_.Exception.Message}}
    finally{
        if($taskApi){try{if($taskBaseId){$taskApi.SetIndependent($taskBaseId,$false)|Out-Null};if($taskGroupChanged){$taskApi.ToggleGroup()}}catch{}}
        if($taskCreated){Write-Warning 'Test interrupted; created document preserved for inspection.'}
    }
}
$taskResults|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $OutputPath -Encoding utf8
$taskResults|Select-Object Host,Passed,UnsavedDocumentPreserved,BatchCommandsExcludeIndependent,RejoinWorks,EmptyGroupRecovery,Error|Format-Table
foreach($taskResult in $taskResults){$taskResult|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $taskRoot ('artifacts\independent-'+$taskResult.Host+'.json')) -Encoding utf8}
if(@($taskResults|Where-Object {-not $_.Passed}).Count){throw 'Independent window check failed; see artifact.'}
