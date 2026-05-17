<#
.SYNOPSIS
    Build script for Waveshare ESP32-S3 monorepo
.EXAMPLE
    .\build.ps1 knob                            # Build all Knob projects
    .\build.ps1 knob Basic_Blink                # Build Knob Basic_Blink only
    .\build.ps1 knob Basic_Encoder -Upload      # Build + upload (auto-detect port)
    .\build.ps1 amoled MyProject -Upload -Port COM13 -Monitor
    .\build.ps1 knob Basic_Blink -Flash         # Upload last build without rebuilding
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
    [switch]$ListDevices,
    [switch]$NoDeviceCheck
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

# ESP32-S3 native first, CH340 fallback. Returns COM string or $null.
function Find-EspPort {
    $dev = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.DeviceID -match 'VID_303A&PID_1001' -and $_.Name -match 'COM\d+' } |
        Select-Object -First 1
    if ($dev -and $dev.Name -match '(COM\d+)') { return $Matches[1] }
    $dev = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.DeviceID -match 'VID_1A86&PID_7523' -and $_.Name -match 'COM\d+' } |
        Select-Object -First 1
    if ($dev -and $dev.Name -match '(COM\d+)') { return $Matches[1] }
    return $null
}

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
    Write-Error "Usage: .\build.ps1 <device|auto> [project...] [-Upload] [-Flash] [-Port COMx] [-Monitor] [-Clean]`nUse -ListDevices to see available devices. Use 'auto' to identify device from MAC."
}

# Auto-resolve device from MAC if requested
if ($Device -eq 'auto') {
    if (-not $Port) {
        Write-Host 'Detecting ESP32-S3 port for auto-resolve...' -ForegroundColor Cyan
        $Port = Find-EspPort
        if (-not $Port) {
            Write-Error 'No device found. Plug in the board or specify -Port COMx'
        }
        Write-Host "Found: $Port" -ForegroundColor Green
    }
    Write-Host 'Resolving device from MAC...' -ForegroundColor Cyan
    $checker = Join-Path $RepoDir 'tools\device_mac.py'
    $pyExe = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
    $resolved = & $pyExe $checker resolve --port $Port
    if ($LASTEXITCODE -ne 0 -or -not $resolved) {
        Write-Error 'Could not auto-resolve device.'
    }
    $Device = $resolved.Trim()
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
    $Port = Find-EspPort
    if (-not $Port) {
        Write-Error 'No device found. Plug in the board or specify -Port COMx'
    }
    Write-Host "Found: $Port" -ForegroundColor Green
}

# Device check (devices.local.yaml MAC ↔ device_dir)
# Exit codes from device_mac.py check :
#   0 = match OK, 1 = explicit refusal (mismatch / secondary), 2 = skip (no MAC read)
# We abort only on 1.
if (-not $NoDeviceCheck -and ($Upload -or $Flash)) {
    $inventory = Join-Path $RepoDir 'devices.local.yaml'
    if (Test-Path $inventory) {
        Write-Host 'Checking device identity...' -ForegroundColor Cyan
        $checker = Join-Path $RepoDir 'tools\device_mac.py'
        $pyExe = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
        & $pyExe $checker check $Device --port $Port
        if ($LASTEXITCODE -eq 1) {
            Write-Host "Aborted. Use -NoDeviceCheck to override." -ForegroundColor Red
            exit 1
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
