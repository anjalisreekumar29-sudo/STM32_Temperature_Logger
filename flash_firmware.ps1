<#
.SYNOPSIS
    Automatic One-Touch ST-Link Firmware Flasher for STM32
.DESCRIPTION
    Automatically locates or compiles the firmware in this folder and flashes it
    to the target STM32F103 (CBT6) board via ST-LINK V2 using OpenOCD.
    
    You do NOT need to specify any parameters. Simply run:
        .\flash_firmware.ps1
#>

param (
    [Parameter(Mandatory=$false)]
    [string]$FirmwarePath,

    [Parameter(Mandatory=$false)]
    [string]$Address = "0x08000000",

    [Parameter(Mandatory=$false)]
    [string]$TargetChip = "stm32f1x"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir  = Join-Path $ScriptDir "build"
$DefaultBin = Join-Path $BuildDir "firmware.bin"
$DefaultElf = Join-Path $BuildDir "firmware.elf"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   STM32 Auto ST-Link Flasher (OpenOCD)   " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Locate Toolchains (OpenOCD & GCC)
$openocdPath = $null
$openocdScripts = $null

$openocdCmd = Get-Command "openocd" -ErrorAction SilentlyContinue
if ($openocdCmd) {
    $openocdPath = $openocdCmd.Source
} else {
    $searchOpenocd = Get-ChildItem -Path "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\tools\xpack-openocd" -Filter "openocd.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($searchOpenocd) {
        $openocdPath = $searchOpenocd.FullName
        $scriptsDir = Join-Path (Split-Path -Parent (Split-Path -Parent $openocdPath)) "openocd\scripts"
        if (Test-Path $scriptsDir) {
            $openocdScripts = $scriptsDir
        }
    }
}

$gccPath = $null
$gccCmd = Get-Command "arm-none-eabi-gcc" -ErrorAction SilentlyContinue
if ($gccCmd) {
    $gccPath = $gccCmd.Source
} else {
    $foundGcc = Get-ChildItem -Path "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\tools\xpack-arm-none-eabi-gcc" -Filter "arm-none-eabi-gcc.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($foundGcc) { $gccPath = $foundGcc.FullName }
}

$objcopyPath = $null
if ($gccPath) {
    $objcopyPath = Join-Path (Split-Path -Parent $gccPath) "arm-none-eabi-objcopy.exe"
}

# 2. Auto-Detect or Auto-Build Firmware
if (-not $FirmwarePath) {
    # Check default build output
    if (Test-Path $DefaultBin) {
        $FirmwarePath = $DefaultBin
        Write-Host "[AUTO] Using build output: $FirmwarePath" -ForegroundColor Green
    } elseif (Test-Path $DefaultElf) {
        $FirmwarePath = $DefaultElf
        Write-Host "[AUTO] Using build output: $FirmwarePath" -ForegroundColor Green
    } else {
        # Search workspace for any existing .bin or .elf
        $foundFiles = Get-ChildItem -Path $ScriptDir -Include *.bin,*.elf -Recurse -Depth 3 -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch "node_modules|\.git" }
        if ($foundFiles.Count -gt 0) {
            $FirmwarePath = $foundFiles[0].FullName
            Write-Host "[AUTO] Discovered binary: $FirmwarePath" -ForegroundColor Green
        }
    }
}

