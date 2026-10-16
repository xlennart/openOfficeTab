param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName,[switch]$UseExistingBlank,[switch]$CloseAllAtEnd,[switch]$LeaveOpen)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
if(-not ('GroupedWindowProbe' -as [type])){Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class GroupedWindowProbe {
 [DllImport("user32.dll")]static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]static extern bool IsIconic(IntPtr w);
 [DllImport("user32.dll")]static extern bool IsZoomed(IntPtr w);
 [DllImport("user32.dll")]static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll")]static extern IntPtr SendMessageTimeout(IntPtr w,uint m,IntPtr a,IntPtr b,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll")]static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")]static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static object Process(long h){uint p;GetWindowThreadProcessId(new IntPtr(h),out p);return (int)p;}
 public static bool Minimized(long h){return IsIconic(new IntPtr(h));}
 public static bool Maximized(long h){return IsZoomed(new IntPtr(h));}
 public static bool Exists(long h){return IsWindow(new IntPtr(h));}
 public static void Command(long h,int command){if(!PostMessage(new IntPtr(h),0x112,new IntPtr(command),IntPtr.Zero))throw new Exception("System command failed");}
 public static void Message(long h,uint message){IntPtr result;if(SendMessageTimeout(new IntPtr(h),message,IntPtr.Zero,IntPtr.Zero,3,1000,out result)==IntPtr.Zero)throw new Exception("Office frame failed the bounded response probe");}
 public static void PlaceAsync(long h,int x,int y,int width,int height){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{if(!SetWindowPos(new IntPtr(h),IntPtr.Zero,x,y,width,height,0x4014))throw new Exception("Async placement failed");}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@}
