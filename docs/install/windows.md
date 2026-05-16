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
