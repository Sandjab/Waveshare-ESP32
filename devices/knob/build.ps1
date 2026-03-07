<#
.SYNOPSIS
    Build script for Waveshare Knob projects
.EXAMPLE
    .\build.ps1                             # Build all projects
    .\build.ps1 Test01                      # Build Test01 only
    .\build.ps1 Test02 -Upload              # Build + upload (auto-detect port)
    .\build.ps1 Test01 -Upload -Port COM13 -Monitor
    .\build.ps1 Test01 -Flash               # Upload last build without rebuilding
    .\build.ps1 -Clean                      # Clean + rebuild all
#>
param(
    [string[]]$Projects,
    [switch]$Upload,
    [switch]$Flash,
    [string]$Port,
    [switch]$Monitor,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$RepoDir = $PSScriptRoot
$ProjectsDir = Join-Path $RepoDir 'projects'
$Pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'

if (-not (Test-Path $Pio)) {
    Write-Error "PlatformIO not found at $Pio"
}

# Default: all projects
if (-not $Projects) {
    $Projects = Get-ChildItem $ProjectsDir -Directory | ForEach-Object { $_.Name }
}

# Validate project names
foreach ($p in $Projects) {
    if (-not (Test-Path (Join-Path $ProjectsDir $p))) {
        Write-Error "Unknown project: $p"
    }
}

# Auto-detect port
if (($Upload -or $Flash -or $Monitor) -and -not $Port) {
    Write-Host 'Detecting ESP32-S3 port (VID:303A PID:1001)...' -ForegroundColor Cyan

    $dev = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.DeviceID -match 'VID_303A&PID_1001' -and $_.Name -match 'COM\d+' } |
        Select-Object -First 1

    if ($dev -and $dev.Name -match '(COM\d+)') {
        $Port = $Matches[1]
        Write-Host "Found: $Port" -ForegroundColor Green
    }
    else {
        Write-Host 'ESP32-S3 not detected. Trying CH340 (VID:1A86 PID:7523)...' -ForegroundColor Yellow

        $dev = Get-CimInstance Win32_PnPEntity |
            Where-Object { $_.DeviceID -match 'VID_1A86&PID_7523' -and $_.Name -match 'COM\d+' } |
            Select-Object -First 1

        if ($dev -and $dev.Name -match '(COM\d+)') {
            $Port = $Matches[1]
            Write-Host "Found CH340: $Port (flip USB-C cable for ESP32-S3 side)" -ForegroundColor Yellow
        }
        else {
            Write-Error 'No device found. Plug in the board or specify -Port COMx'
        }
    }
}

# Build loop
foreach ($proj in $Projects) {
    $dir = Join-Path $ProjectsDir $proj
    Write-Host "`n=== $proj ===" -ForegroundColor White

    if ($Flash) {
        # Flash only - skip build, use esptool directly
        $bin = Join-Path $dir '.pio\build\esp32s3\firmware.bin'
        if (-not (Test-Path $bin)) {
            Write-Error "No firmware found for $proj - build first"
        }
        Write-Host 'Flashing (no rebuild)...'
        $python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
        $esptoolPkg = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy'
        $bootloader = Join-Path $dir '.pio\build\esp32s3\bootloader.bin'
        $partitions = Join-Path $dir '.pio\build\esp32s3\partitions.bin'
        & $python (Join-Path $esptoolPkg 'esptool.py') --chip esp32s3 --port $Port --baud 921600 write_flash 0x0000 $bootloader 0x8000 $partitions 0x10000 $bin
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    else {
        if ($Clean) {
            Write-Host 'Cleaning...'
            & $Pio run -d $dir -t clean
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }

        Write-Host 'Building...'
        & $Pio run -d $dir
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        if ($Upload) {
            Write-Host 'Uploading...'
            $uploadArgs = @('run', '-d', $dir, '-t', 'upload')
            if ($Port) { $uploadArgs += '--upload-port', $Port }
            & $Pio @uploadArgs
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
    }

    if ($Monitor) {
        Write-Host 'Monitor (Ctrl-C to exit)...'
        $monArgs = @('device', 'monitor', '-d', $dir)
        if ($Port) { $monArgs += '--port', $Port }
        & $Pio @monArgs
    }
}

Write-Host "`nDone." -ForegroundColor Green
