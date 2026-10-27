<#
.SYNOPSIS
    ST-Link USB Driver Setup Helper
.DESCRIPTION
    Extracts the bundled STSW-LINK009 driver package and launches the 64-bit installer (dpinst_amd64.exe).
#>

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DriverZip = Join-Path (Split-Path -Parent $ScriptDir) "drivers\stsw-link009.zip"
$ExtractPath = Join-Path $env:TEMP "stlink_driver_setup"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "      ST-Link USB Driver Setup            " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

if (-not (Test-Path $DriverZip)) {
    Write-Host "[ERROR] Driver zip archive not found at: $DriverZip" -ForegroundColor Red
    exit 1
}

Write-Host "[1/2] Extracting ST-LINK USB driver package to $ExtractPath..." -ForegroundColor Cyan
if (Test-Path $ExtractPath) {
    Remove-Item -Recurse -Force $ExtractPath
}
Expand-Archive -Path $DriverZip -DestinationPath $ExtractPath -Force

$installer = Join-Path $ExtractPath "dpinst_amd64.exe"
if (-not (Test-Path $installer)) {
    # Search for dpinst in subfolders if nested
    $found = Get-ChildItem -Path $ExtractPath -Filter "dpinst_amd64.exe" -Recurse | Select-Object -First 1
    if ($found) {
        $installer = $found.FullName
    }
}

if (Test-Path $installer) {
    Write-Host "[2/2] Launching installer: $installer..." -ForegroundColor Green
    Write-Host "Please accept any Windows UAC administrator prompts." -ForegroundColor Yellow
    Start-Process -FilePath $installer -Verb RunAs
} else {
    Write-Host "[ERROR] Could not find dpinst_amd64.exe in extracted archive." -ForegroundColor Red
    exit 1
}
