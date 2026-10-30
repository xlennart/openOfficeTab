param([ValidateSet('en','zh-CN')][string]$Language='en',[ValidateSet('default','minimum')][string]$Size='default',[ValidateSet('beta','stable')][string]$Channel='beta',[switch]$LayoutOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'scripts/desktop-service.ps1') -Library -Language $Language
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class NativeChannelWindow{
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [DllImport("kernel32.dll")]public static extern uint SetThreadExecutionState(uint flags);
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")]static extern IntPtr GetAncestor(IntPtr w,uint flags);
 [DllImport("user32.dll")]static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")]static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 static void Key(byte code){keybd_event(code,0,0,UIntPtr.Zero);keybd_event(code,0,2,UIntPtr.Zero);}
 public static void Choose(IntPtr w,int x,int y,bool beta){
  SetForegroundWindow(w);
  SetWindowPos(w,IntPtr.Zero,0,0,0,0,0x13);
  if(GetAncestor(WindowFromPoint(new Point{X=x,Y=y}),2)!=w)throw new InvalidOperationException("Owned control is occluded; input stopped.");
  SetCursorPos(x,y);mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);
  for(int i=0;i<10&&GetForegroundWindow()!=w;i++)System.Threading.Thread.Sleep(50);
  if(GetForegroundWindow()!=w)throw new InvalidOperationException("Owned click did not establish foreground; keyboard input stopped.");
  Key(0x24);if(beta)Key(0x28);Key(0x0d);
 }
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]static extern bool GetClientRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]static extern bool ClientToScreen(IntPtr w,ref Point p);
 [DllImport("user32.dll")]static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr w);
 [DllImport("user32.dll")]static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 public static Rect Client(IntPtr w){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{Rect r;GetClientRect(w,out r);var p=new Point();ClientToScreen(w,ref p);r.Right+=p.X;r.Bottom+=p.Y;r.Left=p.X;r.Top=p.Y;return r;}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static bool Minimum(IntPtr w){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{Rect a,b;GetWindowRect(w,out a);GetClientRect(w,out b);uint d=GetDpiForWindow(w);return SetWindowPos(w,IntPtr.Zero,0,0,(int)(420*d/96)+a.Right-a.Left-b.Right,(int)(480*d/96)+a.Bottom-a.Top-b.Bottom,0x16);}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
$job=[guid]::NewGuid().ToString('N');$out=Join-Path $root "artifacts/runtime-tests/$job";New-Item -ItemType Directory -Force $out|Out-Null
$report=[ordered]@{jobId=$job;status='running';layoutOnly=[bool]$LayoutOnly;driver='native-owned-keyboard';language=$Language;startedUtc=[DateTime]::UtcNow.ToString('o');checks=@()}
function Await([scriptblock]$query){$end=[DateTime]::UtcNow.AddSeconds(15);do{$v=&$query;if($v){return $v};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow -lt $end);throw 'Native channel condition timed out.'}
function Named($parent,[string]$name){$parent.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name))}
function Combo($parent,[string]$name){$parent.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::ComboBox))|Where-Object {$_.Current.Name -eq $name -and $_.Current.IsEnabled}|Select-Object -First 1}
function ComboValue($parent,[string]$name){$parent.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::ComboBox))|Where-Object {$_.Current.Name -eq $name}|Select-Object -First 1}
$original=Get-OotPreferences;$app=$null;$wake=0
try{
 if((Get-Process -Id $PID).SessionId -eq 0){throw 'BLOCKED: interactive desktop required.'}
 if(Get-Process openOfficetab.Center -ErrorAction SilentlyContinue){throw 'BLOCKED: existing center preserved.'}
 if(Get-Process LogonUI -ErrorAction SilentlyContinue|Where-Object SessionId -eq (Get-Process -Id $PID).SessionId){throw 'BLOCKED: unlock the desktop first.'}
 [void](Invoke-OotDesktopAction preferences @{autoCheck=$false;autoDownload=$false;autoInstall=$false;updateChannel=$(if($LayoutOnly){$Channel}elseif($Channel -eq 'beta'){'stable'}else{'beta'})})
 $center=(Get-ItemProperty HKCU:\Software\openOfficetab).CenterPath;$expected=Join-Path $root 'app/openOfficetab.Center.exe';if(-not(Test-Path $expected)){$expected=Join-Path $root 'dist/desktop/openOfficetab.Center.exe'}
 $report.exeSha256=(Get-FileHash $center).Hash;if($report.exeSha256 -ne (Get-FileHash $expected).Hash){throw 'Candidate identity mismatch.'}
 $wake=[NativeChannelWindow]::SetThreadExecutionState(2147483651);if(-not $wake){throw 'Wake request failed.'}
 $report.officeArchitecture=[OpenOfficeTab.Desktop.Api]::OfficeArchitecture()
 $report.sleepPolicy=(& powercfg.exe /query SCHEME_CURRENT SUB_SLEEP STANDBYIDLE|Out-String)
 $report.stage='launch'
 $app=Start-Process $center -ArgumentList @('--page','updates','--language',$Language) -PassThru -WindowStyle Normal
 $report.pid=$app.Id;$report.processStartUtc=$app.StartTime.ToUniversalTime().ToString('o')
 $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,$app.Id)
 $window=Await {[Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,$condition)|Where-Object {$_.Current.Name -match 'Management center|管理中心' -and $_.Current.BoundingRectangle.Width -gt 300}|Select-Object -First 1}
 $handle=[IntPtr]$window.Current.NativeWindowHandle
 $report.dpi=[NativeChannelWindow]::GetDpiForWindow($handle)
 $report.stage='version'
 $label=if($Language -eq 'en'){'Update channel'}else{'更新通道'}
 $version=if($Language -eq 'en'){'Management center 0.2.3-beta.1'}else{'管理中心 0.2.3-beta.1'}
 [void](Await {Named $window $version})
 foreach($size in @($Size)){
  if($size -eq 'minimum'){if(-not [NativeChannelWindow]::Minimum($handle)){throw 'Client resize failed.'}}
  foreach($channel in @($Channel)){
   $report.stage='keyboard-channel'
   $text=if($Language -eq 'en'){if($channel -eq 'beta'){'Beta'}else{'Stable'}}else{if($channel -eq 'beta'){'Beta 版'}else{'正式版'}}
   $combo=Await {Combo $window $label}
   $combo.GetCurrentPattern([Windows.Automation.ScrollItemPattern]::Pattern).ScrollIntoView()
   [void](Await {$script:currentCombo=ComboValue $window $label;$b=$script:currentCombo.Current.BoundingRectangle;$c=[NativeChannelWindow]::Client($handle);if($b.Width -gt 0 -and $b.Left -ge $c.Left -and $b.Right -le $c.Right -and $b.Top -ge $c.Top -and $b.Bottom -le $c.Bottom){$true}})
   $bounds=$script:currentCombo.Current.BoundingRectangle
   if(-not $LayoutOnly){[NativeChannelWindow]::Choose($handle,[int]($bounds.Left+$bounds.Width/2),[int]($bounds.Top+$bounds.Height/2),($channel -eq 'beta'))}
   $report.stage='persisted-channel'
   [void](Await {if((Get-OotPreferences).updateChannel -eq $channel){$true}})
   $combo=Await {ComboValue $window $label};$bounds=$combo.Current.BoundingRectangle;$client=[NativeChannelWindow]::Client($handle)
   if($combo.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).Current.Value -ne $text){throw 'Rendered channel and saved preference disagree.'}
   if($bounds.Width -le 0 -or $bounds.Left -lt $client.Left -or $bounds.Right -gt $client.Right -or $bounds.Top -lt $client.Top -or $bounds.Bottom -gt $client.Bottom){throw 'Channel extends outside native client area.'}
   $report.checks+=@{size=$size;channel=$channel;persisted=$true;insideClient=$true;inputTested=(-not [bool]$LayoutOnly)}
  }
  $client=[NativeChannelWindow]::Client($handle);$bitmap=[Drawing.Bitmap]::new($client.Right-$client.Left,$client.Bottom-$client.Top);$graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.CopyFromScreen($client.Left,$client.Top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $out "$size.png"),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
 }
 [void](Await {if(-not @(Get-CimInstance Win32_Process -Filter ("ParentProcessId="+$app.Id)|Where-Object Name -eq 'powershell.exe')){$true}})
 $report.status='passed'
}catch{$report.status=if([string]$_ -like '*BLOCKED:*'){'blocked'}else{'failed'};$report.error=[string]$_;$report.stack=$_.ScriptStackTrace;throw}
finally{
 [void](Invoke-OotDesktopAction preferences $original)
 if($app -and -not $app.HasExited){$app.Refresh();if(-not $app.CloseMainWindow() -or -not $app.WaitForExit(30000)){$report.status='failed';$report.cleanupError='Owned center did not exit normally.'}}
 if($wake){[void][NativeChannelWindow]::SetThreadExecutionState(2147483648)}
 $report.processExited=($app -and $app.HasExited);$report.finishedUtc=[DateTime]::UtcNow.ToString('o');$report|ConvertTo-Json -Depth 7|Set-Content (Join-Path $out 'result.json') -Encoding UTF8;Write-Output (Join-Path $out 'result.json')
}
if($report.status -ne 'passed'){throw 'Native channel run failed.'}
