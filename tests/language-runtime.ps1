param([string]$ExpectedVersion='0.2.3-beta.1',[ValidateSet('auto','x86','x64')][string]$Architecture='auto')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $taskRoot 'scripts/office-architecture.ps1')
$Language='auto'
$Architecture=Resolve-OotInstallArchitecture $Architecture
. (Join-Path $PSScriptRoot 'common.ps1')
if(-not ('OotLanguageProbe' -as [type])){
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
[ComVisible(true),ClassInterface(ClassInterfaceType.AutoDispatch)]
public sealed class OotLanguageProbe { public string Id {get;set;} }
public static class OotLanguageNative {
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
 [DllImport("kernel32.dll")] public static extern ushort GetUserDefaultUILanguage();
}
'@
}
$apis=@();$apps=@();$before=@();$reports=@()
$expectedHash=(Get-FileHash -LiteralPath (Join-Path $taskRoot "dist\$Architecture\openOfficetab.dll")).Hash
foreach($hostName in 'Word','Excel','PowerPoint'){
    $fileName=if($hostName -eq 'Word'){'word-demo.docx'}elseif($hostName -eq 'Excel'){'excel-demo.xlsx'}else{'powerpoint-demo.pptx'}
    $file=Join-Path $taskRoot ('tests\fixtures\'+$fileName)
    if($hostName -eq 'PowerPoint'){$app=[OfficeRot]::Attach($hostName);$docs=$app.Presentations}else{$app=([OfficeRot]::AttachFile($file)).Application;if($hostName -eq 'Word'){$docs=$app.Documents}else{$docs=$app.Workbooks}}
    if($docs.Count -ne 1 -or $docs.Item(1).FullName -ne $file -or -not [bool]$docs.Item(1).Saved){throw "Unique saved public fixture required: $hostName"}
    $api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$state=$api.GetState()|ConvertFrom-Json
    if($state.version -ne $ExpectedVersion -or $state.views.Count -ne 1){throw 'Wrong loaded version or unexpected views'}
    [uint32]$officeProcess=0;[OotLanguageNative]::GetWindowThreadProcessId([IntPtr][long]$state.views[0].hwnd,[ref]$officeProcess)|Out-Null
    $loaded=@([OpenOfficeTab.Desktop.Api]::ModulePaths($officeProcess)|Where-Object {[IO.Path]::GetFileName($_) -eq 'openOfficetab.dll'})
    if($loaded.Count -ne 1 -or (Get-FileHash -LiteralPath $loaded[0]).Hash -ne $expectedHash){throw "Loaded payload mismatch: $hostName"}
    $apps+=,$app;$apis+=,$api;$before+=,$state
}
$original=[int]$before[0].diagnostics.languageSetting
function Assert-Language([string]$Expected,[int]$Mode){
    Start-Sleep -Milliseconds 1600
    for($i=0;$i -lt $apis.Count;$i++){
        $s=$apis[$i].GetState()|ConvertFrom-Json
        if($s.diagnostics.language -ne $Expected -or $s.diagnostics.languageSetting -ne $Mode){throw "Language did not synchronize: $($s.host)"}
        if($s.paneMetrics[0].controlHwnd -ne $before[$i].paneMetrics[0].controlHwnd -or $s.views[0].hwnd -ne $before[$i].views[0].hwnd -or $s.views[0].title -ne $before[$i].views[0].title){throw 'Language switch changed a window or document title'}
        foreach($pair in @(@('ootNew','New document','新建文档'),@('ootSave','Save all tabs in this group','保存本组全部标签'),@('ootSettings','Tab settings','标签设置'))){
            $probe=New-Object OotLanguageProbe;$probe.Id=$pair[0]
            $label=$apis[$i].GetLabel($probe)
            if($label -ne $pair[$(if($Expected -eq 'en'){1}else{2})]){throw "Actual Ribbon callback returned wrong language: $($s.host)"}
        }
    }
}
try {
    if(-not $apis[0].SetLanguage(2)){throw 'English selection failed'};Assert-Language 'en' 2
    if(-not $apis[1].SetLanguage(1)){throw 'Chinese selection failed'};Assert-Language 'zh-CN' 1
    if(-not $apis[2].SetLanguage(0)){throw 'Automatic selection failed'}
    $auto=if(([OotLanguageNative]::GetUserDefaultUILanguage() -band 0x3ff) -eq 4){'zh-CN'}else{'en'}
    Assert-Language $auto 0
    for($i=0;$i -lt $apis.Count;$i++){$s=$apis[$i].GetState()|ConvertFrom-Json;$reports+=[pscustomobject]@{Host=$s.host;Version=$s.version;LoadedSHA256=$expectedHash;EnglishChineseAndAuto=$true;LiveSync=$true;RibbonCallback=$true;PaneAndDocumentStable=$true}}
    $reports|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskRoot 'artifacts\language-runtime.json') -Encoding UTF8
    $reports|ConvertTo-Json -Depth 4
} finally {
    $apis[0].SetLanguage($original)|Out-Null
    Start-Sleep -Milliseconds 1000
}
