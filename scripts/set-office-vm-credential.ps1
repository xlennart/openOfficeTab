param([string]$FromCredentialFile,[string]$Destination=(Join-Path $env:USERPROFILE 'AppData\Local\openOfficetab.Development\office-x86.credential.xml'))
$ErrorActionPreference='Stop'
# Run once in the real desktop user's PowerShell. No password belongs in caller argv,
# source, task manifests, transcripts, or the repository's build directory.
. (Join-Path $PSScriptRoot 'desktop-context.ps1')
if($FromCredentialFile -and (Invoke-DesktopContextScript -ScriptPath $PSCommandPath -Arguments @{FromCredentialFile=$FromCredentialFile;Destination=$Destination})){return}
if($env:CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY){throw 'Initialize credentials in the real desktop user context.'}
$credential=if($FromCredentialFile){Import-Clixml -LiteralPath $FromCredentialFile}else{Get-Credential -Message 'Office x86 test VM / 32-bit Office 测试虚拟机'}
if($credential -isnot [Management.Automation.PSCredential]){throw 'Expected a PSCredential record.'}
$target=[IO.Path]::GetFullPath($Destination);$directory=[IO.Path]::GetDirectoryName($target)
if($directory -ne (Join-Path $env:USERPROFILE 'AppData\Local\openOfficetab.Development')){throw 'Credential storage must stay in this desktop user development directory.'}
if((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Credential file cannot be a link.'}
$walk=$directory
while($walk){if(Test-Path -LiteralPath $walk){if((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Credential directory cannot contain links.'}};$walk=[IO.Path]::GetDirectoryName($walk)}
New-Item -ItemType Directory -Force -Path $directory|Out-Null
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User
$acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetOwner($sid);$acl.SetAccessRuleProtection($true,$false)
foreach($identity in @($sid,[Security.Principal.SecurityIdentifier]::new('S-1-5-18'))){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($identity,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
Set-Acl -LiteralPath $directory -AclObject $acl
try{$credential|Export-Clixml -LiteralPath $target;[void](Import-Clixml -LiteralPath $target)}finally{$credential=$null}
Write-Output ('Protected VM credential saved: '+$target)
