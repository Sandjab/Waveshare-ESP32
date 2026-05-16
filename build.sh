#!/usr/bin/env bash
# Build script for Waveshare ESP32-S3 monorepo — macOS / Linux counterpart of build.ps1.
# bash 3.2 compatible (macOS native).
set -euo pipefail

# --- Color codes ---
CYAN=$'\033[36m'
GREEN=$'\033[32m'
YELLOW=$'\033[33m'
RED=$'\033[31m'
WHITE=$'\033[37m'
RESET=$'\033[0m'

# --- Args defaults ---
DEVICE=""
PROJECTS=()
UPLOAD=0
FLASH=0
MONITOR=0
CLEAN=0
LIST_DEVICES=0
PORT=""

usage() {
    cat <<EOF
Usage: ./build.sh <device> [project...] [options]
       ./build.sh --list-devices

Options:
  --upload          Build then flash (auto-detect port unless --port)
  --flash           Flash existing build without rebuilding
  --port PATH       Override port autodetect (e.g. /dev/cu.usbmodem*)
  --monitor         Open serial monitor after build/upload
  --clean           Clean before rebuilding
  --list-devices    List available devices and exit
  --help, -h        Show this help

Examples:
  ./build.sh knob Basic_Blink
  ./build.sh knob Basic_Encoder --upload --monitor
  ./build.sh knob --clean
EOF
}

# --- Parse args ---
while [ $# -gt 0 ]; do
    case "$1" in
        --upload)        UPLOAD=1 ;;
        --flash)         FLASH=1 ;;
        --monitor)       MONITOR=1 ;;
        --clean)         CLEAN=1 ;;
        --list-devices)  LIST_DEVICES=1 ;;
        --port)
            if [ -z "${2:-}" ]; then
                echo "Error: --port requires a value (e.g. /dev/cu.usbmodem*)" >&2
                exit 1
            fi
            PORT="$2"
            shift
            ;;
        --help|-h)       usage; exit 0 ;;
        --*)             echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
        *)
            if [ -z "$DEVICE" ]; then
                DEVICE="$1"
            else
                PROJECTS+=("$1")
            fi
            ;;
    esac
    shift
done

# --- Paths ---
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEVICES_DIR="$REPO_DIR/devices"

# --- Resolve pio ---
if command -v pio >/dev/null 2>&1; then
    PIO="$(command -v pio)"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
    PIO="$HOME/.platformio/penv/bin/pio"
else
    echo "PlatformIO not found. See docs/install/macos.md" >&2
    exit 1
fi

# --- Port autodetect (ESP32-S3 native 303A:1001, CH340 fallback 1A86:7523) ---
detect_port() {
    "$PIO" device list --json-output 2>/dev/null | python3 -c '
import json, sys
try:
    devs = json.load(sys.stdin)
except Exception:
    sys.exit(1)
# Prefer ESP32-S3 native, fallback to CH340
for vidpid in ["VID:PID=303A:1001", "VID:PID=1A86:7523"]:
    for d in devs:
        hwid = d.get("hwid", "") or ""
        if vidpid in hwid:
            print(d.get("port", ""))
            sys.exit(0)
sys.exit(1)
'
}

# --- Upload wrapper: streams output, "Hard resetting" = success regardless of exit code ---
run_upload() {
    local flash_ok=0
    while IFS= read -r line; do
        if [ "$flash_ok" = "1" ]; then continue; fi
        printf '%s\n' "$line"
        case "$line" in *"Hard resetting"*) flash_ok=1 ;; esac
    done < <("$@" 2>&1)
    if [ "$flash_ok" = "1" ]; then
        printf "%sUpload OK%s\n" "$GREEN" "$RESET"
    else
        printf "%sUpload FAILED%s\n" "$RED" "$RESET" >&2
        exit 1
    fi
}

