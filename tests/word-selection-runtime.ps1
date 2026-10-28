param([ValidateRange(1,12)][int]$Cycles=4)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-context.ps1')
if(Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{Cycles=$Cycles}){return}
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'window-probe.ps1')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.IO.Compression,System.IO.Compression.FileSystem
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class SelectionProbe {
 [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr w,uint flag);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] static extern bool IsIconic(IntPtr w);
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static object Front(){return GetAncestor(GetForegroundWindow(),2).ToInt64();}
 public static object Process(long w){uint p;GetWindowThreadProcessId(new IntPtr(w),out p);return (int)p;}
 public static bool Minimized(long w){return IsIconic(new IntPtr(w));}
 public static void Front(long w){SetForegroundWindow(new IntPtr(w));}
 public static void Command(long w,int c){if(!PostMessage(new IntPtr(w),0x112,new IntPtr(c),IntPtr.Zero))throw new Exception("Native command failed");}
 public static void Click(long w,int x,int y){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{var p=new IntPtr((y<<16)|x);if(!PostMessage(new IntPtr(w),0x201,new IntPtr(1),p)||!PostMessage(new IntPtr(w),0x202,IntPtr.Zero,p))throw new Exception("Tab message failed");}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
$baseline=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue)
$emptyBaseline=$false
if($baseline.Count){
 if($baseline.Count -ne 1 -or $baseline[0].ProcessName -ne 'WINWORD' -or $baseline[0].MainWindowHandle -ne 0){throw 'Close Office before this isolated runtime test; existing windows preserved.'}
 try{$emptyApp=[OfficeRot]::Attach('Word');$emptyBaseline=$emptyApp.Documents.Count -eq 0 -and $emptyApp.Windows.Count -eq 0 -and $emptyApp.ProtectedViewWindows.Count -eq 0}finally{$emptyApp=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers()}
 if(-not $emptyBaseline){throw 'Existing Word process is not empty; preserve it.'}
 $baselineStart=$baseline[0].StartTime.ToUniversalTime().ToString('o')
}
$run=Join-Path $root ('artifacts/word-selection-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run|Out-Null
$candidate=(Resolve-Path -LiteralPath (Join-Path $root 'dist/x64/openOfficetab.dll')).Path
$expected=(Get-FileHash -LiteralPath $candidate).Hash
$dataRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab'
$fileBackups=@{}
foreach($name in 'settings.ini','documents.ini'){
 $path=Join-Path $dataRoot $name
 $fileBackups[$name]=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllBytes($path)}else{$null}
 if($null -ne $fileBackups[$name]){[IO.File]::WriteAllBytes((Join-Path $run ($name+'.before')),$fileBackups[$name])}
}
$registry=@();$changed=@();$app=$null;$api=$null;$fixture=$null;$officePid=0;$newOwnedHwnd=0;$rows=@();$failure=$null;$registryRestored=$true;$stateRestored=$false;$exited=$false
$office=Join-Path $env:ProgramFiles 'Microsoft Office/root/Office16/WINWORD.EXE'
foreach($id in '{65f17021-3b6b-4c5e-b54b-512806431710}','{865f57da-724b-4f9c-a6d6-ac2065c70238}'){
 $key='Software\Classes\CLSID\'+$id+'\InprocServer32'
 $entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key,$true)
 if(-not $entry){throw 'Existing per-user COM registration required; no new registration created.'}
 try{$before=[string]$entry.GetValue('');if(-not (Test-Path -LiteralPath $before)){throw 'Original registered DLL unavailable.'};$registry+=@{Key=$key;Before=$before}}finally{$entry.Dispose()}
}
$registry|Export-Clixml -LiteralPath (Join-Path $run 'registration.before.xml')
function State {$api.Refresh()|Out-Null;return ($api.GetState()|ConvertFrom-Json)}
function Wait-Views([int]$count){
 $until=[DateTime]::UtcNow.AddSeconds(7)
 do{try{$s=State;if($s.views.Count -eq $count -and $s.paneMetrics.Count -eq $count -and @($s.paneMetrics|Where-Object {-not $_.matchesReservedRect -or -not $_.displayVisible}).Count -eq 0){return $s}}catch{};Start-Sleep -Milliseconds 80}while([DateTime]::UtcNow -lt $until)
 throw 'Owned document panes did not settle.'
}
function Editor-Contains([long]$hwnd,[string]$marker){
 if([SelectionProbe]::Process($hwnd) -ne $officePid){throw 'UI Automation root is outside the owned Word process.'}
 $element=[System.Windows.Automation.AutomationElement]::FromHandle([IntPtr]$hwnd)
 $condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::Document)
 $editors=$element.FindAll([System.Windows.Automation.TreeScope]::Descendants,$condition)
 foreach($editor in $editors){
  if($editor.Current.IsOffscreen){continue}
  $pattern=$null
  if($editor.TryGetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern,[ref]$pattern)){
   if($pattern.DocumentRange.GetText(256).Contains($marker)){return $true}
  }
 }
 return $false
}
function Check-Selection([string]$name,[string]$phase,[string]$marker=''){
 if(-not $marker){$marker='OOTT_FIXTURE_'+[IO.Path]::GetFileNameWithoutExtension($name)+'_CONTENT'}
 Set-Content -LiteralPath (Join-Path $run 'phase.txt') -Value $phase
 $until=[DateTime]::UtcNow.AddSeconds(5);$last=''
 do{
  try{
   $s=State;$v=@($s.views|Where-Object title -eq $name)
   $active=$app.ActiveDocument;$window=$app.ActiveWindow
   $hwnd=[long]$window.Hwnd
   $ok=$v.Count -eq 1 -and $s.activeId -eq $v[0].id -and $hwnd -eq [long]$v[0].hwnd -and [SelectionProbe]::Front() -eq $hwnd -and $active.Name -eq $name -and $active.Content.Text.Contains($marker) -and $window.Document.Content.Text.Contains($marker)
   if($ok -and (Editor-Contains $hwnd $marker) -and [OfficeLayoutProbe]::RibbonVisible($hwnd)){
    $script:rows+=@([pscustomobject]@{Phase=$phase;Fixture=[IO.Path]::GetFileNameWithoutExtension($name);ActiveDocument=$true;NativeForeground=$true;VisibleEditorText=$true;RibbonContent=$true;SelectionAgrees=($s.activeWindowMatchesNative -eq $true)})
    return $s
   }
   $last='state='+$s.activeId+'; native='+([SelectionProbe]::Front() -eq $hwnd)+'; expectedDocument='+($active.Name -eq $name)+'; mapped='+$v.Count+'; foreground='+[SelectionProbe]::Front()+'; expectedHwnd='+$hwnd+'; foregroundPid='+[SelectionProbe]::Process([long][SelectionProbe]::Front())
  }catch{$last=$_.Exception.Message}
  Start-Sleep -Milliseconds 80
 }while([DateTime]::UtcNow -lt $until)
 throw ('Selection/content mismatch at '+$phase+': '+$last)
}
function Tab-Click([string]$name){
 $s=State;$front=[SelectionProbe]::Front();$owner=@($s.views|Where-Object {[long]$_.hwnd -eq $front})
 if($owner.Count -ne 1){throw 'Owned source frame must be in front before a tab click.'}
 $pane=@($s.paneMetrics|Where-Object viewId -eq $owner[0].id)[0]
 $index=-1;for($i=0;$i -lt $s.views.Count;$i++){if($s.views[$i].title -eq $name){$index=$i}}
 if($index -lt 0){throw 'Target fixture tab absent.'}
 [SelectionProbe]::Click([long]$pane.controlHwnd,[int]((16+120*$index+40)*$pane.dpi/96),[int]($pane.height/2))
}
try{
 foreach($entry in $registry){$key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Key,$true);try{if($key.GetValue('') -ne $entry.Before){throw 'COM registration changed before test.'};$key.SetValue('',$candidate);$changed+=@($entry)}finally{$key.Dispose()}}
 foreach($letter in 'A','B','C'){
  $path=Join-Path $run ($letter+'.docx');Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures/word-demo.docx') -Destination $path
  $zip=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Update)
  try{
   $entry=$zip.GetEntry('word/document.xml');$reader=New-Object IO.StreamReader($entry.Open());try{$xml=[xml]$reader.ReadToEnd()}finally{$reader.Dispose()}
   $ns=New-Object Xml.XmlNamespaceManager($xml.NameTable);$ns.AddNamespace('w','http://schemas.openxmlformats.org/wordprocessingml/2006/main')
   $paragraph=$xml.SelectSingleNode('//w:body/w:p[1]',$ns);$r=$xml.CreateElement('w','r',$ns.LookupNamespace('w'));$t=$xml.CreateElement('w','t',$ns.LookupNamespace('w'));$t.InnerText='OOTT_FIXTURE_'+$letter+'_CONTENT';$r.AppendChild($t)|Out-Null;$paragraph.AppendChild($r)|Out-Null
   $stream=$entry.Open();$stream.SetLength(0);try{$xml.Save($stream)}finally{$stream.Dispose()}
  }finally{$zip.Dispose()}
 }
 Start-Process -FilePath $office -ArgumentList @('/x','/q',('"'+(Join-Path $run 'A.docx')+'"')) -WindowStyle Hidden|Out-Null
 $until=[DateTime]::UtcNow.AddSeconds(16)
 do{try{$fixture=[OfficeRot]::AttachFile((Join-Path $run 'A.docx'));$app=$fixture.Application;$api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$s=State}catch{};if(-not $s){Start-Sleep -Milliseconds 100}}while(-not $s -and [DateTime]::UtcNow -lt $until)
 if(-not $s -or $app.Documents.Count -ne 1 -or $fixture.FullName -ne (Join-Path $run 'A.docx')){throw 'Isolated Word fixture guard failed.'}
 $officePid=[SelectionProbe]::Process([long]$s.views[0].hwnd)
 if($officePid -in @($baseline|ForEach-Object Id)){throw 'The owned fixture must use a separate Word process.'}
 $loaded=@((Get-Process -Id $officePid).Modules|Where-Object ModuleName -eq 'openOfficetab.dll')
 if($loaded.Count -ne 1 -or (Get-FileHash -LiteralPath $loaded[0].FileName).Hash -ne $expected){throw 'Actual loaded candidate DLL hash mismatch.'}
 $app.Visible=$true
 foreach($letter in 'B','C'){$opened=$app.Documents.Open((Join-Path $run ($letter+'.docx')));$opened=$null}
 $s=Wait-Views 3
 if(-not $s.visible){$api.ToggleVisible()};if(-not $s.group){$api.ToggleGroup()};$api.SetTabWidth(120,0)|Out-Null
 $s=State;$first=@($s.views|Where-Object title -eq 'A.docx')[0];$api.Activate([long]$first.id);[SelectionProbe]::Front([long]$first.hwnd);$s=Check-Selection 'A.docx' 'initial'
 for($cycle=1;$cycle -le $Cycles;$cycle++){
  foreach($letter in 'B','C','A'){Tab-Click ($letter+'.docx');$s=Check-Selection ($letter+'.docx') ('tab-'+$cycle+'-'+$letter)}
 }
 # Keep the earlier deferred-new-document/Ribbon repair under regression too.
 Set-Content -LiteralPath (Join-Path $run 'phase.txt') -Value 'plus-request'
 $source=@($s.views|Where-Object {[long]$_.hwnd -eq [SelectionProbe]::Front()})[0]
 $pane=@($s.paneMetrics|Where-Object viewId -eq $source.id)[0]
 [SelectionProbe]::Click([long]$pane.controlHwnd,[int]((31+120*$s.views.Count)*$pane.dpi/96),[int]($pane.height/2))
 $s=Wait-Views 4;$newDoc=$app.ActiveDocument
 if($newDoc.Path -ne '' -or $newDoc.Content.Text.Trim().Length -ne 0 -or $newDoc.Shapes.Count -ne 0 -or $newDoc.InlineShapes.Count -ne 0){throw 'Expected only this test newly created empty document.'}
 $newOwnedHwnd=[long]$newDoc.Windows.Item(1).Hwnd;$newName=[string]$newDoc.Name
 $newDoc.Content.Text='OOTT_FIXTURE_D_CONTENT';$newDoc=$null
 $s=Check-Selection $newName 'plus-new-document' 'OOTT_FIXTURE_D_CONTENT'
 Tab-Click 'A.docx';$s=Check-Selection 'A.docx' 'after-plus'
 $last=@($s.views|Where-Object title -eq 'A.docx')[0];[SelectionProbe]::Command([long]$last.hwnd,0xF020)
 $until=[DateTime]::UtcNow.AddSeconds(4);do{$minimized=@($s.views|Where-Object {-not [SelectionProbe]::Minimized([long]$_.hwnd)}).Count -eq 0;if(-not $minimized){Start-Sleep -Milliseconds 80}}while(-not $minimized -and [DateTime]::UtcNow -lt $until)
 if(-not $minimized){throw 'Grouped minimize did not settle.'}
 [SelectionProbe]::Command([long]$last.hwnd,0xF120);[SelectionProbe]::Front([long]$last.hwnd);$s=Check-Selection 'A.docx' 'restore'
 $other=@($s.views|Where-Object title -eq 'B.docx')[0];[SelectionProbe]::Front([long]$other.hwnd);$s=Check-Selection 'B.docx' 'native-activation'
}catch{$failure=$_|Out-String}
finally{
 # Canonicalize owned paths. The unsaved '+' fixture additionally requires its
 # captured native root and exact test marker; verify all before closing any.
 if($app){try{
  $safe=$true;$prefix=[IO.Path]::GetFullPath($run)+[IO.Path]::DirectorySeparatorChar
  for($i=1;$i -le $app.Documents.Count;$i++){
   $document=$app.Documents.Item($i);$marker='OOTT_FIXTURE_'+[IO.Path]::GetFileNameWithoutExtension($document.FullName)+'_CONTENT'
   $ownedSaved=$document.Path -ne '' -and [IO.Path]::GetFullPath($document.FullName).StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -and $document.Content.Text.Trim() -eq $marker
   $ownedNew=$newOwnedHwnd -ne 0 -and $document.Path -eq '' -and [long]$document.Windows.Item(1).Hwnd -eq $newOwnedHwnd -and $document.Content.Text.Trim() -in @('','OOTT_FIXTURE_D_CONTENT')
   if(-not ($ownedSaved -or $ownedNew) -or $document.Shapes.Count -ne 0 -or $document.InlineShapes.Count -ne 0 -or $document.Tables.Count -ne 0){$safe=$false}
  };$document=$null
  if($safe){for($i=$app.Documents.Count;$i -ge 1;$i--){$document=$app.Documents.Item($i);$document.Close(0);$document=$null};$app.Quit(0)}
 }catch{if(-not $failure){$failure=$_|Out-String}}}
 $api=$null;$fixture=$null;$app=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers();[GC]::Collect();[GC]::WaitForPendingFinalizers()
 $until=[DateTime]::UtcNow.AddSeconds(6);do{$process=if($officePid){Get-Process -Id $officePid -ErrorAction SilentlyContinue}else{$null};if($process){Start-Sleep -Milliseconds 100}}while($process -and [DateTime]::UtcNow -lt $until);$exited=-not $process
 foreach($entry in $changed){$key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($entry.Key,$true);try{if($key.GetValue('') -eq $candidate){$key.SetValue('',$entry.Before)}else{$registryRestored=$false}}finally{$key.Dispose()}}
 $others=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue)
 $baselinePreserved=$others.Count -eq 0
 if($emptyBaseline -and $others.Count -eq 1 -and $others[0].Id -eq $baseline[0].Id -and $others[0].StartTime.ToUniversalTime().ToString('o') -eq $baselineStart -and $others[0].MainWindowHandle -eq 0){
  try{$emptyApp=[OfficeRot]::Attach('Word');$baselinePreserved=$emptyApp.Documents.Count -eq 0 -and $emptyApp.Windows.Count -eq 0 -and $emptyApp.ProtectedViewWindows.Count -eq 0}finally{$emptyApp=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers()}
 }
 if($exited -and $baselinePreserved){
  foreach($name in $fileBackups.Keys){$path=Join-Path $dataRoot $name;if($null -ne $fileBackups[$name]){[IO.File]::WriteAllBytes($path,$fileBackups[$name])}elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}}
  $stateRestored=$true
 }
 $result=[ordered]@{Passed=(-not $failure -and $rows.Count -eq (3*$Cycles+5) -and $registryRestored -and $stateRestored -and $exited);CandidateHash=$expected;OfficeBuild=if(Test-Path -LiteralPath $office){(Get-Item -LiteralPath $office).VersionInfo.FileVersion}else{''};ProcessId=$officePid;Checks=$rows;ProcessExited=$exited;ExistingEmptyWordPreserved=$baselinePreserved;OriginalRegistrationRestored=$registryRestored;OriginalSettingsAndHistoryRestored=$stateRestored;Failure=$failure;Scope='Word native message tab/+ clicks + foreground HWND + COM document text + visible UIA editor text + NetUI content; no screenshots or manual visual acceptance'}
 $result|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding UTF8
 $result|ConvertTo-Json -Depth 7
}
if(-not $result.Passed){throw ('Word selection runtime test incomplete; preserve evidence at '+$run)}
