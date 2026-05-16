# Cross-platform Build Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Permettre l'usage du repo `Waveshare-ESP32` indifféremment sous Windows et macOS, en ajoutant `build.sh` (parité fonctionnelle avec `build.ps1`), une doc d'install par OS, et un split de permissions Claude Code, sans aucune modification de `build.ps1`.

**Architecture:** Deux scripts côte à côte (`build.ps1` Windows intact, `build.sh` nouveau bash 3.2-compatible). CLI mirror : positional args identiques (`Device`, `Projects…`), flags adaptés aux conventions de chaque shell. Auto-detect port via `pio device list --json-output` + parsing `python3`. Permissions Claude Code splittées : `settings.json` tracké universel + `settings.local.json` gitignored OS-spécifique.

**Tech Stack:** bash 3.2 (macOS natif), PlatformIO Core CLI, python3 (macOS natif ≥ 10.15), Claude Code permissions, git.

**Spec source:** `docs/superpowers/specs/2026-05-16-cross-platform-build-design.md`

---

## File Structure

**À créer** :
- `build.sh` — script bash, parité 1:1 avec `build.ps1`
- `.claude/settings.json` — permissions universelles tracked
- `docs/install/macos.md` — install PIO sur macOS
- `docs/install/windows.md` — install PIO sur Windows

**À modifier** :
- `.gitignore` — ajout de `.claude/settings.local.json`
- `README.md` — section Build à deux flavors
- `.claude/settings.local.json` — détaché de git (reste sur disque)

**Intact** :
- `build.ps1`

---

## Task 1: Split permissions Claude Code + gitignore

**Files:**
- Create: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/.claude/settings.json`
- Modify: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/.gitignore`
- Untrack: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/.claude/settings.local.json`

- [ ] **Step 1: Créer `.claude/settings.json` (tracké, universel)**

Contenu exact :

```json
{
  "permissions": {
    "allow": [
      "WebFetch(domain:www.waveshare.com)",
      "WebFetch(domain:github.com)",
      "WebFetch(domain:components.espressif.com)",
      "WebFetch(domain:devices.esphome.io)",
      "WebFetch(domain:www.cnx-software.com)",
      "WebFetch(domain:www.electronics-lab.com)",
      "WebFetch(domain:raw.githubusercontent.com)",
      "mcp__plugin_context7_context7__resolve-library-id",
      "mcp__plugin_context7_context7__query-docs",
      "Bash(gh search:*)",
      "Bash(gh api:*)",
      "Bash(gh repo:*)",
      "Bash(pio:*)",
      "Bash(pio run:*)",
      "Bash(git:*)",
      "Bash(cp:*)",
      "Bash(rm:*)",
      "Bash(chmod +x:*)",
      "Bash(xxd:*)",
      "Bash(pip show:*)",
      "Bash(while read d)",
      "Bash(do echo \"=== $d ===\")",
      "Bash(./build.sh:*)",
      "Bash(bash ./build.sh:*)"
    ]
  }
}
```

- [ ] **Step 2: Ajouter `.claude/settings.local.json` au `.gitignore`**

Lire le `.gitignore` actuel puis ajouter à la fin :

```
# Claude Code permissions (per-user / per-OS)
.claude/settings.local.json
```

- [ ] **Step 3: Détacher `settings.local.json` de git**

```bash
git rm --cached .claude/settings.local.json
```

Attendu : `rm '.claude/settings.local.json'` (le fichier reste sur disque).

- [ ] **Step 4: Vérifier l'état**

```bash
git status
```

Attendu :
- `new file:   .claude/settings.json`
- `deleted:    .claude/settings.local.json`
- `modified:   .gitignore`
- `.claude/settings.local.json` ne doit PAS apparaître comme untracked (le `.gitignore` doit le masquer)

Vérifier qu'il n'est pas en untracked :
```bash
git status --ignored | grep "settings.local.json"
```
Attendu : `.claude/settings.local.json` apparaît sous `Ignored files`.

- [ ] **Step 5: Commit**

```bash
git add .claude/settings.json .gitignore
git commit -m "$(cat <<'EOF'
Split Claude Code permissions: tracked + per-user local