# 3. If no binary exists yet, compile automatically
if (-not $FirmwarePath -or -not (Test-Path $FirmwarePath)) {
    if ($gccPath -and (Test-Path $gccPath)) {
        Write-Host "[BUILD] No pre-built binary found. Compiling codes/*.c automatically..." -ForegroundColor Yellow
        if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

        $core = "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\hardware\stm32\3.0.0"
        $cmsis = "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\tools\CMSIS\6.3.0\CMSIS\Core\Include"
        $ld = "$core\system\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\gcc\linker\stm32f103xb_flash.ld"

        $inc = @(
            "-I$ScriptDir\library",
            "-I$core\system\Drivers\STM32F1xx_HAL_Driver\Inc",
            "-I$core\system\Drivers\CMSIS\Device\ST\STM32F1xx\Include",
            "-I$cmsis"
        )

        $src = @(
            "$ScriptDir\codes\main.c",
            "$ScriptDir\codes\dht11.c",
            "$ScriptDir\codes\i2c_lcd.c",
            "$ScriptDir\codes\w25q128.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_cortex.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_rcc.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_rcc_ex.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_gpio.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_uart.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_dma.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_i2c.c",
            "$core\system\Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_spi.c",
            "$core\system\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\system_stm32f1xx.c",
            "$core\system\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\gcc\startup_stm32f103xb.s"
        )

        $flags = @(
            "-mcpu=cortex-m3", "-mthumb", "-O2",
            "-DSTM32F103xB", "-DUSE_HAL_DRIVER",
            "-T$ld",
            "--specs=nano.specs", "--specs=nosys.specs",
            "-Wl,--gc-sections"
        )

        & $gccPath $flags $inc $src -o $DefaultElf
        if ($LASTEXITCODE -eq 0 -and (Test-Path $objcopyPath)) {
            & $objcopyPath -O binary $DefaultElf $DefaultBin
            $FirmwarePath = $DefaultBin
            Write-Host "[BUILD] Compilation successful! Generated: $FirmwarePath" -ForegroundColor Green
        } else {
            Write-Host "[ERROR] Auto-build failed." -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "[ERROR] No firmware binary found and ARM GCC compiler not available." -ForegroundColor Red
        exit 1
    }
}

Write-Host "[TARGET] Flashing file: $FirmwarePath" -ForegroundColor Cyan

# 4. Program Target via OpenOCD
if ($openocdPath) {
    Write-Host "[TOOL] Using OpenOCD: $openocdPath" -ForegroundColor Green
    Write-Host "Connecting to ST-LINK V2 (SWD, verify, reset)..." -ForegroundColor Cyan

    $ext = [System.IO.Path]::GetExtension($FirmwarePath).ToLower()
    $absFirmware = (Resolve-Path $FirmwarePath).Path.Replace('\', '/')

    $openocdArgs = @()
    if ($openocdScripts) {
        $openocdArgs += @("-s", $openocdScripts)
    }
    $openocdArgs += @(
        "-f", "interface/stlink.cfg",
        "-c", "reset_config none",
        "-f", "target/$TargetChip.cfg",
        "-c", "adapter speed 1000"
    )

    if ($ext -eq ".elf") {
        $openocdArgs += @("-c", "program {$absFirmware} verify reset exit")
    } else {
        $openocdArgs += @("-c", "program {$absFirmware} $Address verify reset exit")
    }

    & $openocdPath $openocdArgs

    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n>>> SUCCESS: Firmware flashed and running on STM32! <<<`n" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "`n[!] OpenOCD flashing failed (Exit code: $LASTEXITCODE)." -ForegroundColor Yellow
        Write-Host "Troubleshooting Checklist:" -ForegroundColor Cyan
        Write-Host "  1. Check 4-wire ST-Link connections:" -ForegroundColor Gray
        Write-Host "     SWDIO -> PA13 | SWCLK -> PA14 | GND -> GND | 3.3V -> 3.3V" -ForegroundColor Gray
        Write-Host "  2. If target is busy/locked by previous firmware:" -ForegroundColor Gray
        Write-Host "     - Move the BOOT0 jumper from '0' to '1'" -ForegroundColor Gray
        Write-Host "     - Press the board hardware RESET button" -ForegroundColor Gray
        Write-Host "     - Run .\flash_firmware.ps1 again" -ForegroundColor Gray
        Write-Host "     - Move BOOT0 back to '0'" -ForegroundColor Gray
        exit $LASTEXITCODE
    }
}

Write-Host "[ERROR] OpenOCD flasher was not found." -ForegroundColor Red
exit 1
