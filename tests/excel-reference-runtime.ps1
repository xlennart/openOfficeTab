param([ValidateRange(1,12)][int]$Cycles=6,[string]$FixtureDirectory='', [switch]$LeaveOpen)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $taskRoot 'scripts\desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Cycles=$Cycles;FixtureDirectory=$FixtureDirectory;LeaveOpen=[bool]$LeaveOpen}){return}
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
. (Join-Path $PSScriptRoot 'excel-reference-input.ps1')
function Require([bool]$condition,[string]$message){if(-not $condition){throw $message}}
function Retry([scriptblock]$action){$deadline=[DateTime]::UtcNow.AddSeconds(5);do{try{return (& $action)}catch{if([DateTime]::UtcNow -ge $deadline){throw};Start-Sleep -Milliseconds 80}}while($true)}
if($FixtureDirectory){
 $fixtureDir=[IO.Path]::GetFullPath($FixtureDirectory).TrimEnd('\')
 $allowed=Join-Path $taskRoot 'artifacts\excel-reference-fixtures\'
 Require ($fixtureDir.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or $fixtureDir -eq (Join-Path $taskRoot 'artifacts\formula-fixtures')) 'Only owned reference-test fixtures may be reused.'
}else{
 Require (-not (Get-Process EXCEL -ErrorAction SilentlyContinue)) 'Existing Excel processes preserved. Exit Excel before starting an isolated run.'
 $fixtureDir=Join-Path $taskRoot ('artifacts\excel-reference-fixtures\'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $fixtureDir|Out-Null
 Copy-Item (Join-Path $PSScriptRoot 'fixtures\excel-demo.xlsx') (Join-Path $fixtureDir 'formula-result.xlsx')
 Start-Process -FilePath 'C:\Program Files\Microsoft Office\root\Office16\EXCEL.EXE' -ArgumentList @('/x',('"'+(Join-Path $fixtureDir 'formula-result.xlsx')+'"')) -WindowStyle Normal
}
$doc=Retry {[OfficeRot]::AttachFile((Join-Path $fixtureDir 'formula-result.xlsx'))};$app=Retry {$candidate=$doc.Application;if($null -eq $candidate){throw 'Excel not ready'};$candidate};$books=$app.Workbooks
if(-not $FixtureDirectory){
 Require ($books.Count -eq 1 -and $doc.FullName -eq (Join-Path $fixtureDir 'formula-result.xlsx')) 'New instance fixture guard failed.'
 foreach($n in 1,2){$book=$books.Add();$book.SaveAs((Join-Path $fixtureDir ('source-'+$n+'.xlsx')))}
}
Require ($books.Count -eq 3 -and $app.ProtectedViewWindows.Count -eq 0) 'Only three owned workbooks allowed.'
foreach($book in $books){Require ($book.FullName.StartsWith($fixtureDir+'\',[StringComparison]::OrdinalIgnoreCase)) 'Non-test workbook preserved.'}
$one=$books.Item('source-1.xlsx');$two=$books.Item('source-2.xlsx');$api=$app.COMAddIns.Item('openOfficetab.Connect').Object
foreach($book in @($doc,$one,$two)){$book.Windows.Item(1).WindowState=-4137}
$doc.Activate();Start-Sleep -Milliseconds 400
function State([int]$count=3){Retry {$api.Refresh()|Out-Null;$s=$api.GetState()|ConvertFrom-Json;Require ($s.views.Count -eq $count -and $s.paneMetrics.Count -eq $count) 'Unexpected view/pane count';return $s}}
$s=State
$initialWidths=@{};foreach($view in $s.views){$initialWidths[[long]$view.id]=[int]$view.manualTabWidth;$api.SetViewWidth([long]$view.id,120)|Out-Null}
$s=State;$root=[long]$s.views[0].hwnd;[ReferenceInput]::Bind($root)
$process=Get-Process -Id ([ReferenceInput]::ProcessId)
$modules=@($process.Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
$expectedHash=(Get-FileHash (Join-Path $taskRoot 'dist\x64\openOfficetab.dll')).Hash
Require ($modules.Count -eq 1 -and (Get-FileHash $modules[0].FileName).Hash -eq $expectedHash) 'Loaded test DLL differs from build.'
function Entries($state){
 $result=@();foreach($book in @($doc,$one,$two)){
  $view=@($state.views|Where-Object title -eq $book.Name)[0];$pane=@($state.paneMetrics|Where-Object viewId -eq $view.id)[0]
  Require ($null -ne $view -and $pane.displayVisible -and $pane.matchesReservedRect) 'Owned tab not mounted.'
  $window=$book.Windows.Item(1);$cell=$book.Worksheets.Item(1).Range('A1');$index=@($state.views|ForEach-Object id).IndexOf([int]$view.id)
  $result+=[pscustomobject]@{Id=[long]$view.id;Root=[long]$view.hwnd;Bar=[long]$pane.controlHwnd;TabX=[int]((25+120*$index)*$pane.dpi/96);TabY=[int]($pane.height/2);Dpi=$pane.dpi;CellX=$window.PointsToScreenPixelsX($cell.Left+$cell.Width/2);CellY=$window.PointsToScreenPixelsY($cell.Top+$cell.Height/2)}
 };return $result
}
function Select-References($entries,[bool]$cancel){
 [ReferenceInput]::Raise($entries[0].Root)
 [ReferenceInput]::Text('=');Start-Sleep -Milliseconds 120
 [ReferenceInput]::Client($entries[0].Bar,$entries[1].TabX,$entries[0].TabY);Start-Sleep -Milliseconds 180
 [ReferenceInput]::Screen($entries[1].CellX,$entries[1].CellY);Start-Sleep -Milliseconds 150
 [ReferenceInput]::Text('+');Start-Sleep -Milliseconds 100
 [ReferenceInput]::Client($entries[1].Bar,$entries[2].TabX,$entries[1].TabY);Start-Sleep -Milliseconds 180
 [ReferenceInput]::Screen($entries[2].CellX,$entries[2].CellY);Start-Sleep -Milliseconds 150
 [ReferenceInput]::Press($(if($cancel){27}else{13}));Start-Sleep -Milliseconds 350
}
$checks=0;$rows=@();$values=@(@(1,2),@(-4,2.5),@(0,99),@(1024.25,-1000))
for($cycle=0;$cycle -lt $Cycles;$cycle++){
 $pair=$values[$cycle%$values.Count];$one.Worksheets.Item(1).Range('A1').Value2=[double]$pair[0];$two.Worksheets.Item(1).Range('A1').Value2=[double]$pair[1]
 $doc.Activate();$doc.Worksheets.Item(1).Range('A1').ClearContents();$doc.Worksheets.Item(1).Range('A1').Select()
 $entries=Entries (State);Start-Sleep -Milliseconds 180;Select-References $entries $false
 $row=Retry {$cell=$doc.Worksheets.Item(1).Range('A1');Require ($null -ne $cell) 'Excel is still editing';[pscustomobject]@{Formula=[string]$cell.Formula;Value=[double]$cell.Value2;Expected=([double]$pair[0]+[double]$pair[1])}}
 Require ($row.Formula.Contains('[source-1.xlsx]') -and $row.Formula.Contains('[source-2.xlsx]') -and [Math]::Abs($row.Value-$row.Expected) -lt 0.0000001) 'Incorrect external formula or value.'
 $rows+=$row
 $s=State;foreach($view in $s.views){Require ([OfficeLayoutProbe]::RibbonVisible([long]$view.hwnd)) 'Actual ribbon content missing';$checks++}
}
# Escape must discard the pending external formula and preserve the old value.
$doc.Activate();$doc.Worksheets.Item(1).Range('B1').Value2=321;$doc.Worksheets.Item(1).Range('B1').Select()
$entries=Entries (State);Select-References $entries $true
Retry {Require ($doc.Worksheets.Item(1).Range('B1').Value2 -eq 321) 'Cancelled formula overwrote original value'}
# Exercise the normal tab handler deterministically after commit/cancel.
# SendMessage coordinates must use the target's physical DPI context.
$s=State;$entries=Entries $s;$current=@($entries|Where-Object Id -eq $s.activeId)[0];if(-not $current){$current=$entries[0]}
foreach($next in $entries){
 [ReferenceInput]::DirectTab($current.Bar,$next.TabX,$current.TabY)
 try{$s=Retry {$state=State;Require ($state.activeId -eq $next.Id) ('Normal tab switch did not resume: expected '+$next.Id+', actual '+$state.activeId);$state}}
 catch{[pscustomobject]@{From=$current;To=$next;Input=[ReferenceInput]::Position();ActiveWorkbook=$app.ActiveWorkbook.Name;ActiveWindow=$app.ActiveWindow.Hwnd;State=($api.GetState()|ConvertFrom-Json)}|ConvertTo-Json -Depth 12|Set-Content (Join-Path $taskRoot 'artifacts\excel-normal-switch-failure.json') -Encoding UTF8;throw}
 $current=$next
}
# Exercise '+' after reference selection and check the actual NetUI contents.
[ReferenceInput]::DirectTab($current.Bar,[int]((31+120*3)*$current.Dpi/96),$current.TabY)
$s=State 4;$created=$app.ActiveWorkbook
Require ($created.Path -eq '' -and $created.Worksheets.Count -gt 0 -and @($created.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2 -or $_.Shapes.Count -ne 0}).Count -eq 0) 'Unexpected new document; preserved.'
foreach($view in $s.views){Require ([OfficeLayoutProbe]::RibbonVisible([long]$view.hwnd)) 'Ribbon/caption missing after new document';$checks++}
$createdView=@($s.views|Where-Object title -eq $created.Name)[0];Require ($api.Close([long]$createdView.id)) 'Owned blank tab close rejected';$s=State
foreach($view in $s.views){$api.SetViewWidth([long]$view.id,$initialWidths[[long]$view.id])|Out-Null}
$result=[pscustomobject]@{Passed=$true;SHA256=$expectedHash;FormulaCycles=$Cycles;Cases=$rows;FormulaUsesNativeInput=$true;CancelPreservesOriginal=$true;NormalTabMessageChecks=3;PlusMessageAfterFormula=$true;ActualRibbonContentChecks=$checks;SingleTaskbar=$s.group;ExistingUserDocumentsTouched=$false;FixtureDirectory=$fixtureDir}
$result|ConvertTo-Json -Depth 6|Set-Content (Join-Path $taskRoot 'artifacts\excel-reference-runtime.json') -Encoding UTF8
if(-not $LeaveOpen){
 Require ($books.Count -eq 3) 'Unexpected documents preserved at cleanup.'
 foreach($book in $books){Require ($book.FullName.StartsWith($fixtureDir+'\',[StringComparison]::OrdinalIgnoreCase)) 'Non-test workbook preserved at cleanup.'}
 foreach($book in $books){$book.Save()}
 foreach($view in $s.views){$api.Close([long]$view.id)|Out-Null;Start-Sleep -Milliseconds 150}
}
$result|ConvertTo-Json -Depth 6