Move shared, OS-agnostic permissions to .claude/settings.json (tracked).
Untrack .claude/settings.local.json and add to .gitignore so each user/OS
can manage their own paths (Windows pio.exe vs macOS pio).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Docs d'install (macOS + Windows)

**Files:**
- Create: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/docs/install/macos.md`
- Create: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/docs/install/windows.md`

- [ ] **Step 1: Créer le dossier `docs/install/`**

```bash
mkdir -p docs/install
```

- [ ] **Step 2: Écrire `docs/install/macos.md`**

Contenu exact :

````markdown
# Installation PlatformIO — macOS

## Pré-requis

- macOS 10.15 (Catalina) ou plus récent
- `python3` natif (déjà présent depuis macOS 10.15)
- `curl` (déjà présent)

## Installation PlatformIO Core

On utilise le script officiel de PlatformIO. Il installe Core dans `~/.platformio/penv/` (cohérent avec l'install Windows) :

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py \
    -o /tmp/get-platformio.py
python3 /tmp/get-platformio.py
```

L'install met `pio` dans `~/.platformio/penv/bin/pio`.

## Ajout au PATH (optionnel mais recommandé)

```bash
echo 'export PATH="$HOME/.platformio/penv/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

Si tu utilises bash : `~/.bash_profile` au lieu de `~/.zshrc`.

## Vérification

```bash
pio --version
```

Attendu : `PlatformIO Core, version 6.x.x`.

Si `pio: command not found` : soit `PATH` pas mis à jour, soit ouvre un nouveau terminal.

## Permissions série

Aucune action nécessaire sur macOS (contrairement à Linux où il faut le groupe `dialout`).

## Settings Claude Code local

Crée `.claude/settings.local.json` (gitignored) avec les permissions spécifiques macOS :

```json
{
  "permissions": {
    "allow": [
      "Bash(~/.platformio/penv/bin/pio:*)",
      "Bash(~/.platformio/penv/bin/python:*)",
      "Bash(which pio:*)",
      "Bash(ls /dev/cu.*:*)",
      "Bash(ioreg:*)"
    ]
  }
}
```

## Drivers USB

- **ESP32-S3 natif (VID:303A PID:1001)** : aucun driver requis, reconnu nativement.
- **CH340 (VID:1A86 PID:7523)** : si le board est branché via le port "secondary" qui passe par un CH340, installer le driver depuis https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html.

## Détection de port

```bash
pio device list
```

Attendu (ESP32-S3 branché) : un port `/dev/cu.usbmodem*` avec `hwid` contenant `VID:PID=303A:1001`.
````

- [ ] **Step 3: Écrire `docs/install/windows.md`**

Contenu exact :

````markdown
# Installation PlatformIO — Windows

## Pré-requis

- Windows 10 ou 11
- Python 3.7+ (pour le mode `-Flash` qui appelle directement esptool)
- PowerShell 5+ (présent par défaut)

## Installation PlatformIO Core

Trois options :

### Option 1 : Script officiel (recommandé)

```powershell
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py" -OutFile "$env:TEMP\get-platformio.py"
python "$env:TEMP\get-platformio.py"
```

Installe PIO dans `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`.

### Option 2 : pip

```powershell
pip install --user platformio
```

### Option 3 : VS Code extension

Installer l'extension "PlatformIO IDE" depuis le marketplace VS Code. PIO Core est installé automatiquement au même endroit.

## Vérification

```powershell
where pio
pio --version
```

Si `pio` n'est pas dans le `PATH`, le `build.ps1` le trouve directement via `$env:USERPROFILE\.platformio\penv\Scripts\pio.exe`.

## Drivers USB

- **ESP32-S3 natif (VID:303A PID:1001)** : drivers Windows 10+ natifs.
- **CH340 (VID:1A86 PID:7523)** : installer le driver depuis https://www.wch-ic.com/downloads/CH341SER_ZIP.html si le board n'est pas reconnu.

## Settings Claude Code local

Crée `.claude/settings.local.json` (gitignored) avec les permissions spécifiques Windows :

```json
{
  "permissions": {
    "allow": [
      "Bash(~/.platformio/penv/Scripts/pio.exe:*)",
      "Bash(~/.platformio/penv/Scripts/python.exe:*)",
      "Bash(powershell.exe:*)",
      "Bash(powershell -Command:*)",
      "Bash(where pio:*)"
    ]
  }
}
```

## Détection de port

```powershell
pio device list
```

Attendu : un port `COM*` avec `hwid` contenant `VID:PID=303A:1001`.
````

- [ ] **Step 4: Vérifier les fichiers**

```bash
ls docs/install/
```

Attendu : `macos.md  windows.md`.

- [ ] **Step 5: Commit**

```bash
git add docs/install/
git commit -m "$(cat <<'EOF'
Add per-OS PlatformIO install docs

Document PIO Core install for macOS (curl + python3, target
~/.platformio/penv/) and Windows (pip / script / VS Code), plus
USB drivers, port detection, and Claude Code local permissions
templates for each OS.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Installer PlatformIO sur cette machine macOS

**Files:** aucun (action machine)

- [ ] **Step 1: Lancer l'installeur officiel**

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py \
    -o /tmp/get-platformio.py
python3 /tmp/get-platformio.py
```

Durée : 1-3 minutes. Crée `~/.platformio/penv/`.

- [ ] **Step 2: Vérifier l'install**

```bash
~/.platformio/penv/bin/pio --version
```

Attendu : `PlatformIO Core, version 6.x.x`.

- [ ] **Step 3: Ajouter au PATH (zshrc)**

```bash
echo 'export PATH="$HOME/.platformio/penv/bin:$PATH"' >> ~/.zshrc
```

Pour activer dans le shell courant (sans relancer) :

```bash
export PATH="$HOME/.platformio/penv/bin:$PATH"
```

- [ ] **Step 4: Confirmer `pio` dans le PATH**

```bash
which pio && pio --version
```

Attendu :
- `/Users/jean-paulgavini/.platformio/penv/bin/pio`
- `PlatformIO Core, version 6.x.x`

- [ ] **Step 5: Créer le `.claude/settings.local.json` macOS**

```json
{
  "permissions": {
    "allow": [
      "Bash(~/.platformio/penv/bin/pio:*)",
      "Bash(~/.platformio/penv/bin/python:*)",
      "Bash(which pio:*)",
      "Bash(ls /dev/cu.*:*)",
      "Bash(ioreg:*)"
    ]
  }
}
```

Vérifier qu'il est bien gitignored :
```bash
git status --ignored | grep "settings.local.json"
```
Attendu : présent sous `Ignored files`.

**Pas de commit** (fichier gitignored).

---

## Task 4: `build.sh` — skeleton, args, list-devices, build, clean

**Files:**
- Create: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/build.sh`

- [ ] **Step 1: Créer `build.sh` avec parsing args + list-devices + build + clean**

Contenu exact (à compléter par les tasks suivantes pour --upload / --flash / --monitor) :

```bash
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
```

- [ ] **Step 2: Rendre `build.sh` exécutable**

```bash
chmod +x build.sh
```

- [ ] **Step 3: Test `--help`**

```bash
./build.sh --help
```

Attendu : message d'usage qui contient `Usage: ./build.sh <device>`.

- [ ] **Step 4: Test `--list-devices`**

```bash
./build.sh --list-devices
```

Attendu :
```
Available devices:
  amoled (0 projects)
  knob (4 projects)
```

- [ ] **Step 5: Test build de Basic_Blink**

```bash
./build.sh knob Basic_Blink
```

Attendu : sortie PIO classique se terminant par `SUCCESS` et `Done.` en vert. La première fois, PIO télécharge la plateforme `pioarduino` (peut prendre quelques minutes).

- [ ] **Step 6: Test `--clean`**

```bash
./build.sh knob Basic_Blink --clean
```

Attendu : `Cleaning...` puis `Building...` puis `SUCCESS`.

- [ ] **Step 7: Test "device inconnu"**

```bash
./build.sh nonexistent_device
```

Attendu : exit 1 avec `Unknown device: nonexistent_device. Use --list-devices.`

- [ ] **Step 8: Test "projet inconnu"**

```bash
./build.sh knob FooBar
```

Attendu : exit 1 avec `Unknown project: FooBar (in device knob)`.

- [ ] **Step 9: Commit**

```bash
git add build.sh
git commit -m "$(cat <<'EOF'
Add build.sh skeleton (list/build/clean) for macOS / Linux

Mirrors build.ps1 args (positional device + projects, --clean, --list-devices).
bash 3.2 compatible. Resolves pio via PATH first, then ~/.platformio/penv/bin.
Auto-detect port and upload/flash/monitor modes come in later commits.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: `build.sh` — auto-detect port USB

**Files:**
- Modify: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/build.sh`

- [ ] **Step 1: Ajouter la fonction `detect_port`**

Après le bloc `# --- Resolve pio ---` et avant `# --- List devices ---`, insérer :

```bash
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
```

- [ ] **Step 2: Brancher l'auto-detect avant la build loop**

Juste avant `# --- Build loop ---`, ajouter :

```bash
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
```

- [ ] **Step 3: Test detect sans board branché**

Débrancher le board, puis :

```bash
./build.sh knob Basic_Blink --upload
```

Attendu : `Detecting ESP32-S3 port…` puis `No device found. Plug in the board or use --port /dev/cu.xxx` (exit 1).

Note : `--upload` ne fait encore rien (sera ajouté en Task 6), mais la résolution du port doit déjà déclencher.

- [ ] **Step 4: Test detect avec board branché**

Brancher le board (côté USB-C ESP32-S3 natif, pas CH340) :

```bash
./build.sh knob Basic_Blink --upload
```

Attendu :
- `Detecting ESP32-S3 port (VID:303A PID:1001)...`
- `Found: /dev/cu.usbmodem*` (selon la sortie de `pio device list`)

Puis le build s'enchaîne. L'upload lui-même n'est pas encore implémenté → arrive en Task 6.

- [ ] **Step 5: Test override `--port`**

```bash
./build.sh knob Basic_Blink --upload --port /dev/cu.fake
```

Attendu : pas de message `Detecting…`. Le script utilise `/dev/cu.fake`.

- [ ] **Step 6: Commit**

```bash
git add build.sh
git commit -m "$(cat <<'EOF'
Add port autodetect to build.sh

Uses 'pio device list --json-output' + python3 to find an ESP32-S3 by
hwid VID:PID=303A:1001 (CH340 1A86:7523 as fallback). Triggers only
when --upload, --flash, or --monitor is set, and is skipped if --port
is provided.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: `build.sh` — upload + monitor + "Hard resetting" detection

**Files:**
- Modify: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/build.sh`

- [ ] **Step 1: Ajouter la fonction `run_upload`**

Après la fonction `detect_port` (insérée en Task 5), ajouter :

```bash
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
```

- [ ] **Step 2: Remplacer la build loop par version `--upload` + `--monitor`**

Remplacer le bloc `# --- Build loop ---` complet par :

```bash
# --- Build loop ---
printf "\n%s[%s]%s\n" "$CYAN" "$DEVICE" "$RESET"
for proj in "${PROJECTS[@]}"; do
    dir="$PROJECTS_DIR/$proj"
    printf "\n%s=== %s/%s ===%s\n" "$WHITE" "$DEVICE" "$proj" "$RESET"

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

    if [ "$MONITOR" = "1" ]; then
        echo "Monitor (Ctrl-C to exit)..."
        "$PIO" device monitor -d "$dir" --port "$PORT"
    fi
done

printf "\n%sDone.%s\n" "$GREEN" "$RESET"
```

- [ ] **Step 3: Test `--upload`**

Brancher le board, puis :

```bash
./build.sh knob Basic_Blink --upload
```

Attendu :
- Detection port
- Build PIO (compile)
- Upload (esptool sortie streamée)
- Ligne `Hard resetting via RTS pin...` apparaît
- `Upload OK` en vert
- `Done.`

L'écran du knob doit clignoter (Basic_Blink).

- [ ] **Step 4: Test `--upload --monitor`**

```bash
./build.sh knob Basic_Blink --upload --monitor
```

Attendu : tout comme Step 3 puis le moniteur série démarre. Ctrl-C pour quitter.

- [ ] **Step 5: Test `--monitor` seul**

```bash
./build.sh knob Basic_Blink --monitor
```

Attendu : pas de build, pas d'upload, juste le moniteur série sur le port auto-détecté.

- [ ] **Step 6: Commit**

```bash
git add build.sh
git commit -m "$(cat <<'EOF'
Add --upload and --monitor modes to build.sh

run_upload() wraps the pio upload command, streams output, and treats
'Hard resetting' as the success signal (matching build.ps1 behavior,
since pio's exit code is unreliable post-flash on macOS too).
--monitor opens 'pio device monitor' on the resolved port.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: `build.sh` — mode `--flash` (esptool direct, pas de rebuild)

**Files:**
- Modify: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/build.sh`

- [ ] **Step 1: Ajouter la branche `--flash` dans la build loop**

Modifier la build loop pour intégrer le mode `--flash`. Remplacer le bloc actuel `# --- Build loop ---` par :

```bash
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
        echo "Monitor (Ctrl-C to exit)..."
        "$PIO" device monitor -d "$dir" --port "$PORT"
    fi
done

printf "\n%sDone.%s\n" "$GREEN" "$RESET"
```

- [ ] **Step 2: Test `--flash` après build préalable**

Pré-requis : avoir buildé `Basic_Blink` au préalable (le `.pio/build/esp32s3/firmware.bin` existe).

```bash
./build.sh knob Basic_Blink --flash
```

Attendu :
- Detection port
- `Flashing (no rebuild)...`
- esptool sortie : `Connecting...`, `Writing at 0x00010000...`, etc.
- `Hard resetting via RTS pin...`
- `Upload OK`
- `Done.`

Plus rapide que `--upload` car pas de recompile.

- [ ] **Step 3: Test `--flash` sans firmware**

Supprimer le firmware buildé :

```bash
rm -rf devices/knob/projects/Basic_Blink/.pio
./build.sh knob Basic_Blink --flash
```

Attendu : exit 1 avec `No firmware found for Basic_Blink — build first.`

- [ ] **Step 4: Re-builder pour ne pas laisser le repo en état moisi**

```bash
./build.sh knob Basic_Blink
```

Attendu : build OK.

- [ ] **Step 5: Commit**

```bash
git add build.sh
git commit -m "$(cat <<'EOF'
Add --flash mode to build.sh (esptool direct, no rebuild)

Mirrors build.ps1 -Flash: calls ~/.platformio/penv/bin/python directly
against esptool.py with the prebuilt bootloader/partitions/firmware
binaries. Reuses run_upload() for Hard-resetting detection.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: README.md — section Build à deux flavors

**Files:**
- Modify: `/Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/README.md`

- [ ] **Step 1: Remplacer la section Build du README**

Ouvrir `README.md`. Repérer la section `## Build`. Remplacer tout son contenu (depuis `## Build` jusqu'à la fin du fichier) par :

````markdown
## Build

Prérequis : [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation.html)
- **Windows** → [docs/install/windows.md](docs/install/windows.md)
- **macOS** → [docs/install/macos.md](docs/install/macos.md)

### Windows (PowerShell)

```powershell
.\build.ps1 knob Basic_Blink                   # Build
.\build.ps1 knob Basic_Blink -Upload           # Build + flash (autodetect port)
.\build.ps1 knob Basic_Blink -Upload -Monitor  # Build + flash + monitor série
.\build.ps1 knob -Clean                        # Clean + rebuild tous les projets du device
.\build.ps1 -ListDevices                       # Lister les devices disponibles
```

### macOS / Linux (bash)

```bash
./build.sh knob Basic_Blink                    # Build
./build.sh knob Basic_Blink --upload           # Build + flash (autodetect port)
./build.sh knob Basic_Blink --upload --monitor # Build + flash + monitor série
./build.sh knob --clean                        # Clean + rebuild tous les projets du device
./build.sh --list-devices                      # Lister les devices disponibles
```
````

- [ ] **Step 2: Vérifier le rendu**

```bash
cat README.md | tail -40
```

Attendu : section `## Build` avec les deux blocs Windows et macOS lisibles, liens vers `docs/install/...`.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "$(cat <<'EOF'
Update README build section with two-flavor instructions

Adds macOS / Linux flavor (./build.sh) alongside the existing
PowerShell flavor, and links to per-OS install docs.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Validation end-to-end sur macOS

**Files:** aucun (test manuel)

- [ ] **Step 1: Vérifier que `build.ps1` n'a PAS été touché**

```bash
git log --oneline build.ps1 | head -5
```

Attendu : le dernier commit sur `build.ps1` est antérieur à ce plan (commit `c3dd7e5` ou plus ancien, **pas** un des commits créés ci-dessus). Le hash ne doit jamais apparaître dans `git log --oneline build.ps1` parmi les commits récents.

Alternative :

```bash
git diff master --stat -- build.ps1
```

Attendu : aucune sortie (pas de changement).

- [ ] **Step 2: Smoke test complet**

Brancher le board ESP32-S3-Knob. Puis :

```bash
./build.sh --list-devices
./build.sh knob Basic_Blink --clean --upload --monitor
```

Attendu :
- `--list-devices` liste `amoled` et `knob (4 projects)`.
- Build complet (clean + recompile), upload, `Upload OK`, monitor série démarre.
- Sur le hardware : l'écran du knob clignote (LCD on/off via backlight).

Ctrl-C pour quitter le monitor.

- [ ] **Step 3: Test mode `--flash` (sans rebuild)**

```bash
./build.sh knob Basic_Blink --flash
```

Attendu : flash sans recompile, `Upload OK`.

- [ ] **Step 4: Vérifier que `git status` est propre**

```bash
git status
```

Attendu : `nothing to commit, working tree clean`.

`.claude/settings.local.json` ne doit PAS apparaître (gitignored).

- [ ] **Step 5: Vérifier le contenu du `.gitignore`**

```bash
grep "settings.local.json" .gitignore
```

Attendu : `.claude/settings.local.json`.

- [ ] **Step 6: Aucun commit nécessaire** — c'est de la validation.

---

## Critères de succès (rappel de la spec)

1. ✅ Sur macOS : `./build.sh knob Basic_Blink` build sans erreur.
2. ✅ Sur macOS : `./build.sh knob Basic_Blink --upload` détecte le port et flashe.
3. ✅ Sur Windows : `.\build.ps1 knob Basic_Blink -Upload` continue à fonctionner (intact).
4. ✅ `.claude/settings.local.json` n'apparaît plus dans `git status`.
5. ✅ README mentionne explicitement les deux flavors.

---

## Self-review (run by plan author)

**Spec coverage** :
- Décision 1 (deux scripts côte à côte) → Task 4-7 (build.sh) + Task 9 (vérif PS1 intact). ✅
- Décision 2 (bash 3.2 compat) → noté dans le code (`PROJECTS+=`, pas d'assoc arrays), shebang `#!/usr/bin/env bash`. ✅
- Décision 3 (parité CLI) → flags POSIX `--upload`, etc. Task 4 step 3-8. ✅
- Décision 4 (split permissions) → Task 1. ✅
- Composant 1 (build.sh) → Tasks 4-7. ✅
- Composant 2 (README) → Task 8. ✅
- Composant 3 (docs/install/macos.md) → Task 2. ✅
- Composant 4 (docs/install/windows.md) → Task 2. ✅
- Composant 5 (settings split) → Task 1 + Task 3 step 5. ✅
- Migration (`git rm --cached`, `.gitignore`) → Task 1 steps 2-3. ✅
- Critère "PIO installé sur macOS" → Task 3. ✅

**Placeholder scan** : aucun TBD / TODO / "implement later". Tous les code blocks contiennent du code complet, pas de "..." de continuation. ✅

**Type consistency** : `detect_port` (Task 5) et `run_upload` (Task 6) sont définies une seule fois et appelées de manière cohérente. `PIO`, `PYTHON_BIN`, `ESPTOOL` sont définis avant usage. ✅

**Cohérence Task 5/6/7** : la build loop est définie en Task 4 (minimal), réécrite en Task 6 (avec upload), réécrite en Task 7 (avec flash). Chaque réécriture est complète (pas d'ajout incrémental ambigu). ✅