# --- List devices ---
if [ "$LIST_DEVICES" = "1" ]; then
    printf "%sAvailable devices:%s\n" "$CYAN" "$RESET"
    for d in "$DEVICES_DIR"/*/; do
        name="$(basename "$d")"
        if [ -d "$d/projects" ]; then
            count=$(find "$d/projects" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | wc -l | tr -d ' ')
        else
            count=0
        fi
        printf "  %s (%s projects)\n" "$name" "$count"
    done
    exit 0
fi

# --- Validate device ---
if [ -z "$DEVICE" ]; then
    usage >&2
    exit 1
fi

DEVICE_DIR="$DEVICES_DIR/$DEVICE"
if [ ! -d "$DEVICE_DIR" ]; then
    echo "Unknown device: $DEVICE. Use --list-devices." >&2
    exit 1
fi

PROJECTS_DIR="$DEVICE_DIR/projects"
if [ ! -d "$PROJECTS_DIR" ]; then
    echo "No projects directory for device: $DEVICE" >&2
    exit 1
fi

# --- Default projects = all ---
if [ ${#PROJECTS[@]} -eq 0 ]; then
    for p in "$PROJECTS_DIR"/*/; do
        [ -d "$p" ] && PROJECTS+=("$(basename "$p")")
    done
fi

if [ ${#PROJECTS[@]} -eq 0 ]; then
    printf "%sNo projects found for device '%s'%s\n" "$YELLOW" "$DEVICE" "$RESET"
    exit 0
fi

# --- Validate projects ---
for p in "${PROJECTS[@]}"; do
    if [ ! -d "$PROJECTS_DIR/$p" ]; then
        echo "Unknown project: $p (in device $DEVICE)" >&2
        exit 1
    fi
done

# --- Resolve port if needed ---
if [ "$UPLOAD" = "1" ] || [ "$FLASH" = "1" ] || [ "$MONITOR" = "1" ]; then
    if [ -z "$PORT" ]; then
        printf "%sDetecting ESP32-S3 port (VID:303A PID:1001)...%s\n" "$CYAN" "$RESET"
        if PORT="$(detect_port)" && [ -n "$PORT" ]; then
            printf "%sFound: %s%s\n" "$GREEN" "$PORT" "$RESET"
        else
            printf "%sNo device found. Plug in the board or use --port /dev/cu.xxx%s\n" "$RED" "$RESET" >&2
            exit 1
        fi
    fi
fi

# --- Build loop ---
PYTHON_BIN="$HOME/.platformio/penv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"

printf "\n%s[%s]%s\n" "$CYAN" "$DEVICE" "$RESET"
for proj in "${PROJECTS[@]}"; do
    dir="$PROJECTS_DIR/$proj"
    printf "\n%s=== %s/%s ===%s\n" "$WHITE" "$DEVICE" "$proj" "$RESET"

    if [ "$FLASH" = "1" ]; then
        bin="$dir/.pio/build/esp32s3/firmware.bin"
        if [ ! -f "$bin" ]; then
            echo "No firmware found for $proj — build first." >&2
            exit 1
        fi
        echo "Flashing (no rebuild)..."
        bootloader="$dir/.pio/build/esp32s3/bootloader.bin"
        partitions="$dir/.pio/build/esp32s3/partitions.bin"
        run_upload "$PYTHON_BIN" "$ESPTOOL" \
            --chip esp32s3 --port "$PORT" --baud 921600 \
            write_flash 0x0000 "$bootloader" 0x8000 "$partitions" 0x10000 "$bin"
    else
        if [ "$CLEAN" = "1" ]; then
            echo "Cleaning..."
            "$PIO" run -d "$dir" -t clean
        fi
        if [ "$UPLOAD" = "1" ]; then
            echo "Building + uploading..."
            run_upload "$PIO" run -d "$dir" -t upload --upload-port "$PORT"
        else
            echo "Building..."
            "$PIO" run -d "$dir"
        fi
    fi

    if [ "$MONITOR" = "1" ]; then
        # macOS keeps the serial port locked briefly after esptool resets the
        # board; give it a couple of seconds before opening the monitor.
        if [ "$UPLOAD" = "1" ] || [ "$FLASH" = "1" ]; then
            sleep 2
        fi
        echo "Monitor (Ctrl-C to exit)..."
        "$PIO" device monitor -d "$dir" --port "$PORT"
    fi
done

printf "\n%sDone.%s\n" "$GREEN" "$RESET"
