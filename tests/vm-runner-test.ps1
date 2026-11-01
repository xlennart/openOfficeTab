$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$testRoot=Join-Path $root ('artifacts/vm-runner-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $testRoot,(Join-Path $testRoot 'guest')|Out-Null
$vmx=Join-Path $testRoot 'fixture.vmx';[IO.File]::WriteAllText($vmx,'offline fixture')
$stub=Join-Path $testRoot 'vmrun-stub.exe'
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe ("/out:"+$stub) /reference:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'vm/vmrun-stub.cs')
if($LASTEXITCODE -ne 0){throw 'VM protocol double compilation failed.'}
$secret=[Security.SecureString]::new();foreach($character in 'unused-test-value'.ToCharArray()){$secret.AppendChar($character)}
$credentialPath=Join-Path $testRoot 'credential.xml'
[pscredential]::new('offline-test',$secret)|Export-Clixml $credentialPath;$secret.Dispose()
$oldRoot=$env:OOT_VM_TEST_ROOT;$oldVMX=$env:OOT_VM_TEST_VMX
$env:OOT_VM_TEST_ROOT=$testRoot;$env:OOT_VM_TEST_VMX=$vmx
$jobId='vm-runner-offline-'+[guid]::NewGuid().ToString('N')
$arguments=@{JobId=$jobId;VMX=$vmx;Vmrun=$stub;CredentialPath=$credentialPath}
try{
 $runner=Join-Path $root 'scripts/test-office-x86-vm.ps1'
 & $runner -Action Preflight @arguments|Out-Null
 & $runner -Action Preflight @arguments|Out-Null
 # Reusing a completed cleanup receipt must not launch the now-deleted job.
 & $runner -Action Cleanup @arguments|Out-Null
 & $runner -Action Cleanup @arguments|Out-Null
 & $runner -Action Shutdown @arguments|Out-Null
 if(-not(Test-Path (Join-Path $testRoot 'normal-shutdown.txt'))){throw 'Normal shutdown was not requested.'}
 # An unexpected executor replacement must be preserved, rather than erased.
 $executor=Join-Path $testRoot 'guest/C_/OOT-QA/office-runtime-executor.ps1';[IO.File]::WriteAllText($executor,'third-party fixture')
 $rejected=$false
 try{& $runner -Action Preflight @arguments|Out-Null}catch{if($_.Exception.Message -notmatch 'ownership/hash changed'){throw};$rejected=$true}
 if(-not $rejected -or [IO.File]::ReadAllText($executor) -ne 'third-party fixture'){throw 'Executor ownership guard failed.'}
 Write-Output 'VM runner protocol passed: negative existence, no-overwrite transfers, repeated preflight, idempotent cleanup, normal shutdown and executor ownership.'
}finally{$env:OOT_VM_TEST_ROOT=$oldRoot;$env:OOT_VM_TEST_VMX=$oldVMX}
