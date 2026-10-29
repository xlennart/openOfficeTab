$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$output=Join-Path $root 'build/native-payload-tests'
New-Item -ItemType Directory -Force $output|Out-Null
foreach($architecture in 'x86','x64'){
    $exe=Join-Path $output ($architecture+'.exe')
    & $compiler /nologo /target:exe ("/platform:$architecture") ("/out:$exe") (Join-Path $PSScriptRoot 'native-payload-test.cs')
    if($LASTEXITCODE -ne 0){throw 'Native payload verifier compilation failed.'}
    & $exe (Join-Path $root "dist/$architecture/openOfficetab.dll") $(if($architecture -eq 'x86'){4}else{8})
    if($LASTEXITCODE -ne 0){throw "$architecture payload verification failed."}
}
