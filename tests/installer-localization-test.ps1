param([string]$Installer)
$ErrorActionPreference='Stop'
if(-not $Installer){$Installer=Join-Path (Split-Path $PSScriptRoot -Parent) 'delivery\openOfficetab-0.1.9-setup-x64.exe'}
Add-Type -AssemblyName System.Windows.Forms,System.Drawing,System.IO.Compression
$assembly=[Reflection.Assembly]::LoadFile([IO.Path]::GetFullPath($Installer))
$windowType=$assembly.GetType('Setup+SetupWindow',$true)
$resource=$assembly.GetManifestResourceStream('openOfficetab.Payload.zip')
$archive=[IO.Compression.ZipArchive]::new($resource,[IO.Compression.ZipArchiveMode]::Read)
try {
    foreach($entry in @(@('README.md','(README.zh-CN.md)','Interface language'),@('README.zh-CN.md','(README.md)','语言设置'))){
        $reader=[IO.StreamReader]::new($archive.GetEntry($entry[0]).Open(),[Text.Encoding]::UTF8)
        try {$content=$reader.ReadToEnd()} finally {$reader.Dispose()}
        if(-not $content.Contains($entry[1]) -or -not $content.Contains($entry[2])){throw 'Packaged instructions have broken language links or encoding'}
    }
    if(-not $archive.GetEntry('scripts/localization.ps1')){throw 'Missing script language helper'}
} finally {$archive.Dispose();$resource.Dispose()}
$flags=[Reflection.BindingFlags]'Instance,NonPublic'
$originalCulture=[Threading.Thread]::CurrentThread.CurrentUICulture
try {
    foreach($culture in 'en-US','zh-CN','de-DE'){
        [Threading.Thread]::CurrentThread.CurrentUICulture=[Globalization.CultureInfo]::GetCultureInfo($culture)
        $window=[Activator]::CreateInstance($windowType,$true)
        try {
            $combo=$windowType.GetField('languageChoice',$flags).GetValue($window)
            $button=$windowType.GetField('install',$flags).GetValue($window)
            if($button.Text -ne $(if($culture -eq 'zh-CN'){'安装 / 更新'}else{'Install / Update'})){throw "Wrong automatic language: $culture"}
            foreach($mode in 1,2){
                $combo.SelectedIndex=$mode
                if($button.Text -ne $(if($mode -eq 1){'安装 / 更新'}else{'Install / Update'})){throw 'Manual language selection failed'}
                foreach($control in $window.Controls){
                    if($control -is [Windows.Forms.Button]){
                        $size=[Windows.Forms.TextRenderer]::MeasureText($control.Text,$control.Font)
                        if($size.Width+8 -gt $control.Width){throw "Button text clipped: $($control.Text)"}
                    }
                    if($control -is [Windows.Forms.Label] -and -not $control.AutoSize){
                        $size=[Windows.Forms.TextRenderer]::MeasureText($control.Text,$control.Font,[Drawing.Size]::new($control.Width,1000),[Windows.Forms.TextFormatFlags]::WordBreak)
                        if($size.Height -gt $control.Height){throw "Label text clipped: $($control.Text)"}
                    }
                }
            }
        } finally {$window.Dispose()}
    }
} finally {[Threading.Thread]::CurrentThread.CurrentUICulture=$originalCulture}
foreach($Language in 'en','zh-CN','auto'){
    . (Join-Path (Split-Path $PSScriptRoot -Parent) 'scripts\localization.ps1')
    $expected=$Language -eq 'en' -or ($Language -eq 'auto' -and [Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -ne 'zh')
    if((Get-OotText '中文' 'English') -ne $(if($expected){'English'}else{'中文'})){throw 'Script language selection failed'}
}
Write-Output 'Installer automatic/manual language, button and label fit, and script localization passed. No installer operation was run.'
