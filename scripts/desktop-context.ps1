# Packaged development tools can give child processes private HKCU/AppData
# views. Use the desktop user's WMI process provider for install/uninstall;
# verify completion in the shared workspace, rather than the private registry.
function Invoke-DesktopContextScript {
    param([string]$ScriptPath,[hashtable]$Arguments)
    if(-not $env:CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY){return $false}
    $taskRoot=Split-Path $PSScriptRoot -Parent
    $jobDirectory=Join-Path $taskRoot ('artifacts\desktop-context-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $jobDirectory -Force | Out-Null
    $requestPath=Join-Path $jobDirectory 'request.xml'
    $resultPath=Join-Path $jobDirectory 'result.xml'
    @{Script=$ScriptPath;Arguments=$Arguments} | Export-Clixml -LiteralPath $requestPath
    $quotedRequest="'"+$requestPath.Replace("'","''")+"'"
    $quotedResult="'"+$resultPath.Replace("'","''")+"'"
    $expectedSid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $command=@"
`$ErrorActionPreference='Stop'
try {
    if (`$env:CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY) { throw 'Desktop process still has the development environment identity.' }
    if ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne '$expectedSid') { throw 'Desktop installer user identity differs from caller.' }
    `$request=Import-Clixml -LiteralPath $quotedRequest
    `$scriptArguments=`$request.Arguments
    `$output=& `$request.Script @scriptArguments 6>&1 | Out-String
    `$result=@{Success=`$true;Output=`$output}
} catch {
    `$result=@{Success=`$false;Output=(`$_ | Out-String)}
}
`$resultFile=$quotedResult
`$result | Export-Clixml -LiteralPath (`$resultFile+'.tmp')
Move-Item -LiteralPath (`$resultFile+'.tmp') -Destination `$resultFile
"@
    $workerPath=Join-Path $jobDirectory 'worker.ps1'
    [IO.File]::WriteAllText($workerPath,$command,(New-Object Text.UTF8Encoding($true)))
    $quotedWorker="'"+$workerPath.Replace("'","''")+"'"
    $powerShell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $startup=New-CimInstance -ClassName Win32_ProcessStartup -ClientOnly -Property @{ShowWindow=[uint16]0}
    $created=Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine=('"'+$powerShell+'" -NoProfile -Command "& '+$quotedWorker+'"')
        ProcessStartupInformation=$startup
    }
    if($created.ReturnValue -ne 0){throw "Desktop installer launch failed: $($created.ReturnValue)"}
    $deadline=[DateTime]::UtcNow.AddSeconds(60)
    while(-not (Test-Path -LiteralPath $resultPath)){
        if([DateTime]::UtcNow -ge $deadline){throw "Desktop installer did not report completion. Check $jobDirectory"}
        Start-Sleep -Milliseconds 100
    }
    $result=Import-Clixml -LiteralPath $resultPath
    if(-not $result.Success){throw $result.Output}
    Write-Host $result.Output
    $resolvedJob=(Resolve-Path -LiteralPath $jobDirectory).Path
    $expectedParent=[IO.Path]::GetFullPath((Join-Path $taskRoot 'artifacts'))
    if([IO.Path]::GetDirectoryName($resolvedJob) -ne $expectedParent -or -not ([IO.Path]::GetFileName($resolvedJob).StartsWith('desktop-context-'))){throw 'Installer work directory identity check failed.'}
    Remove-Item -LiteralPath $resolvedJob -Recurse -Force
    return $true
}
