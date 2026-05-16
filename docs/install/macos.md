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
