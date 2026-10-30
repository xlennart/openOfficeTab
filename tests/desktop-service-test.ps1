param()
$ErrorActionPreference='Stop'
$ootRoot=Split-Path $PSScriptRoot -Parent
$ootOutput=Join-Path $ootRoot ('artifacts\desktop-service-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $ootOutput | Out-Null
$ootCompiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$ootSources=@(Get-ChildItem (Join-Path $ootRoot 'desktop\service') -Filter '*.cs'|ForEach-Object FullName)+@((Join-Path $PSScriptRoot 'desktop-service-test.cs'))
$ootTest=Join-Path $ootOutput 'service-test.exe'
& $ootCompiler /nologo /codepage:65001 /target:exe /optimize+ ("/out:$ootTest") ("/resource:"+(Join-Path $ootRoot 'desktop\service\update-public-key.xml')+',openOfficetab.UpdatePublicKey') ("/resource:"+(Join-Path $ootRoot 'desktop\service\update-sources.json')+',openOfficetab.UpdateSources') ("/resource:"+(Join-Path $ootRoot 'desktop/src/messages.json')+',openOfficetab.DesktopMessages') /reference:System.Web.Extensions.dll $ootSources
if($LASTEXITCODE -ne 0){throw 'Service test compilation failed.'}
& $ootTest (Join-Path $ootOutput 'native-shared-state')
if($LASTEXITCODE -ne 0){throw 'Service regression failed.'}
. (Join-Path $ootRoot 'scripts\desktop-service.ps1') -Library -TestRoot (Join-Path $ootOutput 'isolated-settings')
$count=0
function Assert($value,$label){if(-not $value){throw $label};$script:count++}
$request='{"changes":{"tabWidth":184,"themeMode":2,"autoHide":true}}'|ConvertFrom-Json
$state=Invoke-OotDesktopAction appearance $request
Assert ($state.appearance.tabWidth -eq 184 -and $state.appearance.themeMode -eq 2 -and $state.appearance.autoHide) 'Appearance writes shared INI values'
$preferences=Invoke-OotDesktopAction preferences ('{"autoCheck":false,"autoDownload":false,"autoInstall":false}'|ConvertFrom-Json)
Assert (-not $preferences.preferences.autoCheck) 'Update preference persistence'
$preferences=Invoke-OotDesktopAction preferences ('{"autoCheck":false,"autoDownload":false,"autoInstall":false,"updateChannel":"beta"}'|ConvertFrom-Json)
Assert ($preferences.preferences.updateChannel -eq 'beta') 'Beta channel preference persists'
$preferences=Invoke-OotDesktopAction preferences ('{"autoCheck":false,"autoDownload":false,"autoInstall":false}'|ConvertFrom-Json)
Assert ($preferences.preferences.updateChannel -eq 'beta') 'Old clients preserve the selected update channel'
$channelRejected=$false;try{Invoke-OotDesktopAction preferences ('{"autoCheck":false,"autoDownload":false,"autoInstall":false,"updateChannel":"alpha"}'|ConvertFrom-Json)|Out-Null}catch{$channelRejected=$true}
Assert $channelRejected 'Only stable and beta are user selectable'
$before=[OpenOfficeTab.Desktop.Api]::Hash((Join-Path $script:DataRoot 'settings.ini'))
foreach($json in @('{"changes":{"tabWidth":79}}','{"changes":{"themeMode":99}}','{"changes":{"futureKey":1}}','{"changes":{"autoHide":"false"}}')){
    $rejected=$false;try{[void](Invoke-OotDesktopAction appearance ($json|ConvertFrom-Json))}catch{$rejected=$true}
    Assert $rejected 'Reject invalid settings'
    Assert ([OpenOfficeTab.Desktop.Api]::Hash((Join-Path $script:DataRoot 'settings.ini')) -eq $before) 'Rejected input must not alter settings'
}
$rejected=$false;try{[void](Invoke-OotDesktopAction install ([pscustomobject]@{hosts=@('Word')}))}catch{$rejected=$true}
Assert $rejected 'Test root cannot mutate Office or registry'
$languageState=Invoke-OotDesktopAction language ('{"mode":2}'|ConvertFrom-Json)
Assert ($languageState.language.mode -eq 2 -and $languageState.language.locale -eq 'en') 'English persisted in shared native language preference'
$languageState=Invoke-OotDesktopAction language ('{"mode":1}'|ConvertFrom-Json)
Assert ($languageState.language.locale -eq 'zh-CN' -and $languageState.appearance.tabWidth -eq 184) 'Chinese switch preserves appearance'
$languageState=Invoke-OotDesktopAction language ('{"mode":0}'|ConvertFrom-Json)
Assert ($languageState.language.mode -eq 0) 'Follow Windows persisted'
Assert (-not $languageState.preferences.autoCheck) 'Language change preserves update preferences'
$before=[OpenOfficeTab.Desktop.Api]::Hash((Join-Path $script:DataRoot 'settings.ini'))
foreach($json in @('{"mode":-1}','{"mode":3}','{"mode":"2"}','{"mode":true}','{"mode":1.5}')){
    $rejected=$false;try{[void](Invoke-OotDesktopAction language ($json|ConvertFrom-Json))}catch{$rejected=$true}
    Assert $rejected 'Invalid language mode rejected'
    Assert ([OpenOfficeTab.Desktop.Api]::Hash((Join-Path $script:DataRoot 'settings.ini')) -eq $before) 'Rejected language does not alter settings'
}
Assert ([OpenOfficeTab.Desktop.Api]::ResolveLanguage(0,'zh-TW') -eq 'zh-CN') 'Chinese Windows display language'
Assert ([OpenOfficeTab.Desktop.Api]::ResolveLanguage(0,'fr-FR') -eq 'en') 'Other Windows display language'
Assert ([OpenOfficeTab.Desktop.Api]::LocalizedError('更新清单签名无效。','en',-1) -match '^The update manifest signature is invalid') 'Localized signed update failure'
Assert ([OpenOfficeTab.Desktop.Api]::LocalizedError('private-file-path','en',-1) -eq 'The operation could not be completed. Retry or export diagnostics and logs. (0xFFFFFFFF)') 'Unknown backend error is localized without exposing private paths'
[ordered]@{passed=$true;powerShellChecks=$count;registryModified=$false;officeStarted=$false;networkUsed=$false}|ConvertTo-Json|Set-Content (Join-Path $ootOutput 'result.json') -Encoding UTF8
Write-Output "PowerShell service checks passed: $count"
Write-Output $ootOutput

$ootDiagnostics=Invoke-OotDesktopAction diagnostics @{}
if($ootDiagnostics.runtimeTimeline.maxEvents -ne 4096 -or $ootDiagnostics.documentDataIncluded -ne $false){throw "Diagnostic export contract missing"}
Write-Output "PowerShell diagnostic timeline integration passed"
