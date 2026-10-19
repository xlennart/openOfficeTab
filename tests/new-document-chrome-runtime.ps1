param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName='Word',[int]$Cycles=12,[ValidateRange(1,3)][int]$BaseTabs=1,[switch]$ReuseOwnedPowerPoint,[switch]$LeaveOpen)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $taskRoot 'scripts\desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{HostName=$HostName;Cycles=$Cycles;BaseTabs=$BaseTabs;ReuseOwnedPowerPoint=[bool]$ReuseOwnedPowerPoint;LeaveOpen=[bool]$LeaveOpen}){return}
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class NewDocumentProbe {
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] static extern bool RedrawWindow(IntPtr w,IntPtr r,IntPtr region,uint flags);
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 public static object Process(long h){uint p;GetWindowThreadProcessId(new IntPtr(h),out p);return (int)p;}
 public static void Prepare(long root,long bar){SetForegroundWindow(new IntPtr(root));RedrawWindow(new IntPtr(bar),IntPtr.Zero,IntPtr.Zero,0x101);}
 public static void Plus(long h,int dpi,int height,int tabs){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{var w=new IntPtr(h);int x=(31+120*tabs)*dpi/96,y=height/2;var pos=new IntPtr((y<<16)|x);if(!PostMessage(w,0x201,new IntPtr(1),pos)||!PostMessage(w,0x202,IntPtr.Zero,pos))throw new Exception("Unable to queue tab '+' input");}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
$sampleFile=if($HostName -eq 'Word'){'word-demo.docx'}elseif($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$sampleDirectory=Join-Path $taskRoot ('artifacts\chrome-fixtures\'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $sampleDirectory -Force|Out-Null
$samplePath=Join-Path $sampleDirectory $sampleFile
Copy-Item -LiteralPath (Join-Path $taskRoot ('tests\fixtures\'+$sampleFile)) -Destination $samplePath
$executable=Join-Path 'C:\Program Files\Microsoft Office\root\Office16' $(if($HostName -eq 'Word'){'WINWORD.EXE'}elseif($HostName -eq 'Excel'){'EXCEL.EXE'}else{'POWERPNT.EXE'})
if($HostName -eq 'PowerPoint'){
 if(Get-Process POWERPNT -ErrorAction SilentlyContinue){
  if(-not $ReuseOwnedPowerPoint){throw 'Exit PowerPoint first; existing windows preserved.'}
  $reuse=[OfficeRot]::Attach('PowerPoint');$presentations=$reuse.Presentations
  $prefix=Join-Path $taskRoot 'artifacts\chrome-fixtures\'
  $reuseReady=$false;$reuseDeadline=[DateTime]::UtcNow.AddSeconds(5)
  do{try{$reuseCount=$presentations.Count;$protectedCount=$reuse.ProtectedViewWindows.Count;$reuseDocument=$presentations.Item(1);$reusePath=$reuseDocument.FullName;$reuseSaved=[bool]$reuseDocument.Saved;$reuseReady=$reuseCount -eq 1 -and $protectedCount -eq 0 -and $reusePath.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and $reuseSaved}catch{};if(-not $reuseReady){Start-Sleep -Milliseconds 100}}while(-not $reuseReady -and [DateTime]::UtcNow -lt $reuseDeadline)
  if(-not $reuseReady){throw ('Only an owned saved PowerPoint fixture may be reused. Count='+$reuseCount+'; protected='+$protectedCount+'; saved='+$reuseSaved+'; pathKnown='+($null -ne $reusePath)+'; testPath='+($reusePath -like ($prefix+'*')))}
  $samplePath=$presentations.Item(1).FullName;$sampleDirectory=Split-Path $samplePath -Parent
  $reuse.COMAddIns.Item('openOfficetab.Connect').Connect=$true
 }else{Start-Process -FilePath $executable -ArgumentList ('"'+$samplePath+'"') -WindowStyle Normal}
}else{Start-Process -FilePath $executable -ArgumentList @('/x',('"'+$samplePath+'"')) -WindowStyle Normal}
$app=$null;$fixture=$null;$bound=$false;$deadline=[DateTime]::UtcNow.AddSeconds(15)
do{try{if($HostName -eq 'PowerPoint'){$app=[OfficeRot]::Attach($HostName);$fixture=$app.Presentations.Item(1)}else{$fixture=[OfficeRot]::AttachFile($samplePath);$app=$fixture.Application};$bound=$null -ne $fixture -and $fixture.FullName -eq $samplePath}catch{};if(-not $bound){Start-Sleep -Milliseconds 150}}while(-not $bound -and [DateTime]::UtcNow -lt $deadline)
if(-not $bound){throw 'Owned sample instance unavailable; all windows preserved.'}
if($HostName -eq 'Word'){$docs=$app.Documents}elseif($HostName -eq 'Excel'){$docs=$app.Workbooks}else{$docs=$app.Presentations}
$deadline=[DateTime]::UtcNow.AddSeconds(6)
do{$guard=$docs.Count -eq 1 -and $fixture.FullName -eq $samplePath -and [bool]$fixture.Saved;if(-not $guard){Start-Sleep -Milliseconds 100}}while(-not $guard -and [DateTime]::UtcNow -lt $deadline)
if(-not $guard){throw ('Fixture guard failed; all windows preserved. Count='+$docs.Count+'; saved='+$fixture.Saved+'; expectedPath='+($fixture.FullName -eq $samplePath))}
$api=$app.COMAddIns.Item('openOfficetab.Connect').Object
$script:phase='initial-state'
function State {
 $retryUntil=[DateTime]::UtcNow.AddSeconds(4)
 do{try{$api.Refresh()|Out-Null;return ($api.GetState()|ConvertFrom-Json)}catch{$cause=$_.Exception;while($cause.InnerException){$cause=$cause.InnerException};if($cause.HResult -notin @(-2147418111,-2147417846) -or [DateTime]::UtcNow -ge $retryUntil){throw ('State failed in '+$script:phase+'; '+$cause.Message)};Start-Sleep -Milliseconds 80}}while($true)
}
function Wait-State([int]$count){
 $limit=[DateTime]::UtcNow.AddSeconds(7)
 do{
  $s=State
  if($s.views.Count -eq $count -and $s.paneMetrics.Count -eq $count -and @($s.paneMetrics|Where-Object {-not $_.matchesReservedRect -or -not $_.displayVisible}).Count -eq 0){return $s}
  Start-Sleep -Milliseconds 80
 }while([DateTime]::UtcNow -lt $limit)
 $s|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $taskRoot ('artifacts\chrome-failure-'+$HostName+'.json')) -Encoding UTF8
 throw ('Native document panes failed to settle. Expected='+$count+'; views='+$s.views.Count+'; panes='+$s.paneMetrics.Count+'; operation='+$s.diagnostics.lastOperation)
}
$s=Wait-State 1
$pidOffice=[NewDocumentProbe]::Process([long]$s.views[0].hwnd)
$dll=@((Get-Process -Id $pidOffice).Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
$expected=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'dist\x64\openOfficetab.dll')).Hash
if($dll.Count -ne 1 -or (Get-FileHash -LiteralPath $dll[0].FileName).Hash -ne $expected){throw 'Fresh instance loaded another DLL; windows preserved.'}
if(-not $s.group){throw 'Test requires current single-taskbar preference; settings left unchanged.'}
$fixtureId=[long]$s.views[0].id
if(-not $api.SetViewWidth($fixtureId,120)){throw 'Unable to set owned fixture width'}
for($index=1;$index -lt $BaseTabs;$index++){
 $extraPath=Join-Path $sampleDirectory ('example-'+$index+[IO.Path]::GetExtension($samplePath))
 Copy-Item -LiteralPath $samplePath -Destination $extraPath
 $extra=$docs.Open($extraPath)
 $s=Wait-State ($index+1)
 foreach($view in $s.views){if(-not $api.SetViewWidth([long]$view.id,120)){throw 'Unable to set owned tab width'}}
}
$checks=0;$worstReadyMs=0
for($cycle=0;$cycle -lt $Cycles;$cycle++){
 Write-Host ('Testing '+$HostName+' tab + cycle '+($cycle+1))
 $script:phase='before-plus-'+($cycle+1)
 $s=Wait-State $BaseTabs;$pane=$s.paneMetrics|Where-Object viewId -eq $fixtureId
 $fixture.Windows.Item(1).Activate()
 [NewDocumentProbe]::Prepare([long]$s.views[0].hwnd,[long]$pane.controlHwnd)
 Start-Sleep -Milliseconds 150
 if(-not [OfficeLayoutProbe]::RibbonVisible([long]$s.views[0].hwnd)){throw 'Real Ribbon content missing before new document'}
 $watch=[Diagnostics.Stopwatch]::StartNew()
 # Exercise the actual '+' window procedure, not just the COM NewDocument API.
 [NewDocumentProbe]::Plus([long]$pane.controlHwnd,[int]$pane.dpi,[int]$pane.height,$BaseTabs)
 $script:phase='after-plus-'+($cycle+1)
 $s=Wait-State ($BaseTabs+1);$worstReadyMs=[Math]::Max($worstReadyMs,$watch.ElapsedMilliseconds)
 if($HostName -eq 'Word'){$created=$app.ActiveDocument}elseif($HostName -eq 'Excel'){$created=$app.ActiveWorkbook}else{$created=$docs.Item($BaseTabs+1)}
 if($created.Path -ne '' -or $created.FullName -eq $samplePath){throw 'Unexpected document; preserve all windows.'}
 foreach($view in $s.views){if(-not [OfficeLayoutProbe]::RibbonVisible([long]$view.hwnd)){throw 'Actual Ribbon/caption content missing after tab +'};$checks++}
 foreach($id in @($fixtureId,$s.activeId)){
  $script:phase='switch-'+($cycle+1)
  $api.Activate([long]$id);Start-Sleep -Milliseconds 80
  $active=State;$view=$active.views|Where-Object id -eq $id
  if($active.activeId -ne $id -or -not [OfficeLayoutProbe]::RibbonVisible([long]$view.hwnd)){throw 'Ribbon/caption missing after switching tabs'};$checks++
 }
 $blank=if($HostName -eq 'Word'){$created.Content.Text.Trim().Length -eq 0 -and $created.Shapes.Count -eq 0 -and $created.InlineShapes.Count -eq 0}elseif($HostName -eq 'Excel'){@($created.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2 -or $_.Shapes.Count -ne 0}).Count -eq 0}else{$created.Slides.Count -eq 0 -or ($created.Slides.Count -eq 1 -and @($created.Slides.Item(1).Shapes|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0)}
 if(-not $blank){throw 'Created document has content; preserved.'}
 if($HostName -eq 'PowerPoint'){
  # Persist only this verified empty test presentation to avoid involving a
  # native unsaved-close prompt in a test of new-window Ribbon initialization.
  $created.SaveAs((Join-Path $sampleDirectory ('new-'+$cycle+'.pptx')))
 }
 if(-not $api.Close([long]$s.activeId)){throw 'Owned blank tab close request rejected'}
 $script:phase='close-'+($cycle+1)
 Start-Sleep -Milliseconds 300
 Wait-State $BaseTabs|Out-Null
}
$result=[pscustomobject]@{Host=$HostName;SHA256=$expected;SamplePath=$samplePath;BaseTabs=$BaseTabs;PlusCycles=$Cycles;ActualRibbonContentChecks=$checks;WorstPaneReadyMs=$worstReadyMs;SingleTaskbarMode=$true;ExistingUserDocumentsTouched=$false;VisualAcceptance='User verification pending'}
$result|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskRoot ('artifacts\new-document-chrome-'+$HostName+'-'+$BaseTabs+'-tabs.json')) -Encoding UTF8
$allOwned=$docs.Count -eq $BaseTabs
for($index=1;$index -le $docs.Count;$index++){if(-not $docs.Item($index).FullName.StartsWith($sampleDirectory+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or -not [bool]$docs.Item($index).Saved){$allOwned=$false}}
if($allOwned -and -not $LeaveOpen){$app.Quit()}
$result|ConvertTo-Json
