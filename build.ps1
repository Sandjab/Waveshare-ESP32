<#
.SYNOPSIS
    Build script for Waveshare ESP32-S3 monorepo
.EXAMPLE
    .\build.ps1 knob                            # Build all Knob projects
    .\build.ps1 knob Test01                     # Build Knob Test01 only
    .\build.ps1 knob Test02 -Upload             # Build + upload (auto-detect port)
    .\build.ps1 amoled Test01 -Upload -Port COM13 -Monitor
    .\build.ps1 knob Test01 -Flash              # Upload last build without rebuilding
    .\build.ps1 knob -Clean                     # Clean + rebuild all
    .\build.ps1 -ListDevices                    # Show available devices
#>
param(
    [string]$Device,
    [string[]]$Projects,
    [switch]$Upload,
    [switch]$Flash,
    [string]$Port,
    [switch]$Monitor,
    [switch]$Clean,
    [switch]$ListDevices
)

$ErrorActionPreference = 'Stop'

# Upload helper — streams output in real-time, suppresses post-hard-reset noise.
# "Hard resetting" from esptool = flash write succeeded. The exit code is
# unreliable because the post-reset port disconnect makes PIO report [FAILED].
function Invoke-Upload {
    param(
        [string]$Exe,
        [string[]]$Arguments
    )
    $ErrorActionPreference = 'Continue'   # let stderr flow without throwing
    $flashOk = $false
    & $Exe @Arguments 2>&1 | ForEach-Object {
        $line = $_.ToString()
        if ($flashOk) { return }          # discard post-reset noise
        Write-Host $line
        if ($line -match 'Hard resetting') { $flashOk = $true }
    }
    if ($flashOk) {
        Write-Host 'Upload OK' -ForegroundColor Green
    }
    else {
        Write-Host 'Upload FAILED' -ForegroundColor Red
        exit 1
    }
}

$RepoDir = $PSScriptRoot
$DevicesDir = Join-Path $RepoDir 'devices'
$Pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'

# List devices mode
if ($ListDevices) {
    Write-Host 'Available devices:' -ForegroundColor Cyan
    Get-ChildItem $DevicesDir -Directory | ForEach-Object {
        $projCount = (Get-ChildItem (Join-Path $_.FullName 'projects') -Directory -ErrorAction SilentlyContinue | Measure-Object).Count
        Write-Host "  $($_.Name) ($projCount projects)"
    }
    exit 0
}

if (-not $Device) {
    Write-Error "Usage: .\build.ps1 <device> [project...] [-Upload] [-Flash] [-Port COMx] [-Monitor] [-Clean]`nUse -ListDevices to see available devices."
}

# Resolve device directory
$DeviceDir = Join-Path $DevicesDir $Device
if (-not (Test-Path $DeviceDir)) {
    Write-Error "Unknown device: $Device. Use -ListDevices to see available devices."
}

$ProjectsDir = Join-Path $DeviceDir 'projects'
if (-not (Test-Path $ProjectsDir)) {
    Write-Error "No projects directory for device: $Device"
}

if (-not (Test-Path $Pio)) {
    Write-Error "PlatformIO not found at $Pio"
}

# Default: all projects for this device
if (-not $Projects) {
    $Projects = Get-ChildItem $ProjectsDir -Directory | ForEach-Object { $_.Name }
}

if (-not $Projects) {
    Write-Host "No projects found for device '$Device'" -ForegroundColor Yellow
    exit 0
}

# Validate project names
foreach ($p in $Projects) {
    if (-not (Test-Path (Join-Path $ProjectsDir $p))) {
        Write-Error "Unknown project: $p (in device $Device)"
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
Write-Host "`n[$Device]" -ForegroundColor Cyan
foreach ($proj in $Projects) {
    $dir = Join-Path $ProjectsDir $proj
    Write-Host "`n=== $Device/$proj ===" -ForegroundColor White

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
        $esptoolArgs = @(
            (Join-Path $esptoolPkg 'esptool.py'),
            '--chip', 'esp32s3', '--port', $Port, '--baud', '921600',
            'write_flash', '0x0000', $bootloader, '0x8000', $partitions, '0x10000', $bin
        )
        Invoke-Upload -Exe $python -Arguments $esptoolArgs
    }
    else {
        if ($Clean) {
            Write-Host 'Cleaning...'
            & $Pio run -d $dir -t clean
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }

        if ($Upload) {
            Write-Host 'Building + uploading...'
            $uploadArgs = @('run', '-d', $dir, '-t', 'upload')
            if ($Port) { $uploadArgs += '--upload-port', $Port }
            Invoke-Upload -Exe $Pio -Arguments $uploadArgs
        }
        else {
            Write-Host 'Building...'
            & $Pio run -d $dir
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
