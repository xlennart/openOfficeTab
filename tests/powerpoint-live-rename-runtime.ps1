param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Architecture=$Architecture}){return}
. (Join-Path $root 'scripts/office-architecture.ps1');Initialize-OotArchitecture
if(@(Get-Process POWERPNT -ErrorAction SilentlyContinue).Count){throw 'Existing PowerPoint preserved'}
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.IO.Compression.FileSystem
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;using System.Text;
public static class RenameLiveUi {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr a,string text);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 delegate bool Callback(IntPtr w,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback c,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr w,StringBuilder b,int n);
 public static long Dialog(int pid){long found=0;EnumWindows((w,p)=>{uint owner;GetWindowThreadProcessId(w,out owner);var b=new StringBuilder(100);GetWindowText(w,b,100);if(owner==pid&&GetDlgItem(w,101)!=IntPtr.Zero&&(b.ToString()=="openOfficeTab"&&GetDlgItem(w,110)!=IntPtr.Zero&&GetDlgItem(w,113)!=IntPtr.Zero))found=w.ToInt64();return true;},IntPtr.Zero);return found;}
 public static void InvokeAsync(object pattern){var thread=new System.Threading.Thread(()=>pattern.GetType().GetMethod("Invoke").Invoke(pattern,null));thread.IsBackground=true;thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();}
 public static string CommandError;public static void CommandAsync(object api){var thread=new System.Threading.Thread(()=>{try{api.GetType().InvokeMember("RenameCurrent",System.Reflection.BindingFlags.InvokeMethod,null,api,new object[0]);}catch(Exception e){CommandError=e.GetBaseException().Message;}});thread.IsBackground=true;thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();}
 public static int Pid(long w){uint p;GetWindowThreadProcessId(new IntPtr(w),out p);return (int)p;}
}
'@
$run=Join-Path $root ('artifacts/ppt-live-rename-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $run|Out-Null
$candidate=(Resolve-Path (Join-Path $root "dist/$Architecture/openOfficetab.dll")).Path;$hash=(Get-FileHash $candidate).Hash
$view=if($Architecture -eq 'x86'){[Microsoft.Win32.RegistryView]::Registry32}else{[Microsoft.Win32.RegistryView]::Registry64};$hkcu=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,$view)
$backup=@();$checks=@();$failure=$null;$app=$null;$presentation=$null;$api=$null
try{
 foreach($id in '{65f17021-3b6b-4c5e-b54b-512806431710}','{865f57da-724b-4f9c-a6d6-ac2065c70238}'){$path='Software\Classes\CLSID\'+$id+'\InprocServer32';$key=$hkcu.OpenSubKey($path,$true);try{if(-not $key){throw 'Existing registration required'};$backup+=@{key=$path;before=[string]$key.GetValue('')};$key.SetValue('',$candidate)}finally{if($key){$key.Dispose()}}}
 $source=Join-Path $run 'source.pptx';Copy-Item (Join-Path $root 'tests/fixtures/powerpoint-demo.pptx') $source
 $app=New-Object -ComObject PowerPoint.Application;$app.Visible=-1;$presentation=$app.Presentations.Open($source,0,0,-1)
 $peerPath=Join-Path $run 'peer.pptx';Copy-Item (Join-Path $root 'tests/fixtures/powerpoint-demo.pptx') $peerPath;$peer=$app.Presentations.Open($peerPath,0,0,-1);$api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$initial=$api.GetState()|ConvertFrom-Json;$wasVisible=$initial.visible;if(-not $wasVisible){$api.ToggleVisible()}
 foreach($format in 24,38){
  $kind=if($format -eq 24){'regular'}else{'strict'};$old=Join-Path $run ($kind+'-source.pptx');$presentation.SaveAs($old,$format)
  $shape=$presentation.Slides.Item(1).Shapes.AddTextbox(1,20,20,240,45);$shape.TextFrame.TextRange.Text='OOT_LIVE_RENAME_'+$kind
  Start-Sleep -Milliseconds 250;$api.Refresh()|Out-Null;$state=$api.GetState()|ConvertFrom-Json;$viewRow=@($state.views|Where-Object title -eq ($kind+'-source.pptx'))[0];$api.Activate([int]$viewRow.id);Start-Sleep -Milliseconds 400;$state=$api.GetState()|ConvertFrom-Json;$state|ConvertTo-Json -Depth 12|Set-Content (Join-Path $run ($kind+'-before.json')) -Encoding UTF8;$window=[long]$viewRow.hwnd;$officePid=[RenameLiveUi]::Pid($window)
  $loaded=@([OpenOfficeTab.Desktop.Api]::ModulePaths($officePid)|Where-Object {[IO.Path]::GetFileName($_) -eq 'openOfficetab.dll'});if($loaded.Count -ne 1 -or (Get-FileHash $loaded[0]).Hash -ne $hash){throw 'Loaded DLL mismatch'}
  [RenameLiveUi]::SetForegroundWindow([IntPtr]$window)|Out-Null
  [RenameLiveUi]::CommandAsync($api)
  $until=[DateTime]::UtcNow.AddSeconds(3);$dialog=0
  do{$dialog=[RenameLiveUi]::Dialog($officePid);if(-not $dialog){Start-Sleep -Milliseconds 100}}while(-not $dialog -and [DateTime]::UtcNow -lt $until)
  if(-not $dialog){throw ('Native rename dialog missing: '+[RenameLiveUi]::CommandError)}
  $stem=$kind+'-renamed';[RenameLiveUi]::SendMessage([RenameLiveUi]::GetDlgItem([IntPtr]$dialog,101),0xC,[IntPtr]::Zero,$stem)|Out-Null
  [RenameLiveUi]::PostMessage([RenameLiveUi]::GetDlgItem([IntPtr]$dialog,1),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
  Start-Sleep -Milliseconds 500;$target=Join-Path $run ($stem+'.pptx')
  if($presentation.FullName -ne $target -or $app.Presentations.Count -ne 2 -or (Test-Path $old) -or -not(Test-Path $target)){throw 'Native dialog rename did not preserve the open presentation and clean the old path'}
  $after=$api.GetState()|ConvertFrom-Json;$renamed=@($after.views|Where-Object title -eq ($stem+'.pptx'))[0]
  if(-not $renamed -or [long]$renamed.hwnd -ne $window){throw 'Rename replaced the original editing window'}
  $checks+=@{format=$kind;status='passed';path=$target;pid=$officePid;hwnd=$window}
 }
}catch{$failure=$_.Exception.Message}
finally{
 if($peer){try{$peer.Close()}catch{}};if($api -and -not $wasVisible){try{$api.ToggleVisible()}catch{}};if($presentation){try{$presentation.Close()}catch{}};if($app){try{$app.Quit()}catch{}}
 $peer=$null;$shape=$null;$presentation=$null;$api=$null;$app=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers()
 if($officePid){$until=[DateTime]::UtcNow.AddSeconds(8);while((Get-Process -Id $officePid -ErrorAction SilentlyContinue) -and [DateTime]::UtcNow -lt $until){Start-Sleep -Milliseconds 150};if(Get-Process -Id $officePid -ErrorAction SilentlyContinue){$failure='Owned PowerPoint process did not exit'}}
 foreach($entry in $backup){$key=$hkcu.OpenSubKey($entry.key,$true);try{if($key.GetValue('') -eq $candidate){$key.SetValue('',$entry.before)}else{$failure='Concurrent registration change preserved'}}finally{$key.Dispose()}};$hkcu.Dispose()
 foreach($check in $checks){$zip=[IO.Compression.ZipFile]::OpenRead($check.path);try{
  $entry=$zip.GetEntry('ppt/slides/slide1.xml');$reader=New-Object IO.StreamReader($entry.Open());try{$text=$reader.ReadToEnd();if($text -notmatch ('OOT_LIVE_RENAME_'+$check.format)){$failure='Saved content missing'}}finally{$reader.Dispose()}
  $entry=$zip.GetEntry('ppt/presentation.xml');$reader=New-Object IO.StreamReader($entry.Open());try{$xml=[xml]$reader.ReadToEnd();$expectedNamespace=if($check.format -eq 'strict'){'http://purl.oclc.org/ooxml/presentationml/main'}else{'http://schemas.openxmlformats.org/presentationml/2006/main'};if($xml.DocumentElement.NamespaceURI -ne $expectedNamespace){$failure='Saved presentation format namespace changed'}}finally{$reader.Dispose()}
 }finally{$zip.Dispose()}}
 @{status=$(if($failure){'failed'}else{'passed'});error=$failure;architecture=$Architecture;sha256=$hash;checks=$checks}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $run 'result.json') -Encoding UTF8
}
Write-Output $run
if($failure){throw $failure}
