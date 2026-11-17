param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Architecture=$Architecture}){return}
. (Join-Path $PSScriptRoot 'common.ps1')
if(@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue).Count){throw 'Existing Office processes preserved. Close Office before the isolated mixed-group test.'}
$run=Join-Path $root ('artifacts/mixed-office-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run|Out-Null
$candidate=(Resolve-Path (Join-Path $root "dist/$Architecture/openOfficetab.dll")).Path
$hash=(Get-FileHash $candidate).Hash
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class MixedProbe {
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr w,out Rect r);
 public struct Rect {public int L,T,R,B;}
 public static int[] Bounds(long w){Rect r;if(!GetWindowRect(new IntPtr(w),out r))throw new Exception("Window bounds unavailable");return new[]{r.L,r.T,r.R,r.B};}
 public static int Pid(long w){uint p;GetWindowThreadProcessId(new IntPtr(w),out p);return (int)p;}
 [DllImport("user32.dll")] static extern bool GetLayeredWindowAttributes(IntPtr w,out uint color,out byte alpha,out uint flags);
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
  $loaded=@((Get-Process -Id $officePid).Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
  Assert-True ($loaded.Count -eq 1 -and (Get-FileHash $loaded[0].FileName).Hash -eq $hash) 'Loaded DLL hash mismatch'
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
 foreach($api in $apis){$state=Read-State $api;$state|ConvertTo-Json -Depth 12|Set-Content (Join-Path $run ($state.host+'-snapshot.json')) -Encoding UTF8;Assert-True ($state.mixedTabs.Count -eq 3) 'Mixed member list is not synchronized';$pane=$state.paneMetrics[0];$outer=[MixedProbe]::Bounds([long]$state.views[0].hwnd);$inner=[MixedProbe]::Bounds([long]$pane.controlHwnd);Assert-True ($inner[0] -ge $outer[0] -and $inner[1] -ge $outer[1] -and $inner[2] -le $outer[2] -and $inner[3] -le $outer[3]) 'Tab bar is outside Office frame'}
 $results+=@{name='three-host membership and in-window tab bounds';status='passed'}
 $first=Read-State $apis[0];$excelTab=@($first.mixedTabs|Where-Object host -eq 1)[0];Assert-True ($apis[0].SetViewWidth([int]$excelTab.id,216)) 'Remote width update rejected';Start-Sleep -Milliseconds 400
 foreach($api in $apis){$state=Read-State $api;$tab=@($state.mixedTabs|Where-Object host -eq 1)[0];Assert-True ($tab.width -eq 216) 'Remote width not synchronized'}
 $results+=@{name='cross-host per-tab width synchronization';status='passed'}
 for($i=0;$i -lt 3;$i++){
  $sourceIndex=($i+2)%3;$state=Read-State $apis[$sourceIndex];$target=Read-State $apis[$i]
  [MixedProbe]::SetForegroundWindow([IntPtr][long]$state.views[0].hwnd)|Out-Null;Start-Sleep -Milliseconds 150
  $remote=@($state.mixedTabs|Where-Object host -eq $i)[0];$apis[$sourceIndex].Activate([int]$remote.id)
  Start-Sleep -Milliseconds 500
  Assert-True ([MixedProbe]::GetForegroundWindow().ToInt64() -eq [long]$target.views[0].hwnd) 'Actual foreground did not switch to target Office'
  Start-Sleep -Milliseconds 300
  for($j=0;$j -lt 3;$j++){$frame=Read-State $apis[$j];$hidden=[MixedProbe]::Concealed([long]$frame.views[0].hwnd);Assert-True ($hidden -eq ($j -ne $i)) 'Mixed group exposes an extra window or conceals active window';Assert-True ([bool]$frame.views[0].taskbarRemoved -eq ($j -ne $i)) 'Taskbar membership does not follow selected member'}
 }
 $results+=@{name='Word Excel PowerPoint actual foreground switching';status='passed'}
 # Restore is the existing public recovery command and must detach all members.
 foreach($api in $apis){$api.RestoreWindows()};Start-Sleep -Milliseconds 400
 foreach($api in $apis){$state=Read-State $api;Assert-True ($state.mixedTabs.Count -eq 0) 'Mixed group did not detach';Assert-True (-not [MixedProbe]::Concealed([long]$state.views[0].hwnd)) 'Recovery left a window concealed'}
 $results+=@{name='recovery detaches mixed membership';status='passed'}
}catch{$errorText=$_.Exception.Message}
finally{
 for($i=0;$i -lt $docs.Count;$i++){try{if($i -eq 0){$docs[$i].Close(0)}elseif($i -eq 1){$docs[$i].Close($false)}else{$docs[$i].Close()}}catch{}}
 foreach($app in $apps){try{$app.Quit()}catch{}}
 $docs=@();$apis=@();$apps=@();$app=$null;$doc=$null;$api=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers()
 foreach($entry in $registry){$key=$hkcu.OpenSubKey($entry.Key,$true);try{if($key.GetValue('') -eq $candidate){$key.SetValue('',$entry.Before)}else{$errorText='Concurrent registration change; original registration was not overwritten'}}finally{$key.Dispose()}}
 $hkcu.Dispose()
 @{status=$(if($errorText){'failed'}else{'passed'});error=$errorText;architecture=$Architecture;dllSha256=$hash;processes=$pids;checks=$results;scope='command-driven join and foreground; not human dragging, modal save, DPI or x86 acceptance'}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $run 'result.json') -Encoding UTF8
}
Write-Output $run
if($errorText){throw $errorText}
