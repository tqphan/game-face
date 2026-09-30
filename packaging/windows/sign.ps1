# Authenticode-signs files with the release certificate. release.yml runs it on
# game-face.exe, and Inno Setup runs it (as SignTool "gfsign") on the installer
# and uninstaller.
#
#   packaging/windows/sign.ps1 <file>...
#
# Env: SIGNING_PFX (path to the password-protected .pfx), SIGNING_PFX_PASSWORD,
# TIMESTAMP_URL (optional). signtool.exe must be on PATH.
param(
    [Parameter(Mandatory, ValueFromRemainingArguments)]
    [string[]] $Files
)
$ErrorActionPreference = 'Stop'

foreach ($name in 'SIGNING_PFX', 'SIGNING_PFX_PASSWORD') {
    if (-not [Environment]::GetEnvironmentVariable($name)) { throw "$name is not set." }
}
$timestamp = if ($env:TIMESTAMP_URL) { $env:TIMESTAMP_URL } else { 'http://timestamp.digicert.com' }

signtool sign /f $env:SIGNING_PFX /p $env:SIGNING_PFX_PASSWORD /fd SHA256 /tr $timestamp /td SHA256 /d game-face @Files
if ($LASTEXITCODE) { throw "signtool sign failed ($LASTEXITCODE)." }
