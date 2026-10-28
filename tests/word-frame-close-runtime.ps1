param([ValidateRange(1,30)][int]$Cycles=6,[string]$ContinueFixture='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Cycles=$Cycles;ContinueFixture=$ContinueFixture}){return}
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
[ComVisible(true),Guid("00020A01-0000-0000-C000-000000000046"),InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface IFrameCloseCancel {[DispId(6)] void BeforeClose([MarshalAs(UnmanagedType.IDispatch)] object doc,[MarshalAs(UnmanagedType.VariantBool)] ref bool cancel);}
[ComVisible(true),ClassInterface(ClassInterfaceType.None),ComDefaultInterface(typeof(IFrameCloseCancel))]
public sealed class FrameCloseCancel : IFrameCloseCancel,IDisposable {
 IConnectionPoint point;int cookie;public bool Armed;public int Cancelled;
 public FrameCloseCancel(object app){Guid id=new Guid("00020A01-0000-0000-C000-000000000046");((IConnectionPointContainer)app).FindConnectionPoint(ref id,out point);point.Advise(this,out cookie);}
 public void BeforeClose(object doc,ref bool cancel){if(Armed){cancel=true;Cancelled++;}}
 public void Dispose(){if(point!=null){point.Unadvise(cookie);Marshal.ReleaseComObject(point);point=null;}}
}
public static class FrameCloseProbe {
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("user32.dll")] static extern bool IsWindow(IntPtr w);
 public static object Process(long w){uint pid;GetWindowThreadProcessId(new IntPtr(w),out pid);return (long)pid;}
 public static void Close(long w,uint pid){if((long)Process(w)!=pid||!IsWindow(new IntPtr(w)))throw new Exception("Owned frame guard failed");if(!PostMessage(new IntPtr(w),0x112,new IntPtr(0xf060),IntPtr.Zero))throw new Exception("Close post failed");}
}
'@
if($ContinueFixture){
 $fixturePath=(Resolve-Path -LiteralPath $ContinueFixture).Path
 if([IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($fixturePath)) -ne (Join-Path $root 'artifacts') -or [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($fixturePath)) -notmatch '^word-frame-close-[0-9a-f]{32}$' -or [IO.Path]::GetFileName($fixturePath) -ne 'word-frame-close.docx'){throw 'Only this test unique fixture can be continued'}
}else{
 $run=Join-Path $root ('artifacts/word-frame-close-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $run | Out-Null
 $fixturePath=Join-Path $run 'word-frame-close.docx'
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures/word-demo.docx') -Destination $fixturePath
}
$existing=@(Get-Process WINWORD -ErrorAction SilentlyContinue | ForEach-Object Id)
$office=Join-Path $env:ProgramFiles 'Microsoft Office/root/Office16/WINWORD.EXE'
if(-not $ContinueFixture){Start-Process -FilePath $office -ArgumentList @('/x','/q',('"'+$fixturePath+'"')) -WindowStyle Normal | Out-Null}
$until=[DateTime]::UtcNow.AddSeconds(15)
do {try{$fixture=[OfficeRot]::AttachFile($fixturePath);$app=$fixture.Application;$api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$state=$api.GetState()|ConvertFrom-Json}catch{};if(-not $state){Start-Sleep -Milliseconds 100}}while(-not $state -and [DateTime]::UtcNow -lt $until)
if(-not $state -or $app.Documents.Count -ne 1 -or $fixture.FullName -ne $fixturePath -or -not $fixture.Saved){throw 'Isolated fixture guard failed; preserve windows.'}
$officePid=[FrameCloseProbe]::Process([long]$state.views[0].hwnd)
$processStart=(Get-Process -Id $officePid).StartTime.ToUniversalTime().ToString('o')
if($officePid -in $existing -and -not $ContinueFixture){throw 'Word reused an existing user process; preserve it.'}
if($ContinueFixture){$existing=@($existing|Where-Object {$_ -ne $officePid})}
$loaded=@((Get-Process -Id $officePid).Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
$hash=(Get-FileHash -LiteralPath (Join-Path $root 'dist/x64/openOfficetab.dll')).Hash
if($loaded.Count -ne 1 -or (Get-FileHash -LiteralPath $loaded[0].FileName).Hash -ne $hash){throw 'Wrong payload loaded; preserve test fixture.'}
$originalAction=[int]$state.diagnostics.frameCloseAction;$originalGroup=[bool]$state.group
$baseId=[long]$state.views[0].id
$measure=@();$ribbonChecks=0;$hook=$null
function State { $api.Refresh()|Out-Null; $api.GetState()|ConvertFrom-Json }
function Set-Action([int]$action) {
 $limit=[DateTime]::UtcNow.AddSeconds(3)
 do{$ok=$api.SetFrameCloseAction($action);if($ok){return};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $limit)
 throw 'Close preference did not persist'
}
function Wait-Views([int]$count) {
 $limit=[DateTime]::UtcNow.AddSeconds(8)
 do {try{$s=State;if($s.views.Count -eq $count -and -not $s.diagnostics.closePending){return $s}}catch{};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $limit)
 throw "Word did not settle at $count views; owned fixture preserved."
}
function Assert-OwnedDocuments {
 for($j=1;$j -le $app.Documents.Count;$j++){$d=$app.Documents.Item($j);if($d.FullName -ne $fixturePath -and ($d.Path -ne '' -or $d.Content.Text.Trim().Length)){throw 'Unexpected or edited document detected; preserve all windows.'}}
}
try {
 if(-not $originalGroup){$api.ToggleGroup()}
 Set-Action 1
 $hook=New-Object FrameCloseCancel -ArgumentList $app
 for($cycle=0;$cycle -lt $Cycles;$cycle++) {
  $api.NewDocument();$s=Wait-Views 2;Assert-OwnedDocuments
  $target=$s.views|Where-Object {$_.id -ne $baseId}
  if(@($target).Count -ne 1){throw 'Expected exactly one new blank document'}
  foreach($view in $s.views){if(-not [OfficeLayoutProbe].GetMethod('RibbonVisible').Invoke($null,[object[]]@([long]$view.hwnd))){throw 'Ribbon content unavailable'};$ribbonChecks++}
  if($cycle -eq 0){
   $hook.Armed=$true
   [FrameCloseProbe]::Close([long]$target.hwnd,$officePid)
   $until=[DateTime]::UtcNow.AddSeconds(5)
   do{Start-Sleep -Milliseconds 80;$s=State}while(($hook.Cancelled -eq 0 -or $s.diagnostics.closePending) -and [DateTime]::UtcNow -lt $until)
   if($hook.Cancelled -ne 1 -or $s.views.Count -ne 2 -or $s.diagnostics.closePending){throw 'Native close cancellation did not settle'}
   Set-Action 2
   [FrameCloseProbe]::Close([long]$target.hwnd,$officePid)
   $until=[DateTime]::UtcNow.AddSeconds(5)
   do{Start-Sleep -Milliseconds 80;$s=State}while(($hook.Cancelled -lt 2 -or $s.diagnostics.closePending) -and [DateTime]::UtcNow -lt $until)
   if($hook.Cancelled -ne 2 -or $s.views.Count -ne 2 -or $s.diagnostics.closePending){throw 'Group cancellation failed to preserve both documents'}
   Set-Action 1;$hook.Armed=$false
  }
  $watch=[Diagnostics.Stopwatch]::StartNew()
  [FrameCloseProbe]::Close([long]$target.hwnd,$officePid)
  $s=Wait-Views 1;$watch.Stop();$measure+=@($watch.ElapsedMilliseconds)
  if($s.views[0].id -ne $baseId -or -not $fixture.Saved){throw 'Base fixture changed'}
 }
 # Put two generated blanks in their own group, so the base fixture remains
 # available for restoring settings through the production API afterward.
 $api.NewDocument();$s=Wait-Views 2;Assert-OwnedDocuments
 $newId=[long]($s.views|Where-Object id -ne $baseId).id
 if(-not $api.NewTabGroup($newId)){throw 'Test group creation failed'}
 $api.Activate($newId);$api.NewDocument();$s=Wait-Views 3;Assert-OwnedDocuments
 $hook.Dispose();$hook=$null
 Set-Action 2
 $members=@($s.views|Where-Object id -ne $baseId)
 $frame=[long]$members[0].hwnd
 [FrameCloseProbe]::Close($frame,$officePid)
 # Word may retain/reuse an SDI HWND after its document closes. Verify the
 # actual document/view identities, not IsWindow on the old numeric handle.
 $s=Wait-Views 1
 if($s.views[0].id -ne $baseId -or $app.Documents.Count -ne 1){throw 'Other group was affected'}
 Set-Action $originalAction
 if(-not $originalGroup){$api.ToggleGroup()}
 $restored=$true
 if(-not $fixture.Saved -or $fixture.FullName -ne $fixturePath){throw 'Base fixture modified; preserved'}
 [FrameCloseProbe]::Close([long]$s.views[0].hwnd,$officePid)
 # Release our automation references before evaluating process lifetime.
 foreach($value in @($fixture,$api,$app)){if($value -and [Runtime.InteropServices.Marshal]::IsComObject($value)){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($value)}}
 $fixture=$null;$api=$null;$app=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers();[GC]::Collect()
 $result=@{Passed=$true;SHA256=$hash;ProcessId=$officePid;ProcessStartUtc=$processStart;Cycles=$Cycles;CloseMilliseconds=$measure;RibbonContentChecks=$ribbonChecks;CancellationPreserved=$true;GroupCancellationPreserved=$true;WholeGroupClose=$true;LastCaptionCloseRequested=$true;ExistingProcessesPreserved=$existing;Privacy='Only unique owned blank fixtures; no screenshots or user-document edits'}
 $result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $root 'artifacts/word-frame-close-runtime.json') -Encoding UTF8
 $result | ConvertTo-Json -Depth 5
} finally {
 if($hook){$hook.Armed=$false;$hook.Dispose()}
 if(-not $restored){Set-Action $originalAction;$s=State;if([bool]$s.group -ne $originalGroup){$api.ToggleGroup()}}
}
