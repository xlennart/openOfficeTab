# Shared by the per-user installer and uninstaller; never closes Office.
function Enter-OotMaintenance {
    $mutex=[Threading.Mutex]::new($false,('Local\openOfficetab.Install.'+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value))
    $locked=$false
    try{try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true};if(-not $locked){throw 'Another openOfficetab maintenance operation is running.'};return $mutex}catch{$mutex.Dispose();throw}
}
function Install-OotDesktop {
    param([string]$TaskRoot,[string]$SourceDll,[string]$Architecture,[string]$CenterSource,[switch]$DesktopShortcut)
    $centerSource=$CenterSource
    if(-not $centerSource){$centerSource=Join-Path $TaskRoot 'app\openOfficetab.Center.exe'}
    if(-not(Test-Path -LiteralPath $centerSource)){$centerSource=Join-Path $TaskRoot 'openOfficetab.Center.exe'}
    if(-not(Test-Path -LiteralPath $centerSource)){return}
    $files=@(@{Source=$centerSource;Relative='openOfficetab.Center.exe'},@{Source=$SourceDll;Relative="dist\$Architecture\openOfficetab.dll"},@{Source=(Join-Path $TaskRoot 'LICENSE');Relative='LICENSE'})
    foreach($name in 'install.ps1','uninstall.ps1','diagnose.ps1','desktop-context.ps1','desktop-install.ps1','desktop-service.ps1','localization.ps1','office-architecture.ps1'){$files+=@{Source=(Join-Path $PSScriptRoot $name);Relative=('scripts\'+$name)}}
    $files+=@{Source=(Join-Path $TaskRoot 'service\openOfficetab.Service.dll');Relative='service\openOfficetab.Service.dll'}
    foreach($other in @('x86','x64')|Where-Object {$_ -ne $Architecture}){
        $candidate=Join-Path $TaskRoot "dist\$other\openOfficetab.dll"
        if(-not(Test-Path -LiteralPath $candidate)){throw 'A required Office architecture payload is missing.'}
        $files+=@{Source=$candidate;Relative="dist\$other\openOfficetab.dll"}
    }
    $identity=($files | ForEach-Object {$_.Relative+':'+(Get-FileHash -LiteralPath $_.Source -Algorithm SHA256).Hash}) -join "`n"
    $sha=[Security.Cryptography.SHA256]::Create()
    try{$hash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($identity))).Replace('-','').Substring(0,12)).ToLowerInvariant()}finally{$sha.Dispose()}
    $directory=Join-Path $env:LOCALAPPDATA ('openOfficetab\app\'+$hash)
    Assert-OotNoReparse (Join-Path $env:LOCALAPPDATA 'openOfficetab')
    foreach($file in $files){
        $target=Join-Path $directory $file.Relative
        if(Test-Path -LiteralPath $target){if((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $file.Source).Hash){throw 'Existing desktop slot contains different bytes.'}}
        else{New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null;Copy-Item -LiteralPath $file.Source -Destination $target;Unblock-File -LiteralPath $target}
    }
    $center=Join-Path $directory 'openOfficetab.Center.exe'
    $key='HKCU:\Software\openOfficetab';New-Item -Path $key -Force | Out-Null
    Set-ItemProperty -LiteralPath $key -Name CenterPath -Value $center
    Set-ItemProperty -LiteralPath $key -Name DesktopVersion -Value '0.2.2'
    $shell=New-Object -ComObject WScript.Shell
    try{
        $menu=Join-Path ([Environment]::GetFolderPath('Programs')) 'openOfficetab'
        New-Item -ItemType Directory -Path $menu -Force | Out-Null
        $link=$shell.CreateShortcut((Join-Path $menu 'openOfficetab 管理中心.lnk'));$link.TargetPath=$center;$link.WorkingDirectory=$directory;$link.IconLocation=$center+',0';$link.Description='openOfficeTab · made by @xlennart';$link.Save()
        if($DesktopShortcut){$link=$shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) 'openOfficetab.lnk'));$link.TargetPath=$center;$link.WorkingDirectory=$directory;$link.IconLocation=$center+',0';$link.Save()}
    }finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)}
    $uninstall='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\openOfficetab';New-Item -Path $uninstall -Force | Out-Null
    foreach($pair in @{DisplayName='openOfficeTab';DisplayVersion='0.2.2';Publisher='xlennart';DisplayIcon=($center+',0');UninstallString=('"'+$center+'" --uninstall');URLInfoAbout='https://github.com/xlennart/openOfficetab';InstallLocation=$directory}.GetEnumerator()){Set-ItemProperty -LiteralPath $uninstall -Name $pair.Key -Value $pair.Value}
    Set-ItemProperty -LiteralPath $uninstall -Name NoModify -Value 1 -Type DWord
    Set-ItemProperty -LiteralPath $uninstall -Name NoRepair -Value 1 -Type DWord
    Add-Type -Path (Join-Path $directory 'service\openOfficetab.Service.dll')
    $data=[OpenOfficeTab.Desktop.Api]::Read((Join-Path $env:LOCALAPPDATA 'openOfficetab')) | ConvertFrom-Json
    Set-OotUpdateTask ($data.desktop.autoCheck -ne '0' -and $data.desktop.autoDownload -eq '1')
}
function Assert-OotNoReparse([string]$Directory){
    if(Test-Path -LiteralPath $Directory){
        $items=@(Get-Item -LiteralPath $Directory)+@(Get-ChildItem -LiteralPath $Directory -Directory -Recurse -Force)
        foreach($item in $items){if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Maintenance path contains a reparse point.'}}
    }
}
function Set-OotUpdateTask([bool]$Enabled){
    if(-not $Enabled){Remove-OotUpdateTask;return}
    $center=(Get-ItemProperty 'HKCU:\Software\openOfficetab' -ErrorAction SilentlyContinue).CenterPath
    $appRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab\app'
    if(-not $center -or -not(Test-Path -LiteralPath $center -PathType Leaf) -or [IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($center)) -ne $appRoot -or [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($center)) -notmatch '^[0-9a-f]{12}$' -or [IO.Path]::GetFileName($center) -ne 'openOfficetab.Center.exe'){throw 'Install the management center before enabling background updates.'}
    $scheduler=New-Object -ComObject Schedule.Service
    try{
        $scheduler.Connect();$folder=$scheduler.GetFolder('\');$task=$scheduler.NewTask(0)
        $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
        $task.RegistrationInfo.Description='openOfficetab opt-in updates; never closes Office documents.'
        $task.Principal.UserId=$sid;$task.Principal.LogonType=3;$task.Principal.RunLevel=0
        $task.Settings.StartWhenAvailable=$true;$task.Settings.WakeToRun=$false;$task.Settings.MultipleInstances=2;$task.Settings.ExecutionTimeLimit='PT10M'
        $trigger=$task.Triggers.Create(2);$trigger.StartBoundary=(Get-Date).AddMinutes(1).ToString("yyyy-MM-dd'T'HH:mm:ss");$trigger.DaysInterval=1;$trigger.Repetition.Interval='PT15M';$trigger.Repetition.Duration='P1D'
        $action=$task.Actions.Create(0);$action.Path=$center;$action.Arguments='--background-update'
        [void]$folder.RegisterTaskDefinition(('openOfficetab Update '+$sid),$task,6,$null,$null,3,$null)
    }finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($scheduler)}
}
function Assert-OotCenterClosed {
    $appRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab\app'
    foreach($process in @(Get-Process 'openOfficetab.Center' -ErrorAction SilentlyContinue)){if($process.Path -and $process.Path.StartsWith($appRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Close the management center and wait for its update check to finish before uninstalling.'}}
    # Closing the window can leave its bounded update request finishing in a
    # separate PowerShell process. Wait before unregistering or deleting files;
    # never terminate a worker or mistake an unrelated script for our service.
    $cacheRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab\cache'
    $deadline=[DateTime]::UtcNow.AddSeconds(30)
    do {
        $workers=@(Get-CimInstance Win32_Process -Filter "Name='powershell.exe' OR Name='pwsh.exe'" | Where-Object {
            if($_.ProcessId -eq $PID){return $false}
            if($_.CommandLine -notmatch '(?i)(?:^|\s)-File\s+(?:"(?<service>[^"]+)"|(?<service>\S+))'){return $false}
            $servicePath=[IO.Path]::GetFullPath($Matches.service).Replace('/','\')
            $servicePath.EndsWith('\scripts\desktop-service.ps1',[StringComparison]::OrdinalIgnoreCase) -and
                ($servicePath.StartsWith($appRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $servicePath.StartsWith($cacheRoot+'\',[StringComparison]::OrdinalIgnoreCase))
        })
        if(-not $workers.Count){return}
        if([DateTime]::UtcNow -ge $deadline){throw 'Close the management center and wait for its update check to finish before uninstalling.'}
        Start-Sleep -Milliseconds 200
    } while($true)
}
function Remove-OotUpdateTask {
    $service=New-Object -ComObject Schedule.Service
    try{
        $service.Connect();$folder=$service.GetFolder('\')
        try{$folder.DeleteTask(('openOfficetab Update '+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value),0)}catch{$errorObject=$_.Exception;while($errorObject.InnerException){$errorObject=$errorObject.InnerException};if($errorObject.HResult -ne -2147024894){throw}}
    }finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($service)}
}
function Remove-OotDesktop {
    $appRoot=Join-Path $env:LOCALAPPDATA 'openOfficetab\app'
    if(Test-Path -LiteralPath $appRoot){$resolved=(Resolve-Path -LiteralPath $appRoot).Path;$expected=[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'openOfficetab\app'));if($resolved -ne $expected){throw 'Desktop uninstall path identity check failed.'};Remove-Item -LiteralPath $resolved -Recurse -Force}
    foreach($shortcut in @((Join-Path ([Environment]::GetFolderPath('Programs')) 'openOfficetab\openOfficetab 管理中心.lnk'),(Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) 'openOfficetab.lnk'))){if(Test-Path -LiteralPath $shortcut){Remove-Item -LiteralPath $shortcut -Force}}
    foreach($key in 'HKCU:\Software\openOfficetab','HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\openOfficetab'){if(Test-Path -LiteralPath $key){Remove-Item -LiteralPath $key -Recurse -Force}}
}