$taskFile=if($HostName -eq 'Word'){'word-demo.docx'}elseif($HostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
$taskPath=Join-Path $taskRoot ('tests\fixtures\'+$taskFile)
if($HostName -eq 'PowerPoint'){$taskApp=[OfficeRot]::Attach($HostName);$taskFixture=$taskApp.Presentations.Item(1)}else{$taskFixture=[OfficeRot]::AttachFile($taskPath);$taskApp=$taskFixture.Application}
$taskCount=if($HostName -eq 'Word'){$taskApp.Documents.Count}elseif($HostName -eq 'Excel'){$taskApp.Workbooks.Count}else{$taskApp.Presentations.Count}
if($taskCount -ne $(if($UseExistingBlank){2}else{1}) -or $taskFixture.FullName -ne $taskPath -or -not $taskFixture.Saved -or $taskApp.ProtectedViewWindows.Count -ne 0){throw 'Unique saved public fixture required; user files preserved.'}
$taskApi=$taskApp.COMAddIns.Item('openOfficetab.Connect').Object
function Require([bool]$ok,[string]$message){if(-not $ok){throw $message}}
function State {$taskApi.Refresh()|Out-Null;return ($taskApi.GetState()|ConvertFrom-Json)}
function Wait-Views([int]$count){$until=[DateTime]::UtcNow.AddSeconds(6);do{try{$s=State;if($s.views.Count -eq $count){return $s}}catch{};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $until);throw 'Views did not settle.'}
function Wait-Native([scriptblock]$check,[string]$message){$until=[DateTime]::UtcNow.AddSeconds(5);do{if(& $check){return};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow -lt $until);throw $message}
$s=State;Require $s.diagnostics.windowCoordination 'Old 0.1.6 build loaded.'
$taskPid=[GroupedWindowProbe]::Process([long]$s.views[0].hwnd)
$taskDll=@((Get-Process -Id $taskPid).Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
Require ($taskDll.Count -eq 1 -and (Get-FileHash $taskDll[0].FileName).Hash -eq (Get-FileHash (Join-Path $taskRoot 'dist\x64\openOfficetab.dll')).Hash) 'Loaded DLL hash mismatch.'
if(-not $s.group){$taskApi.ToggleGroup()}
$taskApi.SetThemeMode(0)|Out-Null;$taskApi.SetFrameCloseAction(0)|Out-Null
if(-not $UseExistingBlank){$taskApi.NewDocument()};$s=Wait-Views 2
if($UseExistingBlank){
 $taskExisting=$taskApp.Presentations.Item(2);Require ($HostName -eq 'PowerPoint' -and $taskExisting.Path -eq '') 'Only the previous blank PPT test may be continued.'
 $taskNewId=[long]($s.views|Where-Object {$_.title -eq $taskExisting.Name}).id
}else{$taskNewId=[long]$s.activeId}
$taskSource=$s.views|Where-Object id -ne $taskNewId;$taskTarget=$s.views|Where-Object id -eq $taskNewId
$taskSourceId=[long]$taskSource.id;$taskApi.Activate($taskSourceId)
$taskHwnd=[long]$taskSource.hwnd;$taskOther=[long]$taskTarget.hwnd
[GroupedWindowProbe]::Command($taskHwnd,0xF120)
Start-Sleep -Milliseconds 300
# A synthetic background SC_RESTORE does not grant user foreground activation.
# Select the initiating document before emulating a user's move/resize.
$taskApi.Activate($taskSourceId);$s=State;Require ($s.activeId -eq $taskSourceId) 'Initiating document did not activate.'
$taskSlot=[OfficeLayoutProbe]::WorkArea($taskHwnd);$taskSlot.Right=$taskSlot.Left+[int](($taskSlot.Right-$taskSlot.Left)*0.65);$taskSlot.Bottom=$taskSlot.Top+[int](($taskSlot.Bottom-$taskSlot.Top)*0.8)
[OfficeLayoutProbe]::Place($taskHwnd,$taskSlot)
Wait-Native { [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds($taskHwnd),[OfficeLayoutProbe]::Bounds($taskOther)) } 'Move/resize did not synchronize.'
[GroupedWindowProbe]::Command($taskHwnd,0xF030)
Wait-Native { [GroupedWindowProbe]::Maximized($taskHwnd) -and [GroupedWindowProbe]::Maximized($taskOther) } 'Maximize did not synchronize.'
[GroupedWindowProbe]::Command($taskHwnd,0xF020)
Wait-Native { [GroupedWindowProbe]::Minimized($taskHwnd) -and [GroupedWindowProbe]::Minimized($taskOther) } 'Minimize did not synchronize.'
[GroupedWindowProbe]::Command($taskHwnd,0xF120)
Wait-Native { -not [GroupedWindowProbe]::Minimized($taskHwnd) -and -not [GroupedWindowProbe]::Minimized($taskOther) -and [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds($taskHwnd),[OfficeLayoutProbe]::Bounds($taskOther)) } 'Restore did not synchronize.'
$taskApi.Activate($taskNewId);$s=State;Require ($s.activeId -eq $taskNewId) 'Tab switch failed after restore.'
$taskApi.SetThemeMode(2)|Out-Null;Require (State).diagnostics.dark 'Forced dark failed.'
$taskApi.SetThemeMode(1)|Out-Null;Require (-not (State).diagnostics.dark) 'Forced light failed.'
$taskApi.SetThemeMode(0)|Out-Null
$taskApi.Activate($taskSourceId)
[GroupedWindowProbe]::Command($taskHwnd,0xF120)
Start-Sleep -Milliseconds 200
$taskApi.Activate($taskSourceId)
# Exercise sustained notifications while a move/size transaction is open.
# These are native test messages, not a claim of physical mouse-drag coverage.
$taskResizeWatch=[Diagnostics.Stopwatch]::StartNew();$taskWorstProbe=0
for($taskBatch=0;$taskBatch -lt 3;$taskBatch++){
 [GroupedWindowProbe]::Message($taskHwnd,0x231)
 try{
  for($taskStep=0;$taskStep -lt 40;$taskStep++){
   [GroupedWindowProbe]::PlaceAsync($taskHwnd,$taskSlot.Left+$taskStep,$taskSlot.Top+$taskBatch*8,($taskSlot.Right-$taskSlot.Left)+$taskStep%20,($taskSlot.Bottom-$taskSlot.Top)+$taskStep%15)
   $taskProbeWatch=[Diagnostics.Stopwatch]::StartNew();[GroupedWindowProbe]::Message($taskHwnd,0);$taskProbeWatch.Stop();$taskWorstProbe=[Math]::Max($taskWorstProbe,$taskProbeWatch.ElapsedMilliseconds)
   Start-Sleep -Milliseconds 10
  }
 }finally{[GroupedWindowProbe]::Message($taskHwnd,0x232)}
 Wait-Native { [OfficeLayoutProbe]::Same([OfficeLayoutProbe]::Bounds($taskHwnd),[OfficeLayoutProbe]::Bounds($taskOther)) } 'Post-drag synchronization failed.'
}
$taskResizeWatch.Stop()
$taskApi.Activate($taskNewId);$s=State;Require ($s.activeId -eq $taskNewId) 'Switch failed after sustained resize.'
Require ([OfficeLayoutProbe]::RibbonVisible($taskOther)) 'Ribbon hidden after resize.'
if($LeaveOpen){
 $taskResult=[pscustomobject]@{Host=$HostName;ProcessId=$taskPid;LoadedHash=(Get-FileHash $taskDll[0].FileName).Hash;MoveResize=$true;Maximize=$true;Minimize=$true;Restore=$true;SwitchAfterRestore=$true;ManualThemeModes=$true;AutoThemeMode=(State).diagnostics.themeMode;ResizeNotificationSteps=120;ResizeProbeMaxMs=$taskWorstProbe;ResizeTestMs=$taskResizeWatch.ElapsedMilliseconds;SwitchAfterSustainedResize=$true;RibbonVisibleAfterResize=$true;NativeFrameCloseCurrent='Not exercised';NativeCloseAllEditorWindows='Not exercised';VisualAndPromptReview='User review pending'}
 $taskResult|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot ('artifacts\window-coordination-'+$HostName+'.json'))
 return
}
$taskNew=if($HostName -eq 'Word'){$taskApp.ActiveDocument}elseif($HostName -eq 'Excel'){$taskApp.ActiveWorkbook}else{$taskApp.ActivePresentation}
Require ($taskNew.Path -eq '') 'Unexpected new document; preserved.'
$taskBlank=if($HostName -eq 'Word'){$taskNew.Content.Text.Trim().Length -eq 0}elseif($HostName -eq 'Excel'){@($taskNew.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2}).Count -eq 0}else{$taskNew.Slides.Count -eq 0 -or ($taskNew.Slides.Count -eq 1 -and @($taskNew.Slides.Item(1).Shapes|Where-Object {$_.HasTextFrame -eq 0 -or $_.TextFrame.TextRange.Text.Trim().Length -ne 0}).Count -eq 0)}
Require $taskBlank 'New test document was edited; preserved.'
# Mark only the newly generated blank as saved; do not suppress Office prompts.
if($HostName -eq 'PowerPoint'){$taskNew.Saved=-1}else{$taskNew.Saved=$true}
$taskApi.SetFrameCloseAction(1)|Out-Null
[GroupedWindowProbe]::Command($taskOther,0xF060)
$s=Wait-Views 1
Require ($s.group -and $s.diagnostics.frameCloseAction -eq 1) 'Remembered close-current mode failed.'
$taskApi.SetFrameCloseAction(0)|Out-Null
$taskThemeMode=(State).diagnostics.themeMode
if($CloseAllAtEnd){
 $taskApi.NewDocument();$s=Wait-Views 2;$taskAllWindow=[long]($s.views|Where-Object id -eq $s.activeId).hwnd
 $taskBlankDoc=if($HostName -eq 'Word'){$taskApp.ActiveDocument}elseif($HostName -eq 'Excel'){$taskApp.ActiveWorkbook}else{$taskApp.ActivePresentation}
 Require ($taskBlankDoc.Path -eq '') 'Unexpected document; preserved.'
 $taskEmpty=if($HostName -eq 'Word'){$taskBlankDoc.Content.Text.Trim().Length -eq 0 -and $taskBlankDoc.Shapes.Count -eq 0 -and $taskBlankDoc.InlineShapes.Count -eq 0}elseif($HostName -eq 'Excel'){@($taskBlankDoc.Worksheets|Where-Object {$null -ne $_.UsedRange.Value2 -or $_.Shapes.Count -ne 0}).Count -eq 0}else{$taskBlankDoc.Slides.Count -eq 0}
 Require ($taskEmpty -and $taskFixture.Saved -and $taskFixture.FullName -eq $taskPath) 'Edited test or fixture; preserved.'
 if($HostName -eq 'PowerPoint'){$taskBlankDoc.Saved=-1}else{$taskBlankDoc.Saved=$true}
 $taskApi.SetFrameCloseAction(2)|Out-Null
 # Release automation references before closing: Excel can retain its SDI frame
 # while a test still owns workbook/window RCWs. Only primitive HWNDs remain.
 $taskApi=$null;$taskBlankDoc=$null;$taskNew=$null;$taskExisting=$null;$taskFixture=$null;$taskApp=$null
 [GC]::Collect();[GC]::WaitForPendingFinalizers();[GC]::Collect();[GC]::WaitForPendingFinalizers()
 [GroupedWindowProbe]::Command($taskAllWindow,0xF060)
 Wait-Native { -not [GroupedWindowProbe]::Exists($taskHwnd) -and -not [GroupedWindowProbe]::Exists($taskAllWindow) } 'Close-group did not destroy both editor windows.'
}
$taskResult=[pscustomobject]@{Host=$HostName;ProcessId=$taskPid;LoadedHash=(Get-FileHash $taskDll[0].FileName).Hash;MoveResize=$true;Maximize=$true;Minimize=$true;Restore=$true;SwitchAfterRestore=$true;ManualThemeModes=$true;AutoThemeMode=$taskThemeMode;NativeFrameCloseCurrent=$true;NativeCloseAllEditorWindows=[bool]$CloseAllAtEnd;VisualAndPromptReview='User review pending'}
$taskResult|ConvertTo-Json|Tee-Object -FilePath (Join-Path $taskRoot ('artifacts\window-coordination-'+$HostName+'.json'))
