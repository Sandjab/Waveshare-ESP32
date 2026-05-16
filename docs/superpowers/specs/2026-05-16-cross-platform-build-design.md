# Cross-platform Build Support (Windows + macOS)

**Date** : 2026-05-16
**Auteur** : Jean-Paul Gavini (avec Claude)
**Statut** : Design — en attente d'implémentation

## Contexte

Le repo `Waveshare-ESP32` a été construit sous Windows. Tout l'outillage (PlatformIO, scripts) suppose Windows : `build.ps1` (PowerShell), chemins `~/.platformio/penv/Scripts/pio.exe`, détection port via WMI (`Get-CimInstance Win32_PnPEntity`), permissions `.claude/settings.local.json` avec chemins absolus `D:/DEV/...`.

L'objectif est de permettre l'usage **indifférent** du repo sous Windows et macOS, **sans casser** ce qui fonctionne sur Windows.

## Inventaire des Windows-ismes

| Item | Windows-isme | Action |
|---|---|---|
| `build.ps1` | Chemin PIO Windows, détection WMI, mode `-Flash` avec `python.exe`/`esptool.py` Windows | **Inchangé** |
| `README.md` | Ne documente que `.\build.ps1` | À mettre à jour (deux sections) |
| `.claude/settings.local.json` | Permissions hardcodées `D:/DEV/...`, `Scripts/pio.exe`, `powershell.exe:*` | À splitter (tracké + local) |
| `platformio.ini` (tous) | Chemins relatifs, déjà cross-platform | OK |
| `shared/lib/`, code C/C++ | Universel | OK |
| Install PIO sur macOS | `pio` absent | Documenter |

## Décisions

1. **Stratégie de scripts** : `build.ps1` (intact) + `build.sh` (nouveau) côte à côte. Pas de dispatcher. Justification : zéro risque de régression Windows, chaque script idiomatique, pas de dépendance ajoutée (Python, just…).
2. **Shell pour `build.sh`** : `bash` (shebang `#!/usr/bin/env bash`), compatible bash 3.2 (macOS natif). Pas de feature bash 4+ (associative arrays, etc.).
3. **Parité CLI** : mêmes positional args (`Device`, `Projects…`) et mêmes noms de flags traduits aux conventions de chaque shell :
   - PowerShell : `-Upload`, `-Flash`, `-Port COM3`, `-Monitor`, `-Clean`, `-ListDevices`
   - bash : `--upload`, `--flash`, `--port /dev/cu.usbmodem*`, `--monitor`, `--clean`, `--list-devices`
4. **Settings Claude Code** : split standard
   - `.claude/settings.json` (tracké) : permissions universelles partagées
   - `.claude/settings.local.json` (gitignoré) : permissions OS/utilisateur

## Architecture cible

```
Waveshare-ESP32/
├── build.ps1                  # INCHANGÉ
├── build.sh                   # NOUVEAU
├── README.md                  # MIS À JOUR
├── .gitignore                 # MIS À JOUR (+ .claude/settings.local.json)
├── docs/
│   ├── install/
│   │   ├── windows.md         # NOUVEAU
│   │   └── macos.md           # NOUVEAU
│   └── superpowers/specs/
│       └── 2026-05-16-cross-platform-build-design.md  # CE FICHIER
└── .claude/
    ├── settings.json          # NOUVEAU (tracké)
    └── settings.local.json    # GITIGNORÉ (était tracké)
```

## Composants

### 1. `build.sh` — script de build macOS / Linux

**Responsabilités** (parité 1:1 avec `build.ps1`) :
- Lister les devices : `--list-devices`
- Build un ou plusieurs projets d'un device
- Upload (`--upload`) avec auto-detect port ou `--port`
- Flash sans rebuild (`--flash`)
- Monitor série (`--monitor`)
- Clean (`--clean`)

**Résolution de `pio`** (ordre de priorité) :
1. `pio` dans `$PATH`
2. `~/.platformio/penv/bin/pio` (cohérent avec Windows qui utilise `Scripts/pio.exe`)
3. Sinon → erreur explicite avec lien vers `docs/install/macos.md`

**Auto-detect port USB** : `pio device list --json-output` + parsing `python3` (natif macOS depuis 10.15).

```bash
port=$(pio device list --json-output | python3 -c '
import json, sys
for d in json.load(sys.stdin):
    hwid = d.get("hwid", "")
    if "VID:PID=303A:1001" in hwid:
        print(d["port"]); break
    if "VID:PID=1A86:7523" in hwid:  # CH340 fallback
        print(d["port"]); break
')
```

Port macOS attendu : `/dev/cu.usbmodem*` (ESP32-S3 natif) ou `/dev/cu.wchusbserial*` (CH340).

**Mode `--flash`** : appel direct esptool, chemins POSIX :
- Python : `~/.platformio/penv/bin/python`
- esptool : `~/.platformio/packages/tool-esptoolpy/esptool.py`
- Args identiques au PS1

