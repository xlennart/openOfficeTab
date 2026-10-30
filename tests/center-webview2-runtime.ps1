param([string]$PlaywrightModule,[string]$NodeExecutable='node',[string]$JobId=([guid]::NewGuid().ToString('N')))
$ErrorActionPreference='Stop'
if($JobId -notmatch '^[a-f0-9]{32}$'){throw 'Invalid runtime job identity.'}
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-service.ps1') -Library
if(-not $PlaywrightModule){$PlaywrightModule=Join-Path $PSScriptRoot 'webview2/node_modules/playwright-core'}
if(-not(Test-Path (Join-Path $PlaywrightModule 'package.json'))){throw 'Run npm ci --prefix tests/webview2 first.'}
Add-Type @'
using System;using System.Runtime.InteropServices;using System.Text;
public static class CenterRuntimeWindow {
 [StructLayout(LayoutKind.Sequential)]struct Rect{public int Left,Top,Right,Bottom;}
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr window,out Rect value);
 [DllImport("user32.dll")]static extern bool GetClientRect(IntPtr window,out Rect value);
 [DllImport("user32.dll")]static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
 [DllImport("user32.dll")]static extern bool GetUserObjectInformation(IntPtr obj,int index,StringBuilder text,int length,out int needed);
 [DllImport("user32.dll")]static extern bool CloseDesktop(IntPtr desktop);
 [DllImport("user32.dll")]static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr window);
 [DllImport("user32.dll")]static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("kernel32.dll")]public static extern uint SetThreadExecutionState(uint flags);
 public static string Desktop(){var d=OpenInputDesktop(0,false,1);if(d==IntPtr.Zero)return "unavailable";try{var text=new StringBuilder(256);int n;return GetUserObjectInformation(d,2,text,512,out n)?text.ToString():"unknown";}finally{CloseDesktop(d);}}
 public static bool Minimum(IntPtr window){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{uint dpi=GetDpiForWindow(window);Rect outer,client;if(!GetWindowRect(window,out outer)||!GetClientRect(window,out client))return false;int borderWidth=outer.Right-outer.Left-client.Right+client.Left,borderHeight=outer.Bottom-outer.Top-client.Bottom+client.Top;return SetWindowPos(window,IntPtr.Zero,0,0,(int)(420*dpi/96)+borderWidth,(int)(480*dpi/96)+borderHeight,0x16);}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
$jobRoot=Join-Path $root "artifacts/runtime-tests/$JobId";New-Item -ItemType Directory -Force $jobRoot|Out-Null
$record=[ordered]@{jobId=$JobId;status='running';startedUtc=[DateTime]::UtcNow.ToString('o');sessionId=(Get-Process -Id $PID).SessionId;desktop=[CenterRuntimeWindow]::Desktop();launcherPid=$PID;driver='playwright-webview2-cdp'}
$app=$null;$driver=$null;$awake=0;$original=$null
try{
 if($record.sessionId -eq 0 -or $record.desktop -ne 'Default'){throw 'BLOCKED: unlock the interactive desktop first.'}
 if(Get-Process openOfficetab.Center -ErrorAction SilentlyContinue){throw 'BLOCKED: existing management center preserved.'}
 $center=(Get-ItemProperty HKCU:\Software\openOfficetab).CenterPath
 $expected=Join-Path $root 'app/openOfficetab.Center.exe';if(-not(Test-Path $expected)){$expected=Join-Path $root 'dist/desktop/openOfficetab.Center.exe'}
 $record.exeSha256=(Get-FileHash $center).Hash;if($record.exeSha256 -ne (Get-FileHash $expected).Hash){throw 'Installed center does not match this candidate.'}
 $record.officeArchitecture=[OpenOfficeTab.Desktop.Api]::OfficeArchitecture()
 $record.dllHashes=@(foreach($architecture in 'x64','x86'){@{architecture=$architecture;sha256=(Get-FileHash (Join-Path $root "dist/$architecture/openOfficetab.dll")).Hash}})
 $original=Get-OotPreferences;$originalLanguage=([OpenOfficeTab.Desktop.Api]::Language($script:DataRoot)|ConvertFrom-Json).mode
 $record.sleepPolicy=(& powercfg.exe /query SCHEME_CURRENT SUB_SLEEP STANDBYIDLE|Out-String)
 $awake=[CenterRuntimeWindow]::SetThreadExecutionState(2147483651);if(-not $awake){throw 'Failed to hold the test job awake.'}
 $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop();$record.cdpPort=$port
 $start=[Diagnostics.ProcessStartInfo]::new($center);$start.Arguments='--page updates --language en';$start.UseShellExecute=$false
 $start.EnvironmentVariables['WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS']="--remote-debugging-address=127.0.0.1 --remote-debugging-port=$port"
 $start.EnvironmentVariables['WEBVIEW2_USER_DATA_FOLDER']=Join-Path $jobRoot 'webview-profile'
 $app=[Diagnostics.Process]::Start($start);$record.pid=$app.Id;$record.processStartUtc=$app.StartTime.ToUniversalTime().ToString('o')
 $ready=$false;$deadline=[DateTime]::UtcNow.AddSeconds(30)
 do{
  if($app.HasExited){throw 'Center exited before its test endpoint was ready.'}
  $bindings=@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()|Where-Object Port -eq $port)
  foreach($binding in $bindings){if(-not [Net.IPAddress]::IsLoopback($binding.Address)){throw 'CDP listener is not restricted to loopback.'}}
  $record.cdpBindings=@($bindings|ForEach-Object {$_.Address.ToString()})
  if($bindings.Count){try{$probe=[Net.HttpWebRequest]::Create("http://127.0.0.1:$port/json/version");$probe.Proxy=$null;$probe.Timeout=1000;$response=$probe.GetResponse();try{$ready=([int]$response.StatusCode -eq 200)}finally{$response.Dispose()}}catch{$record.cdpProbeError=[string]$_}}
  if(-not $ready){Start-Sleep -Milliseconds 100}
 }while(-not $ready -and [DateTime]::UtcNow -lt $deadline)
 if(-not $ready){$record.webviewProcesses=@(Get-CimInstance Win32_Process -Filter "Name='msedgewebview2.exe'" -OperationTimeoutSec 5|Where-Object {$_.ParentProcessId -eq $app.Id -or ($_.CommandLine -and $_.CommandLine.Contains($jobRoot))}|ForEach-Object {@{pid=$_.ProcessId;parent=$_.ParentProcessId;testProfile=($_.CommandLine -and $_.CommandLine.Contains($jobRoot));debugArgument=($_.CommandLine -match '--remote-debugging-port=');version=[Diagnostics.FileVersionInfo]::GetVersionInfo($_.ExecutablePath).FileVersion}});throw 'Test-only CDP endpoint did not become ready.'}
 $record.cdpLoopbackOnly=$true;$app.Refresh();$record.dpi=[CenterRuntimeWindow]::GetDpiForWindow($app.MainWindowHandle)
 $node=[Diagnostics.ProcessStartInfo]::new($NodeExecutable);$node.UseShellExecute=$false;$node.CreateNoWindow=$true;$node.RedirectStandardOutput=$true;$node.RedirectStandardError=$true
 $node.Arguments=(@((Join-Path $PSScriptRoot 'center-webview2-runtime.cjs'),([IO.Path]::GetFullPath($PlaywrightModule)),[string]$port,$jobRoot,$JobId,'0.2.3-beta.1')|ForEach-Object {'"'+$_+'"'}) -join ' '
 $driver=[Diagnostics.Process]::Start($node);$stdout=$driver.StandardOutput.ReadToEndAsync();$stderr=$driver.StandardError.ReadToEndAsync()
 $resized=$false;$deadline=[DateTime]::UtcNow.AddSeconds(240)
 while(-not $driver.HasExited -and [DateTime]::UtcNow -lt $deadline){
  $request=Join-Path $jobRoot 'resize-request.json'
  if(-not $resized -and(Test-Path $request)){
   $resize=Get-Content $request -Raw|ConvertFrom-Json
   if($resize.jobId -ne $JobId -or $resize.width -ne 420 -or $resize.height -ne 480){throw 'Unbound native resize request rejected.'}
   $app.Refresh();if(-not [CenterRuntimeWindow]::Minimum($app.MainWindowHandle)){throw 'Native minimum-size resize failed.'}
   @{jobId=$JobId;done=$true}|ConvertTo-Json|Set-Content (Join-Path $jobRoot 'resize-result.json') -Encoding ASCII;$resized=$true
  };Start-Sleep -Milliseconds 100
 }
 if(-not $driver.HasExited){throw 'Driver exceeded its job deadline.'}
 $stdout.GetAwaiter().GetResult()|Set-Content (Join-Path $jobRoot 'driver.log') -Encoding UTF8;$stderr.GetAwaiter().GetResult()|Set-Content (Join-Path $jobRoot 'driver.error') -Encoding UTF8
 $ui=Get-Content (Join-Path $jobRoot 'ui-result.json') -Raw|ConvertFrom-Json
 if($driver.ExitCode -ne 0 -or $ui.status -ne 'ui_passed'){throw ('WebView2 UI failed: '+$ui.error)}
 if(-not $app.WaitForExit(30000)){throw 'Window closed but its owned process remains.'}
 $record.processExited=$true;$record.windowClosed=$ui.windowClosed;$record.checks=$ui.checks;$record.autoLanguagePassed=$ui.autoLanguagePassed;$record.status='passed'
}catch{$record.status=if([string]$_ -like '*BLOCKED:*'){'blocked'}else{'failed'};$record.error=[string]$_;throw}
finally{
 if($driver -and -not $driver.HasExited){$driver.Kill();$driver.WaitForExit()}
 if($original){try{[void](Invoke-OotDesktopAction preferences $original);[void](Invoke-OotDesktopAction language @{mode=[int]$originalLanguage});$record.preferencesRestored=$true}catch{$record.status='failed';$record.cleanupError=[string]$_}}
 if($app -and -not $app.HasExited){if(-not $app.CloseMainWindow() -or -not $app.WaitForExit(30000)){$record.status='failed';$record.cleanupError='Owned center did not exit normally.'}}
 $profile=Join-Path $jobRoot 'webview-profile'
 if($app -and $app.HasExited -and(Test-Path -LiteralPath $profile)){
  try{
   $resolved=(Resolve-Path -LiteralPath $profile).Path
   if(-not [string]::Equals([IO.Path]::GetDirectoryName($resolved),[IO.Path]::GetFullPath($jobRoot),[StringComparison]::OrdinalIgnoreCase) -or ((Get-Item -LiteralPath $resolved).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Profile cleanup identity mismatch.'}
   Remove-Item -LiteralPath $resolved -Recurse -Force;$record.profileCleaned=$true
  }catch{$record.profileCleanupDeferred=$true}
 }
 if($awake){[void][CenterRuntimeWindow]::SetThreadExecutionState($awake)}
 $record.finishedUtc=[DateTime]::UtcNow.ToString('o');$record|ConvertTo-Json -Depth 8|Set-Content (Join-Path $jobRoot 'result.json') -Encoding UTF8
 Write-Output (Join-Path $jobRoot 'result.json')
}
if($record.status -ne 'passed'){throw 'Runtime QA cleanup failed.'}
