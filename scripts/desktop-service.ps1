param(
    [ValidateSet('state','language','appearance','hosts','install','repair','diagnostics','preferences','check-update','probe-routes','download-update','apply-update','background-update','uninstall','finish-uninstall')][string]$Action='state',
    [switch]$Library,[string]$TestRoot,[int]$WaitForProcess,[string]$ParentStartTicks,[switch]$RemoveSettings,[string]$ResultFile,
    [ValidateSet('auto','zh-CN','en')][string]$Language='auto'
)
$ErrorActionPreference='Stop'
[Console]::InputEncoding=[Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$OutputEncoding=[Console]::OutputEncoding
$script:PackageRoot=Split-Path $PSScriptRoot -Parent
$script:DataRoot=if($TestRoot){[IO.Path]::GetFullPath($TestRoot)}else{Join-Path $env:LOCALAPPDATA 'openOfficetab'}
$serviceAssembly=Join-Path $script:PackageRoot 'service\openOfficetab.Service.dll'
if(-not(Test-Path -LiteralPath $serviceAssembly)){$serviceAssembly=Join-Path $script:PackageRoot 'build\desktop-service\openOfficetab.Service.dll'}
Add-Type -Path $serviceAssembly
. (Join-Path $PSScriptRoot 'desktop-install.ps1')
function Get-OotPreferences {
    $data=[OpenOfficeTab.Desktop.Api]::Read($script:DataRoot) | ConvertFrom-Json
    return [ordered]@{autoCheck=$data.desktop.autoCheck -ne '0';autoDownload=$data.desktop.autoDownload -eq '1';autoInstall=$data.desktop.autoInstall -eq '1'}
}
function Get-OotState {
    $data=[OpenOfficeTab.Desktop.Api]::Read($script:DataRoot) | ConvertFrom-Json
    $native=$data.openOfficetab
    function Number($key,$fallback){$parsed=0;if([int]::TryParse([string]$native.$key,[ref]$parsed)){return $parsed};return $fallback}
    $hosts=if($TestRoot){@()}else{@(foreach($hostStatus in ([OpenOfficeTab.Desktop.Api]::InspectHosts() | ConvertFrom-Json)){$hostStatus})}
    $architecture=if($TestRoot){'unknown'}else{[OpenOfficeTab.Desktop.Api]::OfficeArchitecture()}
    $center=if($TestRoot){$null}else{(Get-ItemProperty 'HKCU:\Software\openOfficetab' -ErrorAction SilentlyContinue).CenterPath}
    return [ordered]@{
        version='0.2.2';installed=@($hosts|Where-Object {$_.registered -and $_.dllExists}).Count -gt 0
        centerInstalled=[bool]($center -and (Test-Path -LiteralPath $center -PathType Leaf))
        architecture=$architecture;compatible=[Environment]::Is64BitOperatingSystem -and $architecture -in @('x64','x86');officeRunning=if($TestRoot){$false}else{[OpenOfficeTab.Desktop.Api]::OfficeRunning()};hosts=$hosts
        appearance=[ordered]@{themeMode=(Number 'themeMode' 0);barStyle=(Number 'barStyle' 1);tabWidth=(Number 'tabWidth' 120);widthMode=(Number 'widthMode' 2);autoHide=$native.autoHide -eq '1';doubleClose=$native.doubleClose -eq '1'}
        preferences=(Get-OotPreferences);language=([OpenOfficeTab.Desktop.Api]::Language($script:DataRoot)|ConvertFrom-Json)
    }
}
function Assert-OotSystemMutation {
    if($TestRoot){throw 'System maintenance is disabled with a test data root.'}
}
function Assert-OotOfficeClosed {
    if([OpenOfficeTab.Desktop.Api]::OfficeRunning()){throw '请先保存并退出 Word、Excel 和 PowerPoint。操作尚未执行，不会关闭你的文档。'}
}
function Get-OotSelectedHosts($Request,[bool]$AllowEmpty=$true){
    $names=@($Request.hosts)
    if(((-not $AllowEmpty) -and $names.Count -eq 0) -or @($names|Where-Object {$_ -notin @('Word','Excel','PowerPoint')}).Count){throw 'Invalid Office host selection.'}
    return ,$names
}
function Invoke-OotDesktopAction([string]$Name,$Request){
    $script:OotLocale=([OpenOfficeTab.Desktop.Api]::Language($script:DataRoot)|ConvertFrom-Json).locale
    if($Language -ne 'auto'){$script:OotLocale=$Language}
    if($Request.language -in @('zh-CN','en')){$script:OotLocale=$Request.language}
    switch($Name){
        'state' {return Get-OotState}
        'language' {
            if(($Request.mode -isnot [int] -and $Request.mode -isnot [long]) -or $Request.mode -lt 0 -or $Request.mode -gt 2){throw 'Invalid interface language.'}
            [OpenOfficeTab.Desktop.Api]::Merge($script:DataRoot,'openOfficetab',(@{language=[string]$Request.mode}|ConvertTo-Json -Compress))
            return Get-OotState
        }
        'probe-routes' {if($TestRoot){throw 'Network probes are disabled with a test root.'};return ,@([OpenOfficeTab.Desktop.Api]::ProbeRoutes($script:DataRoot)|ConvertFrom-Json|ForEach-Object {foreach($route in $_){$route}})}
        'appearance' {
            $changes=@{}
            foreach($property in $Request.changes.PSObject.Properties){
                $key=$property.Name;$value=$property.Value
                if($key -in @('autoHide','doubleClose')){if($value -isnot [bool]){throw 'Invalid boolean preference.'};$changes[$key]=[string][int]$value;continue}
                $range=switch($key){'themeMode'{@(0,3)} 'barStyle'{@(0,1)} 'tabWidth'{@(80,360)} 'widthMode'{@(0,2)} default{throw 'Unknown appearance preference.'}}
                if(($value -isnot [int] -and $value -isnot [long]) -or $value -lt $range[0] -or $value -gt $range[1]){throw 'Invalid appearance preference.'}
                $changes[$key]=[string]$value
            }
            if($changes.Count){[OpenOfficeTab.Desktop.Api]::Merge($script:DataRoot,'openOfficetab',($changes|ConvertTo-Json -Compress))}
            return Get-OotState
        }
        'hosts' {
            Assert-OotSystemMutation;$selected=Get-OotSelectedHosts $Request
            foreach($name in @('Word','Excel','PowerPoint')){if(-not(Test-Path -LiteralPath "HKCU:\Software\Microsoft\Office\$name\Addins\openOfficetab.Connect")){throw '请先安装加载组件。'}}
            $mutex=Enter-OotMaintenance
            try{foreach($name in @('Word','Excel','PowerPoint')){Set-ItemProperty -LiteralPath "HKCU:\Software\Microsoft\Office\$name\Addins\openOfficetab.Connect" -Name LoadBehavior -Type DWord -Value $(if($name -in $selected){3}else{0})}}finally{$mutex.ReleaseMutex();$mutex.Dispose()}
            return Get-OotState
        }
        {$_ -in @('install','repair')} {
            Assert-OotSystemMutation;Assert-OotOfficeClosed
            $architecture=[OpenOfficeTab.Desktop.Api]::SelectArchitecture('auto',[OpenOfficeTab.Desktop.Api]::OfficeArchitecture(),[Environment]::Is64BitOperatingSystem)
            $oldState=Get-OotState
            $selected=if($Request.preserveHosts -eq $true -or $Name -eq 'repair'){@($oldState.hosts|Where-Object enabled|ForEach-Object name)}else{Get-OotSelectedHosts $Request $false}
            if(@($oldState.hosts|Where-Object registered).Count -eq 0 -and $selected.Count -eq 0){$selected=@('Word','Excel','PowerPoint')}
            $selection=if($selected.Count){$selected -join ','}else{'None'}
            & (Join-Path $PSScriptRoot 'install.ps1') -Architecture $architecture -HostSelection $selection -CenterSource $Request.exePath -DesktopShortcut:($Request.shortcut -eq $true) -Language $script:OotLocale *>&1 | Out-Null
            $result=Get-OotState
            if(-not $result.installed -or -not $result.centerInstalled){throw '安装结果未通过校验。'}
            return $result
        }
        'diagnostics' {
            $state=Get-OotState
            $match=[OpenOfficeTab.Desktop.Api]::MatchesPayload($script:PackageRoot,$state.architecture)
            return [ordered]@{version=$state.version;architecture=$state.architecture;installed=$state.installed;matchesThisPackage=$match;hosts=$state.hosts;runtimeTimeline=([OpenOfficeTab.Desktop.Api]::DiagnosticTimeline($script:DataRoot)|ConvertFrom-Json);documentDataIncluded=$false;privacy='Event codes and numeric identities only; no document names, paths, content, accounts, URLs or legacy raw logs'}
        }
        'preferences' {
            $preferences=@{};foreach($key in @('autoCheck','autoDownload','autoInstall')){if($Request.$key -isnot [bool]){throw 'Invalid update preference.'};$preferences[$key]=[string][int]$Request.$key}
            if($preferences.autoCheck -eq '0'){$preferences.autoDownload='0';$preferences.autoInstall='0'}
            if($preferences.autoInstall -eq '1'){$preferences.autoCheck='1';$preferences.autoDownload='1'}
            $old=Get-OotPreferences
            try{
                if(-not $TestRoot){Set-OotUpdateTask ($preferences.autoCheck -eq '1' -and $preferences.autoDownload -eq '1')}
                [OpenOfficeTab.Desktop.Api]::Merge($script:DataRoot,'desktop',($preferences|ConvertTo-Json -Compress))
            }catch{if(-not $TestRoot){Set-OotUpdateTask ($old.autoCheck -and $old.autoDownload)};throw}
            return Get-OotState
        }
        {$_ -in @('check-update','download-update','apply-update','background-update')} {
            if($TestRoot){throw 'Network and installers are disabled with a test data root.'}
            $mutex=[Threading.Mutex]::new($false,('Local\openOfficetab.Update.'+[OpenOfficeTab.Desktop.Api]::Identity($script:DataRoot)))
            $held=$false
            try{
                $lockWait=if($Name -eq 'check-update'){30000}else{0}
                try{$held=$mutex.WaitOne($lockWait)}catch [Threading.AbandonedMutexException]{$held=$true}
                if(-not $held){throw '另一项更新检查正在运行，请稍后重试。'}
                $preferences=Get-OotPreferences
                if($Name -eq 'background-update' -and (-not $preferences.autoCheck -or -not $preferences.autoDownload)){return @{skipped=$true}}
                $release=[OpenOfficeTab.Desktop.Api]::CheckUpdate($script:DataRoot,($Name -eq 'check-update')) | ConvertFrom-Json
                if($Name -eq 'check-update' -or -not $release.available){return $release}
                if($Name -ne 'background-update' -and $Request.version -ne $release.version){throw '更新版本已经变化，请重新检查。'}
                [void][OpenOfficeTab.Desktop.Api]::DownloadUpdate($script:DataRoot,$release.version)
                if($Name -eq 'download-update'){return @{available=$true;version=$release.version;downloaded=$true}}
                if([OpenOfficeTab.Desktop.Api]::OfficeRunning()){return @{available=$true;version=$release.version;downloaded=$true;waitingForOffice=$true}}
                if($Name -eq 'background-update' -and -not $preferences.autoInstall){return @{downloaded=$true;version=$release.version}}
                [OpenOfficeTab.Desktop.Api]::InstallUpdate($script:DataRoot,$release.version)
                return @{installed=$true;version=$release.version}
            }finally{if($held){$mutex.ReleaseMutex()};$mutex.Dispose()}
        }
        'uninstall' {
            Assert-OotSystemMutation;Assert-OotOfficeClosed
            $parent=[Diagnostics.Process]::GetProcessById([int]$Request.callerPid)
            $temp=Join-Path ([IO.Path]::GetTempPath()) ('openOfficetab-remove-'+[guid]::NewGuid().ToString('N'))
            New-Item -ItemType Directory -Force (Join-Path $temp 'scripts'),(Join-Path $temp 'service') | Out-Null
            foreach($file in 'desktop-service.ps1','desktop-install.ps1','uninstall.ps1','desktop-context.ps1','localization.ps1','office-architecture.ps1'){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $temp 'scripts')}
            Copy-Item -LiteralPath $serviceAssembly -Destination (Join-Path $temp 'service\openOfficetab.Service.dll')
            $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $temp 'scripts\desktop-service.ps1')+'"'),'-Action','finish-uninstall','-WaitForProcess',[string]$parent.Id,'-ParentStartTicks',[string]$parent.StartTime.ToUniversalTime().Ticks,'-Language',$script:OotLocale,'-ResultFile',('"'+(Join-Path ([IO.Path]::GetTempPath()) 'openOfficetab-uninstall-result.json')+'"'))
            if($Request.removeSettings -eq $true){$arguments+='-RemoveSettings'}
            Start-Process -FilePath (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -ArgumentList $arguments -WindowStyle Hidden | Out-Null
            $parent.Dispose();return @{exit=$true;message='管理中心关闭后完成卸载；结果记录在临时目录 openOfficetab-uninstall-result.json。'}
        }
        'finish-uninstall' {
            Assert-OotSystemMutation
            try{$parent=[Diagnostics.Process]::GetProcessById($WaitForProcess);if([string]$parent.StartTime.ToUniversalTime().Ticks -ne $ParentStartTicks){throw 'Uninstall parent identity changed.'};if(-not $parent.WaitForExit(30000)){throw '请先关闭管理中心。'}}catch [ArgumentException]{}finally{if($parent){$parent.Dispose()}}
            Assert-OotOfficeClosed
            & (Join-Path $PSScriptRoot 'uninstall.ps1') -RemoveSettings:$RemoveSettings -Language $script:OotLocale *>&1 | Out-Null
            return @{uninstalled=$true}
        }
        default {throw 'Unsupported action.'}
    }
}
if($Library){return}
try{
    $request=if($Action -eq 'finish-uninstall'){@{}}else{$text=[Console]::In.ReadToEnd();if($text){$text|ConvertFrom-Json}else{@{}}}
    $ootActionTimer=[Diagnostics.Stopwatch]::StartNew()
    if($Action -ne 'state'){[OpenOfficeTab.Desktop.Api]::RecordAction($script:DataRoot,$Action,'begin',0,0)}
    $result=Invoke-OotDesktopAction $Action $request
    if($Action -ne 'state'){[OpenOfficeTab.Desktop.Api]::RecordAction($script:DataRoot,$Action,'end',0,$ootActionTimer.ElapsedMilliseconds)}
    $response=@{ok=$true;data=$result}
    if($ResultFile){[IO.File]::WriteAllText($ResultFile,($response|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))}
    $response | ConvertTo-Json -Depth 12 -Compress
}catch{
    $ootFailure=$_
    try{[OpenOfficeTab.Desktop.Api]::RecordAction($script:DataRoot,$Action,'error',$ootFailure.Exception.GetBaseException().HResult,0)}catch{}
    $ootException=$ootFailure.Exception.GetBaseException()
    $ootLocale=if($script:OotLocale){$script:OotLocale}else{([OpenOfficeTab.Desktop.Api]::Language($script:DataRoot)|ConvertFrom-Json).locale}
    $response=@{ok=$false;error=[OpenOfficeTab.Desktop.Api]::LocalizedError($ootException.Message,$ootLocale,$ootException.HResult);errorCode=$ootException.HResult}
    if($ResultFile){[IO.File]::WriteAllText($ResultFile,($response|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))}
    $response | ConvertTo-Json -Compress
    exit 1
}finally{
    if($Action -eq 'finish-uninstall'){$temp=[IO.Path]::GetFullPath($script:PackageRoot);if([IO.Path]::GetDirectoryName($temp) -eq [IO.Path]::GetTempPath().TrimEnd('\') -and [IO.Path]::GetFileName($temp) -match '^openOfficetab-remove-[0-9a-f]{32}$'){Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue}}
}
