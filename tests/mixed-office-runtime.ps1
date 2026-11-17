param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Architecture=$Architecture}){return}
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $root 'scripts/office-architecture.ps1');Initialize-OotArchitecture
if(@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue).Count){throw 'Existing Office processes preserved. Close Office before the isolated mixed-group test.'}
$run=Join-Path $root ('artifacts/mixed-office-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run|Out-Null
$candidate=(Resolve-Path (Join-Path $root "dist/$Architecture/openOfficetab.dll")).Path
$hash=(Get-FileHash $candidate).Hash
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class MixedProbe {
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern void SwitchToThisWindow(IntPtr w,bool altTab);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr w,out Rect r);
 public struct Rect {public int L,T,R,B;}
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 public static int[] Bounds(long w){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{Rect r;if(!GetWindowRect(new IntPtr(w),out r))throw new Exception("Window bounds unavailable");return new[]{r.L,r.T,r.R,r.B};}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static int Pid(long w){uint p;GetWindowThreadProcessId(new IntPtr(w),out p);return (int)p;}
 public static void Front(long w,int pid){if(Pid(w)!=pid)throw new Exception("Stale fixture frame");SwitchToThisWindow(new IntPtr(w),false);SetForegroundWindow(new IntPtr(w));}
 [DllImport("user32.dll")] static extern bool GetLayeredWindowAttributes(IntPtr w,out uint color,out byte alpha,out uint flags);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr w);
 [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 public static bool Move(long w,int x,int y,int width,int height){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{return SetWindowPos(new IntPtr(w),IntPtr.Zero,x,y,width,height,0x14);}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);

 [DllImport("user32.dll")] static extern int GetSystemMetrics(int n);
 [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
 [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr w,uint flags);
 [DllImport("user32.dll")] static extern uint SendInput(uint count,Input[] inputs,int size);
 [StructLayout(LayoutKind.Sequential)]struct Point{public int X,Y;}
 [StructLayout(LayoutKind.Sequential)]struct Mouse{public int X,Y;public uint Data,Flags,Time;public IntPtr Extra;}
 [StructLayout(LayoutKind.Sequential)]struct Input{public uint Type;public Mouse Mouse;}
 public static void ClickOwnedTab(long root,long bar,int pid,int dpi){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));var window=new IntPtr(root);
  try{if(Pid(root)!=pid||GetAncestor(new IntPtr(bar),2)!=window||!IsWindowEnabled(window))throw new Exception("Stale or disabled fixture");
   SetWindowPos(window,new IntPtr(-1),0,0,0,0,0x13);
   var r=Bounds(bar);var point=new Point{X=r[0]+24*dpi/96,Y=(r[1]+r[3])/2};
   if(GetAncestor(WindowFromPoint(point),2)!=window)throw new Exception("Owned tab point is not exposed");
   int left=GetSystemMetrics(76),top=GetSystemMetrics(77),width=GetSystemMetrics(78),height=GetSystemMetrics(79);
   var move=new Input{Mouse=new Mouse{X=(point.X-left)*65535/(width-1),Y=(point.Y-top)*65535/(height-1),Flags=0xC001}};
   var down=new Input{Mouse=new Mouse{Flags=2}};var up=new Input{Mouse=new Mouse{Flags=4}};
   if(SendInput(3,new[]{move,down,up},Marshal.SizeOf(typeof(Input)))!=3)throw new Exception("Owned click input was blocked");
  }finally{SetWindowPos(window,new IntPtr(-2),0,0,0,0,0x13);SetThreadDpiAwarenessContext(old);}
 }
 public static bool Concealed(long w){uint c,f;byte a;return GetLayeredWindowAttributes(new IntPtr(w),out c,out a,out f)&&a==0&&(f&2)!=0;}
}
'@
$registry=@();$apps=@();$apis=@();$docs=@();$pids=@();$results=@();$errorText=$null
$view=if($Architecture -eq 'x86'){[Microsoft.Win32.RegistryView]::Registry32}else{[Microsoft.Win32.RegistryView]::Registry64}
$hkcu=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,$view)
function Read-State($api){$api.Refresh()|Out-Null;return $api.GetState()|ConvertFrom-Json}
function Assert-True($value,[string]$message){if(-not $value){throw $message}}
try{
 foreach($id in '{65f17021-3b6b-4c5e-b54b-512806431710}','{865f57da-724b-4f9c-a6d6-ac2065c70238}'){
  $key='Software\Classes\CLSID\'+$id+'\InprocServer32';$entry=$hkcu.OpenSubKey($key,$true)
  if(-not $entry){throw 'Existing per-user registration required.'}
  try{$before=[string]$entry.GetValue('');Assert-True (Test-Path $before) 'Original DLL unavailable';$registry+=@{Key=$key;Before=$before};$entry.SetValue('',$candidate)}finally{$entry.Dispose()}
 }
 $hosts=@('Word','Excel','PowerPoint');$files=@('word-demo.docx','excel-demo.xlsx','powerpoint-demo.pptx')
 for($i=0;$i -lt 3;$i++){
  $path=Join-Path $run $files[$i];Copy-Item (Join-Path $PSScriptRoot ('fixtures/'+$files[$i])) $path
  $app=New-Object -ComObject ($hosts[$i]+'.Application');$apps+=,$app
  if($i -lt 2){$app.Visible=$true}else{$app.Visible=-1}
  $doc=if($i -eq 0){$app.Documents.Open($path)}elseif($i -eq 1){$app.Workbooks.Open($path)}else{$app.Presentations.Open($path,0,0,-1)}
  $docs+=,$doc;$api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$apis+=,$api
  $until=[DateTime]::UtcNow.AddSeconds(12)
  do{$state=Read-State $api;if($state.views.Count -eq 1 -and $state.paneMetrics.Count -eq 1){break};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $until)
  Assert-True ($state.views.Count -eq 1 -and $state.paneMetrics.Count -eq 1) 'Fixture did not load its pane'
  $officePid=[MixedProbe]::Pid([long]$state.views[0].hwnd);$pids+=@{Pid=$officePid;Started=(Get-Process -Id $officePid).StartTime.ToUniversalTime().ToString('o')}
  $loaded=@([OpenOfficeTab.Desktop.Api]::ModulePaths($officePid)|Where-Object {[IO.Path]::GetFileName($_) -eq 'openOfficetab.dll'})
  Assert-True ($loaded.Count -eq 1 -and (Get-FileHash $loaded[0]).Hash -eq $hash) 'Loaded DLL hash mismatch'
 }
 Start-Sleep -Milliseconds 400
 # Merge an existing two-app group onto the third app, the previously failing direction.
 for($i=1;$i -lt 3;$i++){
  $target=Read-State $apis[0];$source=Read-State $apis[$i]
  [MixedProbe]::SetForegroundWindow([IntPtr][long]$target.views[0].hwnd)|Out-Null;Start-Sleep -Milliseconds 200
  $bounds=[MixedProbe]::Bounds([long]$target.paneMetrics[0].controlHwnd)
  if($i -eq 1){Assert-True ($apis[0].JoinOfficeGroup([int]$target.views[0].id,[int]$source.views[0].hwnd)) 'Initial join rejected'}else{Assert-True ($apis[0].JoinOfficeGroup([int]$target.views[0].id,[int]$source.views[0].hwnd)) 'Whole-group merge rejected'}
  Start-Sleep -Milliseconds 400
 }
 foreach($api in $apis){$state=Read-State $api;$state|ConvertTo-Json -Depth 12|Set-Content (Join-Path $run ($state.host+'-snapshot.json')) -Encoding UTF8;Assert-True ($state.mixedTabs.Count -eq 3) 'Mixed member list is not synchronized';$pane=$state.paneMetrics[0];$outer=[MixedProbe]::Bounds([long]$state.views[0].hwnd);$inner=[MixedProbe]::Bounds([long]$pane.controlHwnd);Assert-True ($inner[0] -ge $outer[0] -and $inner[1] -ge $outer[1] -and $inner[2] -le $outer[2] -and $inner[3] -le $outer[3]) ('Tab bar is outside Office frame; outer='+($outer -join ',')+' inner='+($inner -join ','))}
 $results+=@{name='three-host membership and in-window tab bounds';status='passed'}
 $first=Read-State $apis[0];$excelTab=@($first.mixedTabs|Where-Object host -eq 1)[0];Assert-True ($apis[0].SetViewWidth([int]$excelTab.id,216)) 'Remote width update rejected';Start-Sleep -Milliseconds 400
 foreach($api in $apis){$state=Read-State $api;$tab=@($state.mixedTabs|Where-Object host -eq 1)[0];Assert-True ($tab.width -eq 216) 'Remote width not synchronized'}
 $results+=@{name='cross-host per-tab width synchronization';status='passed'}
 # Establish one owned foreground fixture once; subsequent switches are made
 # only through the production mixed-tab command and verified independently.
 $initialIndex=@(foreach($j in 0,1,2){$frame=Read-State $apis[$j];if(-not [MixedProbe]::Concealed([long]$frame.views[0].hwnd)){$j}})[0]
 $initial=Read-State $apis[$initialIndex]
 [MixedProbe]::ClickOwnedTab([long]$initial.views[0].hwnd,[long]$initial.paneMetrics[0].controlHwnd,[int]$pids[$initialIndex].Pid,[int]$initial.paneMetrics[0].dpi);Start-Sleep -Milliseconds 500
 for($i=0;$i -lt 3;$i++){
  $sourceIndex=-1;$front=[MixedProbe]::GetForegroundWindow().ToInt64();for($j=0;$j -lt 3;$j++){if([long](Read-State $apis[$j]).views[0].hwnd -eq $front){$sourceIndex=$j}};if($sourceIndex -lt 0){throw "Driver blocked: no fixture foreground"};$state=Read-State $apis[$sourceIndex];$target=Read-State $apis[$i]
  if($sourceIndex -eq $i){continue}
  $remote=@($state.mixedTabs|Where-Object host -eq $i)[0];$apis[$sourceIndex].Activate([int]$remote.id)
  Start-Sleep -Milliseconds 500
  Assert-True ([MixedProbe]::GetForegroundWindow().ToInt64() -eq [long]$target.views[0].hwnd) 'Actual foreground did not switch to target Office'
  Start-Sleep -Milliseconds 300
  for($j=0;$j -lt 3;$j++){$frame=Read-State $apis[$j];$hidden=[MixedProbe]::Concealed([long]$frame.views[0].hwnd);Assert-True ($hidden -eq ($j -ne $i)) 'Mixed group exposes an extra window or conceals active window';Assert-True ([bool]$frame.views[0].taskbarRemoved -eq ($j -ne $i)) 'Taskbar membership does not follow selected member'}
 }
 $results+=@{name='Word Excel PowerPoint actual foreground switching';status='passed'}
 $active=Read-State $apis[2];[MixedProbe]::PostMessage([IntPtr][long]$active.views[0].hwnd,0x112,[IntPtr]0xF020,[IntPtr]::Zero)|Out-Null
 Start-Sleep -Milliseconds 700
 foreach($api in $apis){$state=Read-State $api;Assert-True ([MixedProbe]::IsIconic([IntPtr][long]$state.views[0].hwnd)) 'Minimize did not apply to entire group'}
 [MixedProbe]::PostMessage([IntPtr][long]$active.views[0].hwnd,0x112,[IntPtr]0xF120,[IntPtr]::Zero)|Out-Null;[MixedProbe]::SetForegroundWindow([IntPtr][long]$active.views[0].hwnd)|Out-Null
 Start-Sleep -Milliseconds 600
 Assert-True (-not [MixedProbe]::IsIconic([IntPtr][long]$active.views[0].hwnd)) 'Group restore failed'
 $results+=@{name='group minimize and restore';status='passed'}
 [MixedProbe]::PostMessage([IntPtr][long]$active.views[0].hwnd,0x112,[IntPtr]0xF030,[IntPtr]::Zero)|Out-Null
 Start-Sleep -Milliseconds 900
 $maxBounds=[MixedProbe]::Bounds([long]$active.views[0].hwnd)
 foreach($api in $apis){$state=Read-State $api;Assert-True ([MixedProbe]::IsZoomed([IntPtr][long]$state.views[0].hwnd)) 'Maximize did not follow selected window';Assert-True (([MixedProbe]::Bounds([long]$state.views[0].hwnd) -join ',') -eq ($maxBounds -join ',')) 'Maximized group bounds differ'}
 [MixedProbe]::PostMessage([IntPtr][long]$active.views[0].hwnd,0x112,[IntPtr]0xF120,[IntPtr]::Zero)|Out-Null
 Start-Sleep -Milliseconds 700
 $slot=[MixedProbe]::Bounds([long]$active.views[0].hwnd)
 Assert-True ([MixedProbe]::Move([long]$active.views[0].hwnd,($slot[0]+24),($slot[1]+24),($slot[2]-$slot[0]),($slot[3]-$slot[1]))) 'Test window move failed'
 Start-Sleep -Milliseconds 900
 $movedBounds=[MixedProbe]::Bounds([long]$active.views[0].hwnd)
 foreach($api in $apis){$state=Read-State $api;Assert-True (-not [MixedProbe]::IsZoomed([IntPtr][long]$state.views[0].hwnd)) 'Restore did not follow selected window';Assert-True (([MixedProbe]::Bounds([long]$state.views[0].hwnd) -join ',') -eq ($movedBounds -join ',')) 'Moved group bounds differ'}
 $results+=@{name='group maximize restore and native window move';status='passed'}
 # Close only the current, saved fixture through the real frame command.
 [MixedProbe]::PostMessage([IntPtr][long]$active.views[0].hwnd,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
 Start-Sleep -Milliseconds 900
 $remaining=@();foreach($i in 0,1){$state=Read-State $apis[$i];Assert-True ($state.views.Count -eq 1 -and $state.mixedTabs.Count -eq 2) 'Closing current document damaged surviving group';$remaining+=,[MixedProbe]::Concealed([long]$state.views[0].hwnd)}
 Assert-True (@($remaining|Where-Object {-not $_}).Count -ge 1) 'Closing selected document stranded surviving windows'
 $results+=@{name='native frame close preserves remaining two-host group';status='passed'}
 # Restore is the existing public recovery command and must detach all members.
 foreach($i in 0,1){$apis[$i].RestoreWindows()};Start-Sleep -Milliseconds 400
 foreach($i in 0,1){$state=Read-State $apis[$i];Assert-True ($state.mixedTabs.Count -eq 0) 'Mixed group did not detach';Assert-True (-not [MixedProbe]::Concealed([long]$state.views[0].hwnd)) 'Recovery left a window concealed'}
 $results+=@{name='recovery detaches mixed membership';status='passed'}
}catch{$errorText=$_.Exception.Message}
finally{
 if($errorText){
  try{
   $front=[MixedProbe]::GetForegroundWindow().ToInt64();$frames=@(foreach($api in $apis){$s=Read-State $api;@{host=$s.host;hwnd=$s.views[0].hwnd;iconic=[MixedProbe]::IsIconic([IntPtr][long]$s.views[0].hwnd);enabled=[MixedProbe]::IsWindowEnabled([IntPtr][long]$s.views[0].hwnd);concealed=[MixedProbe]::Concealed([long]$s.views[0].hwnd)}})
   @{foreground=$front;foregroundPid=[MixedProbe]::Pid($front);driverSession=(Get-Process -Id $PID).SessionId;frames=$frames}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $run 'failure-state.json') -Encoding UTF8
   Add-Type -AssemblyName System.Drawing,System.Windows.Forms
   $rect=[Windows.Forms.SystemInformation]::VirtualScreen;$bitmap=[Drawing.Bitmap]::new($rect.Width,$rect.Height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
   try{$graphics.CopyFromScreen($rect.Location,[Drawing.Point]::Empty,$rect.Size);$bitmap.Save((Join-Path $run 'failure-screen.png'),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
  }catch{}
 }
 for($i=0;$i -lt $docs.Count;$i++){try{if($i -eq 0){$docs[$i].Close(0)}elseif($i -eq 1){$docs[$i].Close($false)}else{$docs[$i].Close()}}catch{}}
 foreach($app in $apps){try{$app.Quit()}catch{}}
 $docs=@();$apis=@();$apps=@();$app=$null;$doc=$null;$api=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers()
 foreach($entry in $registry){$key=$hkcu.OpenSubKey($entry.Key,$true);try{if($key.GetValue('') -eq $candidate){$key.SetValue('',$entry.Before)}else{$errorText='Concurrent registration change; original registration was not overwritten'}}finally{$key.Dispose()}}
 $hkcu.Dispose()
 @{status=$(if($errorText){'failed'}else{'passed'});error=$errorText;architecture=$Architecture;dllSha256=$hash;processes=$pids;checks=$results;scope='command-driven join and foreground; not human dragging, modal save, DPI or x86 acceptance'}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $run 'result.json') -Encoding UTF8
}
Write-Output $run
if($errorText){throw $errorText}
