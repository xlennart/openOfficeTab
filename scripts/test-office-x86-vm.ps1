param(
 [ValidateSet('Preflight','Run','Collect','Cleanup','Shutdown','RecoverFixtures')][string]$Action='Preflight',
 [string]$JobId=('office-x86-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)),
 [string]$VMX='D:\VMs\Win11-LTSC-OfficeX86\Win11-LTSC-OfficeX86.vmx',
 [string]$Vmrun='C:\Program Files\VMware\VMware Workstation\vmrun.exe',
 [string]$CredentialPath=(Join-Path $env:USERPROFILE 'AppData\Local\openOfficetab.Development\office-x86.credential.xml'),
 [string]$DeliveryDirectory,[switch]$KeepVMRunning,[int]$TimeoutMinutes=20
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if($JobId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$'){throw 'Invalid jobId.'}
$version=(Get-Content (Join-Path $root 'desktop/package.json') -Raw|ConvertFrom-Json).version
$directory=[IO.Path]::GetFullPath((Join-Path $root ('artifacts/runtime-vm/'+$JobId)));New-Item -ItemType Directory -Force $directory|Out-Null
function Host-Phase([string]$name){
 @{jobId=$JobId;phase=$name;utc=[DateTime]::UtcNow.ToString('o');pid=$PID}|ConvertTo-Json|Set-Content (Join-Path $directory 'host-phase.json') -Encoding UTF8
}
$guestDirectory='C:\OOT-QA\runs\'+$JobId
if(-not(Test-Path -LiteralPath $VMX) -or -not(Test-Path -LiteralPath $Vmrun)){throw 'Configured VMX or vmrun is missing.'}
if(-not(Test-Path -LiteralPath $CredentialPath)){throw 'VM credential record missing. Run scripts/set-office-vm-credential.ps1 once in desktop-user PowerShell; never recover secrets automatically from history.'}
Host-Phase 'read-protected-credential'
$credential=Import-Clixml -LiteralPath $CredentialPath
Host-Phase 'credential-loaded'
if($credential -isnot [Management.Automation.PSCredential]){throw 'Invalid VM credential record.'}
function Vm([string]$operation,[string[]]$parameters){
 $output=& $Vmrun -T ws -gu $credential.UserName -gp $credential.GetNetworkCredential().Password $operation $VMX @parameters 2>&1|Out-String
 if($operation -eq 'fileExistsInGuest' -and $output.Trim() -eq 'The file does not exist.'){return $output.Trim()}
 if($LASTEXITCODE -ne 0){$safe=$output;$secret=$credential.GetNetworkCredential().Password;if($secret){$safe=$safe.Replace($secret,'[redacted]')};$secret=$null;$safe=$safe.Replace($credential.UserName,'[user]');throw "VM operation failed: $operation. $safe No force reset was attempted."}
 return $output.Trim()
}
function Download([string]$guest,[string]$local){
 $local=[IO.Path]::GetFullPath($local)
 if(-not $local.StartsWith($directory+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Download target must stay in this job directory.'}
 $temporary=$local+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
 try{[void](Vm 'copyFileFromGuestToHost' @($guest,$temporary));Move-Item -LiteralPath $temporary -Destination $local -Force}finally{if(Test-Path $temporary){Remove-Item -LiteralPath $temporary -Force}}
}
function Fetch([string]$relative,[string]$local){Download ($guestDirectory+'\'+$relative) $local}
function Push([string]$local,[string]$relative){
 if($relative -match '[\\/:]|\.\.'){throw 'Only a job input filename is allowed.'}
 $target=$guestDirectory+'\'+$relative
 if((Vm 'fileExistsInGuest' @($target)) -eq 'The file exists.'){[void](Vm 'deleteFileInGuest' @($target))}
 [void](Vm 'copyFileFromHostToGuest' @([IO.Path]::GetFullPath($local),$target))
}
function Sync-Executor {
 $source=[IO.Path]::GetFullPath((Join-Path $root 'tests/vm/office-runtime-job.ps1'));$hash=(Get-FileHash $source).Hash
 $target='C:\OOT-QA\office-runtime-executor.ps1';$record=Join-Path ([IO.Path]::GetDirectoryName($CredentialPath)) 'office-x86-executor.json'
 $replace=$true
 if((Vm 'fileExistsInGuest' @($target)) -eq 'The file exists.'){
  Download $target (Join-Path $directory 'executor-current.ps1');$existing=(Get-FileHash (Join-Path $directory 'executor-current.ps1')).Hash
  if($existing -eq $hash){$replace=$false}else{
   if(-not(Test-Path $record)){throw 'Unknown existing executor preserved; inspect its ownership before replacement.'}
   $previous=Get-Content $record -Raw|ConvertFrom-Json
   if($previous.vmx -ne [IO.Path]::GetFullPath($VMX) -or $previous.sha256 -ne $existing){throw 'Executor ownership/hash changed; replacement stopped.'}
   if((Vm 'listProcessesInGuest' @()) -match '(?im)cmd=.*office-runtime-executor\.ps1'){throw 'Guest executor is still running.'}
   [void](Vm 'deleteFileInGuest' @($target))
  }
 }
 if($replace){[void](Vm 'copyFileFromHostToGuest' @($source,$target))}
 @{vmx=[IO.Path]::GetFullPath($VMX);guestPath=$target;sha256=$hash}|ConvertTo-Json|Set-Content -LiteralPath $record -Encoding UTF8
}
function Start-Worker([string]$mode){
 [void](Vm 'runProgramInGuest' @('-noWait','-interactive',"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",'-NoProfile','-ExecutionPolicy','Bypass','-File','C:\OOT-QA\office-runtime-executor.ps1','-Action',$mode,'-JobDirectory',$guestDirectory))
}
function Wait-Report([string]$filename,[int]$seconds){
 $until=[DateTime]::UtcNow.AddSeconds($seconds);$misses=0
 do{
  try{
   if((Vm 'fileExistsInGuest' @($guestDirectory+'\'+$filename)) -ne 'The file exists.'){Start-Sleep -Seconds 2;continue}
   Fetch $filename (Join-Path $directory $filename);$data=Get-Content (Join-Path $directory $filename) -Raw|ConvertFrom-Json;$misses=0
   if($data.jobId -ne $JobId){throw 'Guest report belongs to another job.'}
   if($data.status -ne 'running'){return $data}
  }catch{$misses++;if($misses -ge 2){throw "Guest report unavailable twice: $filename. Inspect desktop/update state; do not send blind input."}}
  Start-Sleep -Seconds 5
 }while([DateTime]::UtcNow -lt $until)
 throw "Guest job timed out: $JobId. Collect evidence and inspect owned processes before cleanup."
}
function Collect {
 Fetch 'result.json' (Join-Path $directory 'result.json')
 $until=[DateTime]::UtcNow.AddSeconds(30)
 while((Vm 'fileExistsInGuest' @($guestDirectory+'\evidence-index.json')) -ne 'The file exists.'){
  if([DateTime]::UtcNow -ge $until){throw 'Final evidence index is not ready.'};Start-Sleep -Seconds 2
 }
 Fetch 'evidence-index.json' (Join-Path $directory 'evidence-index.json')
 $index=Get-Content (Join-Path $directory 'evidence-index.json') -Raw|ConvertFrom-Json
 if($index.jobId -ne $JobId){throw 'Evidence index jobId mismatch.'}
 foreach($file in $index.files){
  if($file.path -match '(^[\\/]|\.\.|:)' -or $file.length -le 0){throw 'Unsafe evidence path.'}
  $local=Join-Path $directory ('guest/'+$file.path);New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($local))|Out-Null
  Fetch $file.path $local
  if((Get-Item $local).Length -ne $file.length -or (Get-FileHash $local).Hash -ne $file.sha256){throw 'Collected evidence hash mismatch.'}
 }
 return (Get-Content (Join-Path $directory 'result.json') -Raw|ConvertFrom-Json)
}
function Cleanup {
 $guestRecord='C:\OOT-QA\cleanup-'+$JobId+'.json'
 if((Vm 'fileExistsInGuest' @($guestRecord)) -eq 'The file exists.'){
  Download $guestRecord (Join-Path $directory 'cleanup.json')
  $existing=Get-Content (Join-Path $directory 'cleanup.json') -Raw|ConvertFrom-Json
  if($existing.jobId -eq $JobId -and $existing.status -eq 'passed'){return}
 }
 Start-Worker 'Cleanup'
 for($attempt=0;$attempt -lt 2;$attempt++){
  Start-Sleep -Seconds 3
  try{Download $guestRecord (Join-Path $directory 'cleanup.json');break}catch{if($attempt -eq 1){throw}}
 }
 $cleanup=Get-Content (Join-Path $directory 'cleanup.json') -Raw|ConvertFrom-Json
 if($cleanup.jobId -ne $JobId -or $cleanup.status -ne 'passed'){throw 'Guest cleanup did not pass.'}
}
try{
 Host-Phase 'dispatch'
 if($Action -eq 'Collect'){Collect|ConvertTo-Json -Depth 12;return}
 if($Action -eq 'Cleanup'){
  Cleanup;Get-Content (Join-Path $directory 'cleanup.json');return
 }
 if($Action -eq 'Shutdown'){
  $processes=Vm 'listProcessesInGuest' @()
  if($processes -match '(?im)cmd=.*(WINWORD|EXCEL|POWERPNT|openOfficetab\.Center)\.exe'){throw 'Guest applications are still running; shutdown stopped.'}
  [void](Vm 'stop' @('soft'));Write-Output 'Normal guest shutdown requested.';return
 }
 Host-Phase 'list-virtual-machines'
 $running=& $Vmrun -T ws list 2>&1|Out-String
 if($running -notmatch [regex]::Escape($VMX)){[void](Vm 'start' @('gui'))}
 Host-Phase 'check-tools'
 $tools=Vm 'checkToolsState' @()
 if($tools -ne 'running'){
  & $Vmrun -T ws captureScreen $VMX (Join-Path $directory 'boot-state.png')|Out-Null
  @{jobId=$JobId;status='blocked';reason='guest_boot_or_windows_update';tools=$tools}|ConvertTo-Json|Set-Content (Join-Path $directory 'preflight.json')
  throw 'Guest is booting or applying Windows updates. Let it finish, then run Preflight again; never reset it.'
 }
 # Shared executor is fixed; every actual run holds a guest mutex and has a
 # unique directory. These directory creations are idempotent checks.
 Host-Phase 'guest-directories'
 foreach($path in 'C:\OOT-QA','C:\OOT-QA\runs',$guestDirectory){try{[void](Vm 'createDirectoryInGuest' @($path))}catch{[void](Vm 'listDirectoryInGuest' @($path))}}
 Host-Phase 'sync-executor'
 Sync-Executor
 if($Action -eq 'RecoverFixtures'){Start-Worker 'RecoverFixtures';$recovery=Wait-Report 'recovery.json' 120;if($recovery.status -ne 'passed'){throw 'Owned fixture recovery failed; documents preserved.'};$recovery|ConvertTo-Json -Depth 8;return}
 $request=@{schema=1;jobId=$JobId;version=$version;createdUtc=[DateTime]::UtcNow.ToString('o')}
 if($Action -eq 'Run'){
  if(Test-Path (Join-Path $directory 'result.json')){throw 'This jobId already has runtime evidence. Use Collect or a fresh jobId.'}
  if((Vm 'fileExistsInGuest' @($guestDirectory+'\result.json')) -eq 'The file exists.'){throw 'Guest runtime evidence already exists; use Collect or a fresh jobId.'}
  if(-not $DeliveryDirectory){$DeliveryDirectory=Join-Path $root ('delivery/beta-'+$version)}
  $installer=Join-Path $DeliveryDirectory "openOfficetab-$version-setup-x64.exe";$archive=Join-Path $DeliveryDirectory "openOfficetab-$version-win-x64.zip"
  foreach($path in $installer,$archive){if(-not(Test-Path $path)){throw 'Build/package both Office payloads before VM Run.'}}
  $request.installerSha256=(Get-FileHash $installer).Hash;$request.packageSha256=(Get-FileHash $archive).Hash
  $request.x86Sha256=(Get-FileHash (Join-Path $root 'dist/x86/openOfficetab.dll')).Hash
  $request.x64Sha256=(Get-FileHash (Join-Path $root 'dist/x64/openOfficetab.dll')).Hash
  $qa=Join-Path $directory 'qa';New-Item -ItemType Directory -Force "$qa/tests/fixtures","$qa/build"|Out-Null
  foreach($name in 'common.ps1','lifecycle-runtime.ps1','lifecycle-process-runtime.ps1','save-runtime.ps1','save-suite-runtime.ps1','language-runtime.ps1','desktop-install-runtime.ps1') {Copy-Item (Join-Path $root "tests/$name") "$qa/tests"}
  foreach($name in 'word-demo.docx','excel-demo.xlsx','powerpoint-demo.pptx'){Copy-Item (Join-Path $root "tests/fixtures/$name") "$qa/tests/fixtures"}
  Compress-Archive -Path "$qa/*" -DestinationPath (Join-Path $directory 'qa.zip') -Force
  $request.qaSha256=(Get-FileHash (Join-Path $directory 'qa.zip')).Hash
  Push $installer 'setup.exe';Push $archive 'package.zip';Push (Join-Path $directory 'qa.zip') 'qa.zip'
 }
 $request|ConvertTo-Json -Depth 6|Set-Content (Join-Path $directory 'request.json') -Encoding UTF8;Push (Join-Path $directory 'request.json') 'request.json'
 Start-Worker 'Preflight';$ready=Wait-Report 'preflight.json' 30
 if(-not $ready.unlocked -or $ready.session -eq 0 -or $ready.officeCount -ne 0 -or $ready.centerCount -ne 0){throw 'Guest desktop locked, noninteractive, or busy. Log in manually and preserve unknown windows.'}
 if($Action -eq 'Preflight'){$ready|ConvertTo-Json -Depth 8;return}
 Start-Worker 'Run';$result=Wait-Report 'result.json' ($TimeoutMinutes*60)
 $result=Collect
 if($result.status -ne 'passed'){throw "VM run $JobId did not pass; evidence was retained at $directory"}
 Cleanup
 if(-not $KeepVMRunning){[void](Vm 'stop' @('soft'))}
 $result|ConvertTo-Json -Depth 12
}finally{$credential=$null}
