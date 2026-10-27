<#
.SYNOPSIS
    Serial Data Logger & Flash Memory Dump Utility
.DESCRIPTION
    Connects to the STM32 USART1 port (115200 baud), sends the 'D' command
    to dump all stored records from W25QXX Flash memory, and saves them to CSV.
.EXAMPLE
    .\dump_logs.ps1 -Port COM3
#>

param (
    [Parameter(Mandatory=$false)]
    [string]$Port,

    [Parameter(Mandatory=$false)]
    [int]$BaudRate = 115200
)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "    STM32 Flash Log Extraction Tool       " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Detect or validate serial port
if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 1) {
        $Port = $ports[0]
        Write-Host "[AUTO] Detected single active serial port: $Port" -ForegroundColor Green
    } elseif ($ports.Count -gt 1) {
        Write-Host "Multiple COM ports detected: $($ports -join ', ')" -ForegroundColor Yellow
        $Port = $ports[0]
        Write-Host "[DEFAULT] Using: $Port (Specify with -Port <COMx> to override)" -ForegroundColor Gray
    } else {
        Write-Host "[ERROR] No active COM ports detected. Please connect your USB-TTL or ST-Link VCP." -ForegroundColor Red
        exit 1
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$csvFile = "temperature_dump_$timestamp.csv"

Write-Host "Connecting to $Port at $BaudRate baud..." -ForegroundColor Cyan

try {
    $sp = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, None, 8, One
    $sp.ReadTimeout = 3000
    $sp.Open()

    Write-Host "[CONNECTED] Sending dump command 'D' to STM32..." -ForegroundColor Green
    $sp.Write("D")

    Start-Sleep -Milliseconds 500

    $lines = @()
    $capture = $false
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    while ($stopwatch.ElapsedMilliseconds -lt 8000) {
        try {
            $line = $sp.ReadLine()
            Write-Host $line

            if ($line -match "RecordID|ElapsedSec|Temp_C") {
                $capture = $true
            }

            if ($capture) {
                $lines += $line
            }

            if ($line -match "END OF DUMP") {
                break
            }
        } catch [System.TimeoutException] {
            if ($lines.Count -gt 0) { break }
        }
    }

    $sp.Close()

    if ($lines.Count -gt 0) {
        $lines | Out-File -FilePath $csvFile -Encoding utf8
        Write-Host "`n>>> SUCCESS: Captured $($lines.Count) lines to $csvFile <<<`n" -ForegroundColor Green
    } else {
        Write-Host "`n[INFO] No data captured. Ensure STM32 is powered and running." -ForegroundColor Yellow
    }

} catch {
    Write-Host "[ERROR] Serial error: $_" -ForegroundColor Red
    if ($sp -and $sp.IsOpen) { $sp.Close() }
}
