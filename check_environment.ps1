<#
.SYNOPSIS
    STM32 Environment & Toolchain Diagnostic Script
.DESCRIPTION
    Checks for the presence of STM32 development tools, compilers, flashing utilities,
    and downloaded installers on Windows.
#>

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "    STM32 Development Environment Diagnostics    " -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

# 1. Check for STM32CubeIDE installations
$stPaths = @(
    "C:\ST\STM32CubeIDE*",
    "${env:ProgramFiles}\STMicroelectronics\*",
    "${env:ProgramFiles(x86)}\STMicroelectronics\*"
)

$foundIDE = $false
foreach ($p in $stPaths) {
    $matches = Get-ChildItem -Path $p -ErrorAction SilentlyContinue
    if ($matches) {
        foreach ($m in $matches) {
            Write-Host "[FOUND] IDE Installation: $($m.FullName)" -ForegroundColor Green
            $foundIDE = $true
        }
    }
}
if (-not $foundIDE) {
    Write-Host "[MISSING] STM32CubeIDE is not installed in standard directories (C:\ST\)." -ForegroundColor Yellow
}

# 2. Check for Downloaded Installers
$installerZip = "C:\Users\User\Downloads\stm32cubeide_2.2.0-Win-x86_64.exe.zip"
if (Test-Path $installerZip) {
    $item = Get-Item $installerZip
    $sizeMB = [math]::Round($item.Length / 1MB, 2)
    Write-Host "[READY] Installer Zip available: $installerZip ($sizeMB MB)" -ForegroundColor Green
} else {
    Write-Host "[INFO] No STM32CubeIDE installer zip found in Downloads." -ForegroundColor Gray
}

# 3. Check CLI Tools in PATH
$tools = @(
    @{ Name = "arm-none-eabi-gcc"; Desc = "ARM GCC Compiler" },
    @{ Name = "STM32_Programmer_CLI"; Desc = "STM32 Cube Programmer CLI" },
    @{ Name = "openocd"; Desc = "OpenOCD Debugger" },
    @{ Name = "cmake"; Desc = "CMake Build System" },
    @{ Name = "ninja"; Desc = "Ninja Build Tool" },
    @{ Name = "make"; Desc = "GNU Make" }
)

Write-Host "`nChecking Command-Line Tools in PATH:" -ForegroundColor Cyan
foreach ($t in $tools) {
    $cmd = Get-Command $t.Name -ErrorAction SilentlyContinue
    if ($cmd) {
        Write-Host "  [OK]   $($t.Name) ($($t.Desc)) -> $($cmd.Source)" -ForegroundColor Green
    } else {
        Write-Host "  [--]   $($t.Name) ($($t.Desc)) not found in PATH" -ForegroundColor DarkGray
    }
}

Write-Host "`nDiagnostics Complete.`n" -ForegroundColor Cyan
