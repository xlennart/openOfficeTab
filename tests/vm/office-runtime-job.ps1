param([ValidateSet('Preflight','Run','Cleanup','RecoverFixtures')][string]$Action='Preflight',[Parameter(Mandatory)][string]$JobDirectory)
$ErrorActionPreference='Stop'
$jobDirectory=[IO.Path]::GetFullPath($JobDirectory)
if([IO.Path]::GetDirectoryName($jobDirectory) -ne 'C:\OOT-QA\runs' -or [IO.Path]::GetFileName($jobDirectory) -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$'){throw 'Invalid guest job directory.'}
$request=Get-Content (Join-Path $jobDirectory 'request.json') -Raw|ConvertFrom-Json
$jobId=[IO.Path]::GetFileName($jobDirectory);$root=Join-Path $jobDirectory 'package'
if($request.schema -ne 1 -or $request.jobId -ne $jobId){throw 'Request identity mismatch.'}
Add-Type @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class OfficeVmDesktop {
 [DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint flags);
 [DllImport("user32.dll")] static extern IntPtr OpenInputDesktop(uint f,bool i,uint a);
 [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr d);
 [DllImport("user32.dll")] static extern bool GetUserObjectInformation(IntPtr o,int i,StringBuilder b,int n,out int r);
 [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
 public static bool Unlocked(){var d=OpenInputDesktop(0,false,1);if(d==IntPtr.Zero)return false;try{var b=new StringBuilder(256);int n;return GetUserObjectInformation(d,2,b,512,out n)&&b.ToString()=="Default";}finally{CloseDesktop(d);}}
 delegate bool Callback(IntPtr w,IntPtr p);
 struct Rect{public int L,T,R,B;}
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback c,IntPtr p);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr w,StringBuilder b,int n);
 public static long[] CenterWindows(int pid){var found=new List<long>();EnumWindows((w,p)=>{uint owner;GetWindowThreadProcessId(w,out owner);Rect r;var b=new StringBuilder(256);GetWindowText(w,b,256);if(owner==pid&&IsWindowVisible(w)&&GetWindowRect(w,out r)&&r.R-r.L>=400&&r.B-r.T>=400&&b.ToString().IndexOf("openOfficeTab",StringComparison.OrdinalIgnoreCase)>=0)found.Add(w.ToInt64());return true;},IntPtr.Zero);return found.ToArray();}
 public static bool Close(long w,int pid){uint owner;GetWindowThreadProcessId(new IntPtr(w),out owner);return owner==pid&&PostMessage(new IntPtr(w),0x10,IntPtr.Zero,IntPtr.Zero);}
}
'@
function Preflight {
 return @{jobId=$jobId;status='complete';utc=[DateTime]::UtcNow.ToString('o');unlocked=[OfficeVmDesktop]::Unlocked();session=(Get-Process -Id $PID).SessionId;dpi=[OfficeVmDesktop]::GetDpiForSystem();officeCount=@(Get-Process WINWORD,EXCEL,POWERPNT -ErrorAction SilentlyContinue).Count;centerCount=@(Get-Process openOfficetab.Center -ErrorAction SilentlyContinue).Count;sleepPolicy=(& powercfg /query SCHEME_CURRENT SUB_SLEEP STANDBYIDLE|Out-String)}
}
if($Action -eq 'Preflight'){Preflight|ConvertTo-Json -Depth 6|Set-Content (Join-Path $jobDirectory 'preflight.json') -Encoding UTF8;return}
if($Action -eq 'Cleanup'){
 $cleanup=@{jobId=$jobId;status='blocked';utc=[DateTime]::UtcNow.ToString('o')}
 try{
  $ready=Preflight;if($ready.officeCount -or $ready.centerCount){throw 'Application still running; cleanup stopped.'}
  $state=Get-Content (Join-Path $jobDirectory 'result.json') -Raw|ConvertFrom-Json
  if($state.jobId -ne $jobId -or $state.status -eq 'running'){throw 'Job is not complete.'}
  $resolved=(Resolve-Path -LiteralPath $jobDirectory).Path
  if($resolved -ne $jobDirectory -or ((Get-Item $resolved).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Cleanup root mismatch.'}
  if(@(Get-ChildItem $resolved -Force -Recurse|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Cleanup contains links.'}
  Remove-Item -LiteralPath $resolved -Recurse -Force;$cleanup.status='passed'
 }catch{$cleanup.error=$_.Exception.Message}
 $cleanup|ConvertTo-Json|Set-Content ('C:\OOT-QA\cleanup-'+$jobId+'.json') -Encoding UTF8;return
}
$report=@{jobId=$jobId;version=$request.version;status='running';startedUtc=[DateTime]::UtcNow.ToString('o');executorPid=$PID;executorStartUtc=(Get-Process -Id $PID).StartTime.ToUniversalTime().ToString('o');stages=@()}
function Write-Report([string]$name){
 # The host may be copying the previous report. Never truncate that file;
 # atomically replace it, allowing a bounded wait for a non-delete-sharing reader.
 $target=Join-Path $jobDirectory $name;$temporary=$target+'.'+$PID+'.tmp'
 $text=$report|ConvertTo-Json -Depth 12
 [IO.File]::WriteAllText($temporary,$text,[Text.UTF8Encoding]::new($true))
 for($attempt=0;$attempt -lt 20;$attempt++){
  try{if(Test-Path -LiteralPath $target){[IO.File]::Replace($temporary,$target,[Management.Automation.Language.NullString]::Value)}else{[IO.File]::Move($temporary,$target)};return}
  catch [IO.IOException]{if($attempt -eq 19){throw};Start-Sleep -Milliseconds 200}
 }
}
function Report {Write-Report 'result.json'}
function Ready {if(-not [OfficeVmDesktop]::Unlocked() -or (Get-Process -Id $PID).SessionId -eq 0){throw 'Interactive desktop is locked or unavailable.'}}
function Run([string]$label,[string]$file,[string[]]$extra){
 Ready
 $start=[Diagnostics.ProcessStartInfo]::new("$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe")
 $start.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$file+'" '+($extra -join ' ')
 $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 $child=[Diagnostics.Process]::Start($start);$stdout=$child.StandardOutput.ReadToEndAsync();$stderr=$child.StandardError.ReadToEndAsync()
 $started=$child.StartTime.ToUniversalTime().ToString('o');$childId=$child.Id
 if(-not $child.WaitForExit(180000)){throw "Stage timed out: $label; test client PID $childId and Office windows retained."}
 [IO.File]::WriteAllText((Join-Path $jobDirectory ($label+'.log')),$stdout.GetAwaiter().GetResult())
 [IO.File]::WriteAllText((Join-Path $jobDirectory ($label+'.error')),$stderr.GetAwaiter().GetResult())
 $report.stages+=@{name=$label;pid=$childId;startedUtc=$started;exitCode=$child.ExitCode;status=$(if($child.ExitCode -eq 0){'passed'}else{'failed'})};Report
 if($child.ExitCode -ne 0){throw "Stage failed: $label"}
}
function Close-Fixtures {
 . (Join-Path $root 'tests/common.ps1')
 foreach($hostName in 'Word','Excel','PowerPoint'){
  $expected=Join-Path $root ('tests/fixtures/'+@{Word='word-demo.docx';Excel='excel-demo.xlsx';PowerPoint='powerpoint-demo.pptx'}[$hostName])
  $app=[OfficeRot]::Attach($hostName)
  if($hostName -eq 'Word'){$docs=$app.Documents}elseif($hostName -eq 'Excel'){$docs=$app.Workbooks}else{$docs=$app.Presentations}
  if($docs.Count -ne 1 -or $docs.Item(1).FullName -ne $expected -or -not $docs.Item(1).Saved){throw 'Unknown/edited document preserved during cleanup.'}
  $api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$state=$api.GetState()|ConvertFrom-Json
  $process=@(Get-Process @{Word='WINWORD';Excel='EXCEL';PowerPoint='POWERPNT'}[$hostName]);if($process.Count -ne 1){throw 'Ambiguous fixture process.'}
  $nativeProcess=$process[0];$started=$nativeProcess.StartTime.ToUniversalTime().ToString('o');$processId=$nativeProcess.Id
  $api.Close([long]$state.views[0].id)|Out-Null
  foreach($value in @($api,$docs,$app)){if($value -and [Runtime.InteropServices.Marshal]::IsComObject($value)){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($value)}}
  $api=$null;$docs=$null;$app=$null;[GC]::Collect();[GC]::WaitForPendingFinalizers()
  if(-not $nativeProcess.WaitForExit(30000)){throw "Owned fixture did not exit normally: $hostName"}
  $report.stages+=@{name=('fixture-exit-'+$hostName);pid=$processId;startedUtc=$started;status='passed'};Report
 }
}
if($Action -eq 'RecoverFixtures'){
 $originalResult=Join-Path $jobDirectory 'result.json'
 $before=Get-Content $originalResult -Raw|ConvertFrom-Json
 if($before.jobId -ne $jobId -or $before.status -eq 'running'){throw 'Only completed jobs may recover owned fixtures.'}
 $report=@{jobId=$jobId;status='running';stages=@();action='recover-fixtures'}
 # Keep the initial failed result immutable. Recovery has its own report.
 function Report {Write-Report 'recovery.json'}
 try{Report;Ready;Close-Fixtures;$report.status='passed'}catch{$report.status='failed';$report.error=$_.Exception.Message}finally{Report}
 return
}
$mutex=[Threading.Mutex]::new($false,'Local\openOfficetab.OfficeVmRuntime');$held=$false;$awake=0
try{
 Report;try{$held=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$held=$true}
 if(-not $held){throw 'Another guest runtime job holds the serial executor.'}
 $ready=Preflight;$report.preflight=$ready
 if(-not $ready.unlocked -or $ready.session -eq 0 -or $ready.officeCount -or $ready.centerCount){throw 'Guest is locked, noninteractive or busy.'}
 $awake=[OfficeVmDesktop]::SetThreadExecutionState(2147483651);if(-not $awake){throw 'Wake request failed.'}
 foreach($pair in @(@('setup.exe','installerSha256'),@('package.zip','packageSha256'),@('qa.zip','qaSha256'))){if((Get-FileHash (Join-Path $jobDirectory $pair[0])).Hash -ne $request.($pair[1])){throw 'Input file hash mismatch.'}}
 Expand-Archive (Join-Path $jobDirectory 'package.zip') $root -Force;Expand-Archive (Join-Path $jobDirectory 'qa.zip') $root -Force
 # Reusable public fixtures survive removal of this job's temporary package.
 $review=Join-Path 'C:\OOT-QA\fixtures' $jobId
 if(Test-Path -LiteralPath $review){throw 'Review fixture directory already exists; preserved.'}
 New-Item -ItemType Directory -Path $review|Out-Null
 foreach($name in 'word-demo.docx','excel-demo.xlsx','powerpoint-demo.pptx'){Copy-Item -LiteralPath (Join-Path $root ('tests/fixtures/'+$name)) -Destination $review}
 $report.reviewFixtureDirectory=$review
 New-Item -ItemType Directory -Force (Join-Path $root 'artifacts')|Out-Null
 foreach($architecture in 'x86','x64'){if((Get-FileHash (Join-Path $root "dist/$architecture/openOfficetab.dll")).Hash -ne $request.($architecture+'Sha256')){throw 'Native payload hash mismatch.'}}
 Add-Type -Path (Join-Path $root 'service/openOfficetab.Service.dll')
 if([OpenOfficeTab.Desktop.Api]::OfficeArchitecture() -ne 'x86'){throw 'Actual Office PE architecture must be x86.'}
 $report.architecture='x86';$report.x86Sha256=$request.x86Sha256;$report.installerSha256=$request.installerSha256;$report.os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber)
 Ready
 $setup=Start-Process (Join-Path $jobDirectory 'setup.exe') -ArgumentList @('--silent','--language','en','--result-file',('"'+(Join-Path $jobDirectory 'install-result.json')+'"')) -WindowStyle Hidden -PassThru
 $null=$setup.Handle
 if(-not $setup.WaitForExit(120000) -or $setup.ExitCode -ne 0){throw 'Unified installer failed or timed out.'}
 $installed=Get-Content (Join-Path $jobDirectory 'install-result.json') -Raw|ConvertFrom-Json
 if(-not $installed.ok){throw 'Unified installer reported failure.'}
 $report.stages+=@{name='unified-install';pid=$setup.Id;exitCode=$setup.ExitCode;status='passed'};Report
 Run 'registration' (Join-Path $root 'tests/desktop-install-runtime.ps1') @('-Architecture','x86','-ExpectedVersion',$request.version)
 foreach($hostName in 'Word','Excel','PowerPoint'){Run ('lifecycle-'+$hostName) (Join-Path $root 'tests/lifecycle-process-runtime.ps1') @('-HostName',$hostName)}
 Run 'save' (Join-Path $root 'tests/save-suite-runtime.ps1') @('-Architecture','x86','-Root',$root)
 Run 'open-fixtures' (Join-Path $root 'tests/desktop-install-runtime.ps1') @('-Architecture','x86','-ExpectedVersion',$request.version,'-OpenFixtures')
 try{Run 'language' (Join-Path $root 'tests/language-runtime.ps1') @('-Architecture','x86','-ExpectedVersion',$request.version)}finally{Close-Fixtures}
 # Entry-point integration only: full management-center UI coverage is on
 # the x64 host. Exclude Tauri's hidden resize window by visibility and size.
 $centerPath=(Get-ItemProperty HKCU:\Software\openOfficetab).CenterPath
 if((Get-FileHash $centerPath).Hash -ne $request.installerSha256){throw 'Installed center hash mismatch.'}
 $center=Start-Process $centerPath -ArgumentList @('--page','overview') -PassThru;$null=$center.Handle
 $until=[DateTime]::UtcNow.AddSeconds(25)
 do{$windows=@([OfficeVmDesktop]::CenterWindows($center.Id));if($windows.Count -eq 1){break};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $until)
 if($windows.Count -ne 1){throw 'Unique visible management-center window not found.'}
 if(-not [OfficeVmDesktop]::Close($windows[0],$center.Id) -or -not $center.WaitForExit(30000)){throw 'Owned center did not close and exit normally.'}
 $report.stages+=@{name='center-entry-and-exit';pid=$center.Id;hwnd=$windows[0];exitCode=$center.ExitCode;status='passed'};Report
 Run 'repair' (Join-Path $root 'scripts/diagnose.ps1') @('-Repair','-Language','en')
 $ini=Join-Path $env:LOCALAPPDATA 'openOfficetab/settings.ini';$settingsHash=if(Test-Path $ini){(Get-FileHash $ini).Hash}else{$null}
 Run 'uninstall' (Join-Path $root 'scripts/uninstall.ps1') @('-Language','en')
 if($settingsHash -and (Get-FileHash $ini).Hash -ne $settingsHash){throw 'Settings-preserving uninstall changed the INI.'}
 Run 'reinstall' (Join-Path $root 'scripts/install.ps1') @('-Language','auto','-DesktopShortcut')
 Run 'final-registration' (Join-Path $root 'tests/desktop-install-runtime.ps1') @('-Architecture','x86','-ExpectedVersion',$request.version)
 $report.status='passed'
}catch{$report.status='failed';$report.error=$_.Exception.Message;$report.stack=$_.ScriptStackTrace}
finally{
 if($awake){[void][OfficeVmDesktop]::SetThreadExecutionState($awake)}
 if($held){$mutex.ReleaseMutex()};$mutex.Dispose();$report.wakeReleased=$true;$report.endedUtc=[DateTime]::UtcNow.ToString('o');Report
 $files=@(Get-ChildItem $jobDirectory -File -Recurse|Where-Object {$_.Length -gt 0 -and ($_.Extension -in '.json','.log','.error') -and $_.Name -ne 'evidence-index.json'}|ForEach-Object {@{path=$_.FullName.Substring($jobDirectory.Length+1);length=$_.Length;sha256=(Get-FileHash $_.FullName).Hash}})
 @{jobId=$jobId;files=$files}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $jobDirectory 'evidence-index.json') -Encoding UTF8
}
