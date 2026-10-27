<#
.SYNOPSIS
    One-Click Build and Flash Script for STM32 using Arduino CLI & OpenOCD
.DESCRIPTION
    Compiles source files in codes/ (with headers in library/ and HAL drivers from the
    installed Arduino STM32 core) and flashes via ST-LINK V2 using OpenOCD.
#>

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutputDir = Join-Path $ScriptDir "build"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   STM32 Build & Flash (Arduino/OpenOCD)  " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Locate arm-none-eabi-gcc and objcopy from Arduino core or PATH
$gcc = $null
$objcopy = $null

$gccCmd = Get-Command "arm-none-eabi-gcc" -ErrorAction SilentlyContinue
if ($gccCmd) {
    $gcc = $gccCmd.Source
    $objcopy = (Get-Command "arm-none-eabi-objcopy" -ErrorAction SilentlyContinue).Source
} else {
    $searchGcc = Get-ChildItem -Path "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\tools\xpack-arm-none-eabi-gcc" -Filter "arm-none-eabi-gcc.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($searchGcc) {
        $gcc = $searchGcc.FullName
        $objcopy = Join-Path (Split-Path -Parent $gcc) "arm-none-eabi-objcopy.exe"
    }
}

# 2. Locate HAL & CMSIS headers and sources from Arduino STM32 platform
$halInc = @()
$extraSources = @()
$ldScript = $null
$arduinoCoreBase = "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\hardware\stm32"
if (Test-Path $arduinoCoreBase) {
    $latestCore = Get-ChildItem -Path $arduinoCoreBase | Sort-Object Name -Descending | Select-Object -First 1
    if ($latestCore) {
        $coreRoot = $latestCore.FullName
        $halDriverInc = Join-Path $coreRoot "system\Drivers\STM32F1xx_HAL_Driver\Inc"
        $cmsisDeviceInc = Join-Path $coreRoot "system\Drivers\CMSIS\Device\ST\STM32F1xx\Include"

        if (Test-Path $halDriverInc) { $halInc += "-I$halDriverInc" }
        if (Test-Path $cmsisDeviceInc) { $halInc += "-I$cmsisDeviceInc" }

        # CMSIS Core include
        $cmsisCore = Get-ChildItem -Path "$env:LOCALAPPDATA\Arduino15\packages\STMicroelectronics\tools\CMSIS" -Filter "Include" -Recurse -Directory -ErrorAction SilentlyContinue | Where-Object { $_.FullName -like "*Core\Include" } | Select-Object -First 1
        if ($cmsisCore) { $halInc += "-I$($cmsisCore.FullName)" }

        # Startup assembly and CMSIS system file
        $startupFile = Join-Path $coreRoot "system\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\gcc\startup_stm32f103xb.s"
        $systemFile = Join-Path $coreRoot "system\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\system_stm32f1xx.c"
        if (Test-Path $startupFile) { $extraSources += $startupFile }
        if (Test-Path $systemFile) { $extraSources += $systemFile }

        # Key HAL driver C sources required by project
        $halSrcDir = Join-Path $coreRoot "system\Drivers\STM32F1xx_HAL_Driver\Src"
        $halDrivers = @("stm32f1xx_hal.c", "stm32f1xx_hal_cortex.c", "stm32f1xx_hal_gpio.c", "stm32f1xx_hal_rcc.c", "stm32f1xx_hal_rcc_ex.c", "stm32f1xx_hal_dma.c", "stm32f1xx_hal_pwr.c", "stm32f1xx_hal_flash.c", "stm32f1xx_hal_flash_ex.c", "stm32f1xx_hal_i2c.c", "stm32f1xx_hal_spi.c", "stm32f1xx_hal_uart.c")
        foreach ($hd in $halDrivers) {
            $p = Join-Path $halSrcDir $hd
            if (Test-Path $p) { $extraSources += $p }
        }

        # Linker script for STM32F103C8T/CBT6
        $ldCandidate = Join-Path $coreRoot "variants\STM32F1xx\F103C8T_F103CB(T-U)\ldscript.ld"
        if (Test-Path $ldCandidate) { $ldScript = $ldCandidate }
    }
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$elfFile = Join-Path $OutputDir "firmware.elf"
$binFile = Join-Path $OutputDir "firmware.bin"

# 3. Compile sources if GCC is available
if ($gcc -and (Test-Path $gcc)) {
    Write-Host "[1/3] Found ARM GCC Toolchain: $gcc" -ForegroundColor Green
    Write-Host "      Compiling sources from codes/ (Headers in library/)..." -ForegroundColor Cyan

    $sources = (Get-ChildItem -Path (Join-Path $ScriptDir "codes\*.c") | ForEach-Object { $_.FullName }) + $extraSources
    $includes = @("-I" + (Join-Path $ScriptDir "library")) + $halInc

    # Compile for Cortex-M3 (STM32F103CBT6: 128KB Flash, 20KB RAM)
    $cflags = @("-mcpu=cortex-m3", "-mthumb", "-O2", "-DSTM32F103xB", "-DUSE_HAL_DRIVER", "--specs=nano.specs", "--specs=nosys.specs")
    if ($ldScript) {
        $cflags += "-T$ldScript"
        $cflags += "-Wl,--defsym=LD_FLASH_OFFSET=0"
        $cflags += "-Wl,--defsym=LD_MAX_SIZE=128K"
        $cflags += "-Wl,--defsym=LD_MAX_DATA_SIZE=20K"
    }

    & $gcc $cflags $includes -o $elfFile $sources

    if ($LASTEXITCODE -eq 0) {
        Write-Host "[2/3] Generating raw binary..." -ForegroundColor Cyan
        & $objcopy -O binary $elfFile $binFile
        Write-Host "[OK] Binary created: $binFile" -ForegroundColor Green
    } else {
        Write-Host "[!] Standalone C link requires startup vector table. If using CubeIDE or Arduino, build output will be detected." -ForegroundColor Yellow
    }
} else {
    Write-Host "[INFO] ARM GCC compiler not found." -ForegroundColor Yellow
}

# 4. Check for binary to flash
Write-Host "[3/3] Checking firmware file to flash via ST-LINK..." -ForegroundColor Cyan

$firmwareToFlash = $null
if (Test-Path $binFile) {
    $firmwareToFlash = $binFile
} else {
    $existing = Get-ChildItem -Path $ScriptDir -Include *.bin,*.elf -Recurse -Depth 3 -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch "node_modules|\.git" }
    if ($existing.Count -gt 0) {
        $firmwareToFlash = $existing[0].FullName
    }
}

if (-not $firmwareToFlash) {
    Write-Host "[INFO] No pre-built firmware binary (.bin / .elf) found in workspace." -ForegroundColor Yellow
    Write-Host "       Build your project in STM32CubeIDE or Arduino CLI first." -ForegroundColor Yellow
    exit 0
}

# 5. Program via flash_firmware.ps1
Write-Host "[FLASH] Invoking ST-Link programmer with: $firmwareToFlash" -ForegroundColor Green
& (Join-Path $ScriptDir "flash_firmware.ps1") -FirmwarePath $firmwareToFlash
