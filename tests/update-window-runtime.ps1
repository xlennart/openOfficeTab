param([ValidateSet('zh-CN','en')][string]$Language='zh-CN')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$center=(Get-ItemProperty HKCU:\Software\openOfficetab).CenterPath
$root=Split-Path $PSScriptRoot -Parent
$expected=Join-Path $root 'app/openOfficetab.Center.exe'
if(-not(Test-Path -LiteralPath $expected)){$expected=Join-Path $root 'dist/desktop/openOfficetab.Center.exe'}
$centerHash=(Get-FileHash -LiteralPath $center).Hash
if($centerHash -ne (Get-FileHash -LiteralPath $expected).Hash){throw 'Registered management center does not match the candidate payload.'}
if(Get-Process openOfficetab.Center -ErrorAction SilentlyContinue){throw 'Close the existing management center before this isolated UI test.'}
$app=Start-Process -FilePath $center -ArgumentList @('--page','updates','--language',$Language) -WindowStyle Normal -PassThru
$condition=New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::ProcessIdProperty,$app.Id)
function Windows {
 $windowType=New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Window)
 foreach($top in [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,$condition)){
  if($top.Current.BoundingRectangle.Width -gt 300){$top}
  foreach($owned in $top.FindAll([Windows.Automation.TreeScope]::Descendants,$windowType)){
   if($owned.Current.ProcessId -eq $app.Id -and $owned.Current.BoundingRectangle.Width -gt 300){$owned}
  }
 }
}
function Wait-Ui([scriptblock]$query,[string]$stage='update control',[int]$timeoutSeconds=30){
 $deadline=[DateTime]::UtcNow.AddSeconds($timeoutSeconds)
 do{$value=& $query;if($value){return $value};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $deadline)
 throw "Expected update UI did not appear: $stage."
}
function Button($window,[string]$name){
 $match=New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::NameProperty,$name)
 $window.FindAll([Windows.Automation.TreeScope]::Descendants,$match)|Where-Object {$_.Current.ControlType -eq [Windows.Automation.ControlType]::Button -and $_.Current.IsEnabled}|Select-Object -First 1
}
function Invoke-Button($button){$button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()}
$main=Wait-Ui {Windows|Where-Object {$_.Current.Name -match 'Management center|管理中心'}|Select-Object -First 1} 'main window'
$check=if($Language -eq 'en'){'Check for updates'}else{'检查更新'}
$ok=if($Language -eq 'en'){'OK'}else{'确定'}
$close=if($Language -eq 'en'){'Close update window'}else{'关闭更新窗口'}
$rows=@()
foreach($action in 'confirm','close'){
 Invoke-Button (Wait-Ui {Button $main $check} 'check button')
 $update=Wait-Ui {Windows|Where-Object {$_.Current.Name -match 'Software update|软件更新'}|Select-Object -First 1} 'update window'
 if($action -eq 'confirm'){Invoke-Button (Wait-Ui {Button $update $ok} 'OK button' 120)}else{Invoke-Button (Wait-Ui {Button $update $close} 'close button')}
 [void](Wait-Ui {if(-not(Windows|Where-Object {$_.Current.Name -match 'Software update|软件更新'})){$true}})
 if(-not(Windows|Where-Object {$_.Current.Name -match 'Management center|管理中心'})){throw 'Closing update also closed its owner.'}
 $rows+=@{language=$Language;action=$action;updateClosed=$true;ownerPreserved=$true}
}
$main.GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close()
if(-not $app.WaitForExit(30000)){throw 'Management center did not exit after its window closed.'}
@{centerSha256=$centerHash;centerPath=$center;checks=$rows}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $root "artifacts/update-window-runtime-$Language.json") -Encoding UTF8
