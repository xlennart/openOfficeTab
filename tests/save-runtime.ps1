param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName='Word',[ValidateSet('x86','x64')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $root 'scripts/office-architecture.ps1')
$Language='auto'
[void](Resolve-OotInstallArchitecture $Architecture)
$processName=@{Word='WINWORD';Excel='EXCEL';PowerPoint='POWERPNT'}[$HostName]
if(Get-Process $processName -ErrorAction SilentlyContinue){throw 'Close this Office host before the isolated save test.'}
$name=@{Word='word-demo.docx';Excel='excel-demo.xlsx';PowerPoint='powerpoint-demo.pptx'}[$HostName]
$directory=Join-Path $root ('artifacts/save-fixture-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory|Out-Null
$copy=Join-Path $directory $name
Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('fixtures/'+$name)) -Destination $copy
$before=(Get-FileHash $copy).Hash
Start-Process -FilePath $copy|Out-Null
$deadline=[DateTime]::UtcNow.AddSeconds(60)
do{try{$app=[OfficeRot]::Attach($HostName);$api=$app.COMAddIns.Item('openOfficetab.Connect').Object;$state=$api.GetState()|ConvertFrom-Json;if($state.views.Count -eq 1){break}}catch{};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $deadline)
if(-not $state -or $state.views.Count -ne 1){throw 'Save fixture did not initialize.'}
$office=Get-Process $processName
if(@($office).Count -ne 1){throw 'Ambiguous Office process identity.'}
$started=$office.StartTime.ToUniversalTime().ToString('o')
if($HostName -eq 'Word'){$doc=$app.ActiveDocument;if($doc.FullName -ne $copy){throw 'Unexpected document'};$doc.Range().Text='OOT_SAVE_QA'}
elseif($HostName -eq 'Excel'){$doc=$app.ActiveWorkbook;if($doc.FullName -ne $copy){throw 'Unexpected workbook'};$doc.Worksheets.Item(1).Range('A1').Value2='OOT_SAVE_QA'}
else{$doc=$app.ActivePresentation;if($doc.FullName -ne $copy){throw 'Unexpected presentation'};$shape=$doc.Slides.Item(1).Shapes.AddTextbox(1,20,20,300,50);$shape.TextFrame.TextRange.Text='OOT_SAVE_QA'}
if($doc.Saved){throw 'The save fixture was not modified.'}
if(-not $api.Save($state.views[0].id)){throw 'Native save command failed.'}
if(-not $doc.Saved){throw 'Native save left the document unsaved.'}
$loaded=[OpenOfficeTab.Desktop.Api]::RegisteredDll($Architecture)
if(-not([OpenOfficeTab.Desktop.Api]::ModulePaths($office.Id) -contains $loaded)){throw 'Registered native payload is not loaded.'}
$api.Close($state.views[0].id)|Out-Null
@{Host=$HostName;Architecture=$Architecture;OfficePid=$office.Id;OfficeStartUtc=$started;NativeSaveAccepted=$true;BeforeSha256=$before;FixtureFile=$copy;LoadedSha256=(Get-FileHash $loaded).Hash;FixtureDirectory=$directory}|ConvertTo-Json|Set-Content (Join-Path $root "artifacts/save-runtime-$HostName.json") -Encoding UTF8