**Détection succès upload** ("Hard resetting" → flash OK, ignore le bruit post-reset) : utiliser process substitution pour éviter le subshell scoping issue de `cmd | while` :

```bash
flash_ok=0
while IFS= read -r line; do
    if [ "$flash_ok" = "1" ]; then continue; fi
    printf '%s\n' "$line"
    case "$line" in *"Hard resetting"*) flash_ok=1 ;; esac
done < <("$pio" run -d "$dir" -t upload --upload-port "$port" 2>&1)
```

**Couleurs** : codes ANSI bruts (`\033[36m` etc.), pas de dépendance.

### 2. `README.md` — section Build à deux flavors

Structure :

```markdown
## Build

Prérequis : PlatformIO Core
- Windows → docs/install/windows.md
- macOS → docs/install/macos.md

### Windows (PowerShell)
.\build.ps1 knob Basic_Blink
.\build.ps1 knob Basic_Blink -Upload -Monitor
.\build.ps1 -ListDevices

### macOS / Linux (bash)
./build.sh knob Basic_Blink
./build.sh knob Basic_Blink --upload --monitor
./build.sh --list-devices
```

### 3. `docs/install/macos.md`

Install PIO via script officiel (cohérent avec Windows : pose PIO dans `~/.platformio/penv/`) :

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o /tmp/get-platformio.py
python3 /tmp/get-platformio.py
echo 'export PATH="$HOME/.platformio/penv/bin:$PATH"' >> ~/.zshrc
```

Note : aucune gestion de groupe `dialout` requise sur macOS (contrairement à Linux).

### 4. `docs/install/windows.md`

Extrait condensé : install PIO Core (`pip install --user platformio` ou via VS Code extension), drivers USB CP210x/CH340 si nécessaire pour le CH340 fallback.

### 5. Split des permissions Claude Code

**`.claude/settings.json`** (tracké, universel) :
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

**`.claude/settings.local.json`** (gitignoré, exemple macOS) :
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

**`.claude/settings.local.json`** (gitignoré, exemple Windows à recréer côté Windows) :
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

**Migration** :
```bash
git rm --cached .claude/settings.local.json
echo ".claude/settings.local.json" >> .gitignore
# Créer .claude/settings.json avec le commun
# Le user (sur chaque OS) recrée son settings.local.json
```

Le `settings.local.json` actuel reste sur le disque local (pas supprimé), juste détaché de git. Sur macOS, le user peut soit le remplacer par le contenu macOS ci-dessus, soit le laisser tel quel (les entrées Windows ne matcheront rien sur macOS — inoffensif mais bruyant).

## Flux d'utilisation

### Sur macOS (état cible)
```bash
# Install one-shot (voir docs/install/macos.md)
python3 /tmp/get-platformio.py

# Usage quotidien
./build.sh knob Basic_Blink --upload --monitor
```

### Sur Windows (inchangé)
```powershell
.\build.ps1 knob Basic_Blink -Upload -Monitor
```

## Non-objectifs

- **Linux** : non couvert explicitement. `build.sh` devrait largement fonctionner mais aucune validation. Pas d'effort dédié.
- **Réécriture en Python** : rejeté pour éviter une dépendance Python système et le risque sur Windows.
- **Outil de tâches externe** (just, make) : rejeté pour éviter une dépendance.
- **Refactoring de `build.ps1`** : intact. Aucune modification.

## Risques et mitigations

| Risque | Mitigation |
|---|---|
| Divergence comportementale entre `build.ps1` et `build.sh` | Spec de parité explicite ; CLI mirror ; tests manuels sur un device par OS |
| `python3` absent sur macOS pré-10.15 | Pré-requis listé dans `docs/install/macos.md` ; erreur explicite |
| Process substitution `< <(...)` non disponible sur shells non-bash | Shebang `#!/usr/bin/env bash` impose bash |
| Perte de l'historique des permissions utiles après `git rm --cached` | Le fichier reste localement ; le tracké `settings.json` capture le commun |
| Permissions Windows-only orphelines dans le repo | Toutes déplacées vers `settings.local.json` (gitignored) |

## Critères de succès

1. Sur macOS : `./build.sh knob Basic_Blink` build sans erreur (avec PIO installé).
2. Sur macOS : `./build.sh knob Basic_Blink --upload` détecte le port automatiquement et flashe.
3. Sur Windows : `.\build.ps1 knob Basic_Blink -Upload` continue de fonctionner **à l'identique**.
4. `.claude/settings.local.json` n'apparaît plus dans `git status` après modification locale.
5. README mentionne explicitement les deux flavors.

## Plan d'implémentation (à détailler dans writing-plans)

Ordre suggéré, chaque étape testable indépendamment :

1. Setup permissions split (`settings.json` tracké + `.gitignore`)
2. Création `docs/install/macos.md` + `docs/install/windows.md`
3. Installation PIO sur macOS (validation prérequis)
4. Création `build.sh` avec parité PS1
5. Mise à jour `README.md`
6. Validation manuelle : build d'un projet Knob sur macOS
