param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName,[switch]$UseExistingBlank,[string]$FixturePath='')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class MaskProbe {
 public class Appearance {public bool Layered,Visible;public int Alpha;public uint Color,Flags;}
 [DllImport("user32.dll")]static extern IntPtr GetWindowLongPtr(IntPtr w,int index);
 [DllImport("user32.dll")]static extern bool GetLayeredWindowAttributes(IntPtr w,out uint color,out byte alpha,out uint flags);
 [DllImport("user32.dll")]static extern IntPtr SendMessageTimeout(IntPtr w,uint m,IntPtr a,IntPtr b,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll")]static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")]static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")]static extern bool IsIconic(IntPtr w);
 [DllImport("user32.dll")]static extern bool IsZoomed(IntPtr w);
 [DllImport("user32.dll")]static extern bool ShowWindowAsync(IntPtr w,int command);
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static object Process(long h){uint p;GetWindowThreadProcessId(new IntPtr(h),out p);return (int)p;}
 public static object Minimized(long h){return IsIconic(new IntPtr(h));}
 public static object Maximized(long h){return IsZoomed(new IntPtr(h));}
 public static void MinimizeDirect(long h){ShowWindowAsync(new IntPtr(h),7);}
 public static object Read(long h){var w=new IntPtr(h);uint color,flags;byte alpha;bool known=GetLayeredWindowAttributes(w,out color,out alpha,out flags);return new Appearance{Layered=(GetWindowLongPtr(w,-20).ToInt64()&0x80000)!=0,Visible=(GetWindowLongPtr(w,-16).ToInt64()&0x10000000)!=0,Alpha=known?(int)alpha:-1,Color=known?color:0,Flags=known?flags:0};}
 public static void Message(long h,uint m){IntPtr result;if(SendMessageTimeout(new IntPtr(h),m,IntPtr.Zero,IntPtr.Zero,3,1000,out result)==IntPtr.Zero)throw new Exception("Office did not respond within one second");}
 public static void Command(long h,int command){if(!PostMessage(new IntPtr(h),0x112,new IntPtr(command),IntPtr.Zero))throw new Exception("Command failed");}
 public static void Place(long h,int x,int y,int width,int height){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{if(!SetWindowPos(new IntPtr(h),IntPtr.Zero,x,y,width,height,0x4014))throw new Exception("Placement failed");}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
function Require([bool]$ok,[string]$message){if(-not $ok){throw $message}}
function Idempotent([scriptblock]$call){
 $until=[DateTime]::UtcNow.AddSeconds(4)
 do{try{return (& $call)}catch{if($_.Exception.HResult -notin @(-2147418111,-2147417846) -or [DateTime]::UtcNow -ge $until){throw};Start-Sleep -Milliseconds 100}}while($true)
}
function Wait-For([scriptblock]$check,[string]$message){$until=[DateTime]::UtcNow.AddSeconds(6);do{if(& $check){return};Start-Sleep -Milliseconds 80}while([DateTime]::UtcNow -lt $until);throw $message}
$taskFile=if($HostName -eq 'Word'){'word-demo.docx'}elseif($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$taskPath=Join-Path $taskRoot ('tests\fixtures\'+$taskFile)
if($FixturePath){
 $taskAllowed=[IO.Path]::GetFullPath((Join-Path $taskRoot 'artifacts\mask-fixtures'))+[IO.Path]::DirectorySeparatorChar
 $taskCandidate=[IO.Path]::GetFullPath($FixturePath)
 Require ($taskCandidate.StartsWith($taskAllowed,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path $taskCandidate -Leaf) -eq $taskFile) 'Only a copied public fixture in the mask test directory is allowed.'
 $taskPath=$taskCandidate
}
if($HostName -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($HostName);$taskFixture=$taskApp.Presentations.Item(1)}else{$taskFixture=[OfficeRot]::AttachFile($taskPath);$taskApp=$taskFixture.Application}
if($HostName -eq 'Word'){$taskDocs=$taskApp.Documents}elseif($HostName -eq 'Excel'){$taskDocs=$taskApp.Workbooks}else{$taskDocs=$taskApp.Presentations}
Require ($taskDocs.Count -eq $(if($UseExistingBlank){2}else{1}) -and $taskFixture.FullName -eq $taskPath -and $taskFixture.Saved -and $taskApp.ProtectedViewWindows.Count -eq 0) 'Only owned saved fixture and optional blank allowed.'
if($UseExistingBlank){
 $taskBlank=@(for($i=1;$i -le $taskDocs.Count;$i++){$d=$taskDocs.Item($i);if($d.FullName -ne $taskPath){$d}})[0];Require ($taskBlank.Path -eq '') 'Other saved document preserved.'
 $taskEmpty=if($HostName -eq 'Word'){$taskBlank.Content.Text.Trim().Length -eq 0 -and $taskBlank.Shapes.Count -eq 0 -and $taskBlank.InlineShapes.Count -eq 0}elseif($HostName -eq 'Excel'){@($taskBlank.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2 -or $_.Shapes.Count -ne 0}).Count -eq 0}else{$taskBlank.Slides.Count -eq 0 -or ($taskBlank.Slides.Count -eq 1 -and @($taskBlank.Slides.Item(1).Shapes|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0)}
 Require $taskEmpty 'Edited document preserved.'
}
$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
function State {Idempotent {$taskApi.Refresh()}|Out-Null;return (Idempotent {$taskApi.GetState()}|ConvertFrom-Json)}
$s=State;$taskPid=[MaskProbe]::Process([long]$s.views[0].hwnd)
$taskDll=@((Get-Process -Id $taskPid).Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
$taskHash=(Get-FileHash (Join-Path $taskRoot 'dist\x64\openOfficetab.dll')).Hash
Require ($taskDll.Count -eq 1 -and (Get-FileHash $taskDll[0].FileName).Hash -eq $taskHash) 'Loaded DLL mismatch.'
if(-not $s.group){$taskApi.ToggleGroup()}
Idempotent {$taskApi.SetThemeMode(0)}|Out-Null;Idempotent {$taskApi.SetFrameCloseAction(0)}|Out-Null
if(-not $UseExistingBlank){$taskApi.NewDocument()}
Wait-For { $script:taskState=State;$taskState.views.Count -eq 2 } 'Two views did not settle.'
$s=$taskState;$taskSource=@($s.views|Where-Object title -eq $taskFixture.Name)
Require ($taskSource.Count -eq 1) 'Fixture identity ambiguous.'
$taskOther=@($s.views|Where-Object id -ne $taskSource[0].id)[0]
$taskSourceId=[long]$taskSource[0].id;$taskOtherId=[long]$taskOther.id
$taskHwnd=[long]$taskSource[0].hwnd;$taskPeer=[long]$taskOther.hwnd
Idempotent {$taskApi.Activate($taskSourceId)}|Out-Null
[MaskProbe]::Command($taskHwnd,0xF120);Start-Sleep -Milliseconds 250
Idempotent {$taskApi.Activate($taskSourceId)}|Out-Null
$taskWork=[OfficeLayoutProbe]::WorkArea($taskHwnd);$taskWidth=[int](($taskWork.Right-$taskWork.Left)*0.58);$taskHeight=[int](($taskWork.Bottom-$taskWork.Top)*0.65)
[MaskProbe]::Place($taskHwnd,$taskWork.Left+30,$taskWork.Top+30,$taskWidth,$taskHeight)
Wait-For { $r=[OfficeLayoutProbe]::Bounds($taskHwnd);$r.Left -eq $taskWork.Left+30 -and $r.Top -eq $taskWork.Top+30 -and [OfficeLayoutProbe]::Same($r,[OfficeLayoutProbe]::Bounds($taskPeer)) -and ([MaskProbe]::Read($taskPeer)).Alpha -ne 0 } 'Initial alignment/appearance failed.'
$taskBaseline=[MaskProbe]::Read($taskPeer);$taskSourceAppearance=[MaskProbe]::Read($taskHwnd)
function Restored { $a=[MaskProbe]::Read($taskPeer);return $a.Layered -eq $taskBaseline.Layered -and $a.Visible -eq $taskBaseline.Visible -and (-not $a.Layered -or ($a.Alpha -eq $taskBaseline.Alpha -and $a.Flags -eq $taskBaseline.Flags -and $a.Color -eq $taskBaseline.Color)) }
$taskWorstProbe=0;$taskMaskedChecks=0
for($taskBatch=0;$taskBatch -lt 3;$taskBatch++){
 [MaskProbe]::Message($taskHwnd,0x231)
 try{
  $a=[MaskProbe]::Read($taskPeer);Require ($a.Layered -and $a.Alpha -eq 0 -and $a.Visible) 'Peer not veiled before first movement.'
  $a=[MaskProbe]::Read($taskHwnd);Require ($a.Alpha -eq $taskSourceAppearance.Alpha -and $a.Layered -eq $taskSourceAppearance.Layered) 'Source appearance changed.'
  for($taskStep=0;$taskStep -lt 40;$taskStep++){
   [MaskProbe]::Place($taskHwnd,$taskWork.Left+50+$taskStep*16,$taskWork.Top+30+$taskStep*5,$taskWidth+($taskStep%12)*8,$taskHeight+($taskStep%9)*6)
   $taskWatch=[Diagnostics.Stopwatch]::StartNew();[MaskProbe]::Message($taskHwnd,0);$taskWatch.Stop();$taskWorstProbe=[Math]::Max($taskWorstProbe,$taskWatch.ElapsedMilliseconds)
   $a=[MaskProbe]::Read($taskPeer);Require ($a.Alpha -eq 0 -and $a.Visible) 'Peer veil lost during movement.';$taskMaskedChecks++
   Start-Sleep -Milliseconds 10
  }
 }finally{[MaskProbe]::Message($taskHwnd,$(if($taskBatch -eq 1){0x1F}else{0x232}))}
 Wait-For { (Restored) -and [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds($taskHwnd),[OfficeLayoutProbe]::Bounds($taskPeer)) } 'Appearance/geometry did not recover after drag or cancel.'
}
foreach($taskCommand in 0xF030,0xF120,0xF020,0xF120){
 [MaskProbe]::Command($taskHwnd,$taskCommand)
 Wait-For {
  $taskStateOk=if($taskCommand -eq 0xF030){[MaskProbe]::Maximized($taskHwnd) -and [MaskProbe]::Maximized($taskPeer)}elseif($taskCommand -eq 0xF020){[MaskProbe]::Minimized($taskHwnd) -and [MaskProbe]::Minimized($taskPeer)}else{-not [MaskProbe]::Minimized($taskHwnd) -and -not [MaskProbe]::Minimized($taskPeer) -and -not [MaskProbe]::Maximized($taskHwnd) -and -not [MaskProbe]::Maximized($taskPeer)}
  $taskStateOk -and (Restored)
 } ('Actual frame state/appearance did not settle after command '+$taskCommand+'.')
 if($taskCommand -eq 0xF020){Start-Sleep -Milliseconds 900;Require ([MaskProbe]::Minimized($taskHwnd) -and [MaskProbe]::Minimized($taskPeer)) 'Minimized group bounced back.'}
 [MaskProbe]::Message($taskHwnd,0)
}
# Exercise the SIZE_MINIMIZED route without a root SC_MINIMIZE command.
[MaskProbe]::MinimizeDirect($taskHwnd)
Wait-For { [MaskProbe]::Minimized($taskHwnd) -and [MaskProbe]::Minimized($taskPeer) -and (Restored) } 'Direct minimize did not synchronize.'
Start-Sleep -Milliseconds 900;Require ([MaskProbe]::Minimized($taskHwnd) -and [MaskProbe]::Minimized($taskPeer)) 'Direct minimized group bounced back.'
[MaskProbe]::Command($taskHwnd,0xF120)
Wait-For { -not [MaskProbe]::Minimized($taskHwnd) -and -not [MaskProbe]::Minimized($taskPeer) -and (Restored) } 'Direct minimize did not restore.'
Idempotent {$taskApi.Activate($taskOtherId)}|Out-Null;Start-Sleep -Milliseconds 120
$s=State;Require ($s.activeId -eq $taskOtherId -and $s.views.Count -eq 2) 'Tab selection failed or masked document disappeared.'
Require ([OfficeLayoutProbe]::RibbonVisible($taskPeer)) 'Ribbon hidden.'
[pscustomobject]@{Host=$HostName;ProcessId=$taskPid;LoadedHash=$taskHash;MaskedBeforeMovement=$true;MaskedChecks=$taskMaskedChecks;OfficeVisibilityRetained=$true;AppearanceRestored=$true;CancelRecovered=$true;MaximizeMinimizeRestore=$true;DirectMinimizeAndNoBounce=$true;SwitchAfterResize=$true;RibbonVisible=$true;MaxProbeMs=$taskWorstProbe;VisualReview='User mouse-drag acceptance pending'}|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot ('artifacts\window-mask-'+$HostName+'.json'))
