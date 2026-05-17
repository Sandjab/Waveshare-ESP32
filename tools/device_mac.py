#!/usr/bin/env python3
"""device_mac.py — utility for the devices.local.yaml device inventory.

Subcommands:
  scan  [--port PORT]
      Read the MAC of the ESP32 connected on PORT, look it up in
      devices.local.yaml, and print the label/device_dir. Use this to enrol
      new devices : run it once, copy the MAC into devices.local.yaml.

  check DEVICE_DIR [--port PORT]
      Verify the connected device's MAC corresponds to DEVICE_DIR.
      Used by build.sh / build.ps1 just before upload.
      Exit codes :
        0 = MAC matches DEVICE_DIR, or MAC unknown to inventory (warn-only)
        1 = MAC matches a different DEVICE_DIR, or it's the secondary chip
        2 = MAC could not be read (port absent, chip not responding)

  resolve [--port PORT]
      Print the device_dir of the connected device on stdout, deduced from
      the MAC. Used by build.sh / build.ps1 when `auto` is given instead
      of an explicit device name.
      Exit codes :
        0 = primary match (device_dir printed to stdout)
        1 = secondary chip (hint printed to stderr)
        2 = MAC unknown to inventory, or could not be read

The YAML parser is deliberately minimal — only the documented schema is
supported (top-level `devices:` list of flat dicts, 2-space indent, no
nested structures, no multi-line strings).
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INVENTORY = REPO_ROOT / "devices.local.yaml"
ESPTOOL = Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"

if sys.platform == "win32":
    PIO_PYTHON = Path.home() / ".platformio" / "penv" / "Scripts" / "python.exe"
else:
    PIO_PYTHON = Path.home() / ".platformio" / "penv" / "bin" / "python"


def parse_inventory() -> list[dict]:
    """Parse devices.local.yaml. Returns [] if file missing."""
    if not INVENTORY.exists():
        return []
    devices: list[dict] = []
    current: dict | None = None
    in_devices = False
    for raw in INVENTORY.read_text().splitlines():
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped == "devices:":
            in_devices = True
            continue
        if not in_devices:
            continue
        if stripped.startswith("- "):
            if current is not None:
                devices.append(current)
            current = {}
            stripped = stripped[2:]
        if current is None:
            continue
        m = re.match(r"^([a-zA-Z_][a-zA-Z0-9_]*):\s*(.*?)\s*$", stripped)
        if m:
            current[m.group(1)] = m.group(2)
    if current is not None:
        devices.append(current)
    return devices


def detect_port() -> str | None:
    """Same VID:PID logic as build.sh / build.ps1."""
    try:
        result = subprocess.run(
            [str(PIO_PYTHON), "-m", "platformio", "device", "list", "--json-output"],
            capture_output=True, text=True, timeout=10,
        )
        devs = json.loads(result.stdout or "[]")
    except Exception:
        return None
    for vidpid in ("VID:PID=303A:1001", "VID:PID=1A86:7523"):
        for d in devs:
            if vidpid in (d.get("hwid") or ""):
                return d.get("port")
    return None


def read_mac(port: str) -> str | None:
    # Retry: just after a previous esptool call, the chip is mid hard-reset and
    # the port re-enumerates → first attempt may fail.
    import time
    for attempt in range(3):
        try:
            result = subprocess.run(
                [str(PIO_PYTHON), str(ESPTOOL), "--port", port, "read_mac"],
                capture_output=True, text=True, timeout=20,
            )
        except Exception:
            time.sleep(1.5)
            continue
        for line in result.stdout.splitlines():
            line = line.strip()
            if line.startswith("MAC:"):
                mac = line.split(":", 1)[1].strip().lower()
                if re.fullmatch(r"[0-9a-f:]{17}", mac):
                    return mac
        time.sleep(1.5)
    return None


def lookup_mac(devices: list[dict], mac: str) -> tuple[dict | None, str | None]:
    """Return (entry, role) where role is 'primary', 'secondary' or None."""
    mac = mac.lower()
    for d in devices:
        if d.get("mac", "").lower() == mac:
            return d, "primary"
        if d.get("mac_secondary", "").lower() == mac:
            return d, "secondary"
    return None, None


def cmd_scan(args) -> int:
    port = args.port or detect_port()
    if not port:
        print("No ESP32-S3 port detected", file=sys.stderr)
        return 2
    print(f"Port: {port}")
    mac = read_mac(port)
    if not mac:
        print("Failed to read MAC", file=sys.stderr)
        return 2
    print(f"MAC : {mac}")
    entry, role = lookup_mac(parse_inventory(), mac)
    if entry is None:
        print("Match: NEW — not yet in devices.local.yaml")
        return 0
    tag = "" if role == "primary" else f" [{role.upper()}]"
    print(f"Match: {entry.get('label')}{tag}  →  device_dir={entry.get('device_dir')}")
    desc = entry.get("description")
    if desc:
        print(f"       {desc}")
    if role == "secondary":
        hint = entry.get("secondary_hint")
        if hint:
            print(f"       Hint : {hint}")
    return 0


def cmd_check(args) -> int:
    port = args.port or detect_port()
    if not port:
        print("[device-check] no port detected — skipping check", file=sys.stderr)
        return 2
    mac = read_mac(port)
    if not mac:
        print(f"[device-check] could not read MAC on {port} — skipping check", file=sys.stderr)
        return 2
    entry, role = lookup_mac(parse_inventory(), mac)
    if entry is None:
        print(
            f"[device-check] MAC {mac} not in devices.local.yaml — "
            f"flashing anyway. Run `tools/device_mac.py scan` to enrol.",
            file=sys.stderr,
        )
        return 0
    if role == "secondary":
        hint = entry.get("secondary_hint") or "this MAC belongs to a secondary, non-flashable chip"
        print(
            f"[device-check] REFUSED: this is the SECONDARY chip of "
            f"{entry.get('label')} ({args.device_dir}). {hint}",
            file=sys.stderr,
        )
        return 1
    if entry.get("device_dir") != args.device_dir:
        print(
            f"[device-check] REFUSED: connected device is "
            f"{entry.get('label')} (device_dir={entry.get('device_dir')}) "
            f"but you asked for device_dir={args.device_dir}.",
            file=sys.stderr,
        )
        return 1
    print(
        f"[device-check] OK: {entry.get('label')} matches {args.device_dir}",
        file=sys.stderr,
    )
    return 0


def cmd_resolve(args) -> int:
    port = args.port or detect_port()
    if not port:
        print("No ESP32-S3 port detected", file=sys.stderr)
        return 2
    mac = read_mac(port)
    if not mac:
        print(f"Failed to read MAC on {port}", file=sys.stderr)
        return 2
    entry, role = lookup_mac(parse_inventory(), mac)
    if entry is None:
        print(
            f"MAC {mac} not in devices.local.yaml — cannot auto-resolve. "
            f"Run `tools/device_mac.py scan` to enrol.",
            file=sys.stderr,
        )
        return 2
    if role == "secondary":
        hint = entry.get("secondary_hint") or "this MAC belongs to a secondary, non-flashable chip"
        print(
            f"This is the SECONDARY chip of {entry.get('label')} "
            f"(device_dir={entry.get('device_dir')}). {hint}",
            file=sys.stderr,
        )
        return 1
    # stdout = the device_dir, parseable by the caller. nothing else.
    print(entry.get("device_dir", ""))
    print(
        f"Auto-resolved: {entry.get('label')} → device_dir={entry.get('device_dir')}",
        file=sys.stderr,
    )
    return 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_scan = sub.add_parser("scan", help="Read MAC and lookup in inventory")
    p_scan.add_argument("--port")
    p_scan.set_defaults(func=cmd_scan)

    p_check = sub.add_parser("check", help="Verify connected device matches device_dir")
    p_check.add_argument("device_dir")
    p_check.add_argument("--port")
    p_check.set_defaults(func=cmd_check)

    p_resolve = sub.add_parser("resolve", help="Print the device_dir of the connected device")
    p_resolve.add_argument("--port")
    p_resolve.set_defaults(func=cmd_resolve)

    args = parser.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
