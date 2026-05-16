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
        --port)          PORT="${2:-}"; shift ;;
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

# --- Build loop ---
printf "\n%s[%s]%s\n" "$CYAN" "$DEVICE" "$RESET"
for proj in "${PROJECTS[@]}"; do
    dir="$PROJECTS_DIR/$proj"
    printf "\n%s=== %s/%s ===%s\n" "$WHITE" "$DEVICE" "$proj" "$RESET"

    if [ "$CLEAN" = "1" ]; then
        echo "Cleaning..."
        "$PIO" run -d "$dir" -t clean
    fi

    echo "Building..."
    "$PIO" run -d "$dir"
done

printf "\n%sDone.%s\n" "$GREEN" "$RESET"
