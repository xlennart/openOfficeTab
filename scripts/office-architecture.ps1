# Shared CLI/service architecture policy. Reads Office evidence, never registers it.
function Initialize-OotArchitecture {
    $root=Split-Path $PSScriptRoot -Parent
    $assembly=Join-Path $root 'service\openOfficetab.Service.dll'
    if(-not(Test-Path -LiteralPath $assembly)){$assembly=Join-Path $root 'build\desktop-service\openOfficetab.Service.dll'}
    Add-Type -Path $assembly
}
function Resolve-OotInstallArchitecture([string]$Requested='auto') {
    Initialize-OotArchitecture
    try{return [OpenOfficeTab.Desktop.Api]::SelectArchitecture($Requested,[OpenOfficeTab.Desktop.Api]::OfficeArchitecture(),[Environment]::Is64BitOperatingSystem)}
    catch {
        $failure=$_.Exception.GetBaseException()
        $locale=if($Language -eq 'auto'){([OpenOfficeTab.Desktop.Api]::Language((Join-Path $env:LOCALAPPDATA 'openOfficetab'))|ConvertFrom-Json).locale}else{$Language}
        throw [OpenOfficeTab.Desktop.Api]::LocalizedError($failure.Message,$locale,$failure.HResult)
    }
}
