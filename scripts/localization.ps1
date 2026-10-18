# Shared presentation-only language selection. Does not change Office or user settings.
$script:OotEnglish=$Language -eq 'en' -or ($Language -eq 'auto' -and [Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -ne 'zh')
function Get-OotText {
    param([string]$Chinese,[string]$English)
    if($script:OotEnglish){return $English}
    return $Chinese
}
