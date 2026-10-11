param([string]$OutputPath=(Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts\new-document-runtime.json'))
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path (Join-Path $taskRoot 'artifacts') -Force | Out-Null
. (Join-Path $PSScriptRoot 'common.ps1')
$taskResults=@()
foreach($taskPair in @(@('Word','word-demo.docx'),@('Excel','excel-demo.xlsx'),@('PowerPoint','powerpoint-demo.pptx'))){
    $taskCreated=$null
    try {
        $taskFixturePath=Join-Path $taskRoot ('tests\fixtures\'+$taskPair[1])
        $taskFixture=[OfficeRot]::AttachFile($taskFixturePath)
        if($taskFixture.FullName -ne $taskFixturePath){throw 'Unexpected fixture; preserve all documents.'}
        $taskApp=$taskFixture.Application
        switch($taskPair[0]){'Word'{$taskCollection=$taskApp.Documents};'Excel'{$taskCollection=$taskApp.Workbooks};'PowerPoint'{$taskCollection=$taskApp.Presentations}}
        if($taskCollection.Count -ne 1){throw 'Project fixture instance contains other documents; preserve them.'}
        $taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
        $taskApi.Refresh()|Out-Null
        $taskBefore=$taskApi.GetState()|ConvertFrom-Json
        if($taskBefore.views.Count -ne 1 -or -not $taskBefore.paneMetrics[0].reserveFrameHwnd){throw 'Fixture tab is not ready.'}
        $taskWatch=[Diagnostics.Stopwatch]::StartNew()
        $taskApi.NewDocument()
        $taskCommandMs=$taskWatch.ElapsedMilliseconds
        do {
            $taskApi.Refresh()|Out-Null
            $taskAfter=$taskApi.GetState()|ConvertFrom-Json
            $taskReady=$taskAfter.views.Count -eq 2 -and $taskAfter.paneMetrics.Count -eq 2 -and @($taskAfter.paneMetrics|Where-Object {-not $_.reserveFrameHwnd -or -not $_.matchesReservedRect -or $_.width -le 0 -or $_.height -le 0}).Count -eq 0
            if(-not $taskReady){Start-Sleep -Milliseconds 20}
        }while(-not $taskReady -and $taskWatch.ElapsedMilliseconds -lt 4000)
        $taskReadyMs=$taskWatch.ElapsedMilliseconds
        $taskCreated=switch($taskPair[0]){'Word'{$taskApp.ActiveDocument};'Excel'{$taskApp.ActiveWorkbook};'PowerPoint'{$taskApp.ActivePresentation}}
        if($taskCreated.FullName -eq $taskFixture.FullName){$taskCreated=$null;throw 'Native new command did not create another document.'}
        $taskStable=$true
        $taskBaseline=$taskAfter|ConvertTo-Json -Depth 10 -Compress
        for($taskI=0;$taskI -lt 5;$taskI++){
            Start-Sleep -Milliseconds 200
            $taskApi.Refresh()|Out-Null
            if(($taskApi.GetState()|ConvertFrom-Json|ConvertTo-Json -Depth 10 -Compress) -ne $taskBaseline){$taskStable=$false}
        }
        $taskOld=$taskAfter.paneMetrics|Where-Object viewId -eq $taskBefore.views[0].id
        $taskResults+=[pscustomobject]@{Host=$taskPair[0];CommandMs=$taskCommandMs;ReadyMs=$taskReadyMs;Ready=$taskReady;Stable=$taskStable;ExistingRendererRetained=($taskOld.controlHwnd -eq $taskBefore.paneMetrics[0].controlHwnd);Before=$taskBefore;After=$taskAfter;VisualFlicker='User verification required'}
    } catch {
        $taskResults+=[pscustomobject]@{Host=$taskPair[0];Error=$_.Exception.Message}
    } finally {
        # Close only the empty document created by this exact test command.
        if($taskCreated){
            $taskBlank=switch($taskPair[0]){'Word'{$taskCreated.Content.Text.Trim().Length -eq 0};'Excel'{$taskCreated.Worksheets.Count -gt 0 -and @($taskCreated.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2}).Count -eq 0};'PowerPoint'{$taskCreated.Slides.Count -eq 0 -or ($taskCreated.Slides.Count -eq 1 -and @($taskCreated.Slides.Item(1).Shapes|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0)}}
            if($taskBlank){switch($taskPair[0]){'Word'{$taskCreated.Close(0)};'Excel'{$taskCreated.Close($false)};'PowerPoint'{$taskCreated.Close()}}}else{Write-Warning 'Test document has content; preserved.'}
        }
    }
}
$taskResults|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $OutputPath -Encoding utf8
$taskResults|Select-Object Host,CommandMs,ReadyMs,Ready,Stable,ExistingRendererRetained,Error|Format-Table
if(@($taskResults|Where-Object {$_.Error -or -not $_.Ready -or -not $_.Stable -or -not $_.ExistingRendererRetained}).Count){throw 'Runtime lifecycle check failed; see output artifact.'}
