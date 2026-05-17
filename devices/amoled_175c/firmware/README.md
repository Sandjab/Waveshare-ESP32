# Restauration du firmware d'usine — ESP32-S3-Touch-AMOLED-1.75C

Procédure pour remettre la carte dans son état d'origine — typiquement après avoir flashé un de nos projets.

## Fichier

| Champ | Valeur |
|---|---|
| Nom | `ESP32-S3-Touch-AMOLED-1.75C-FactoryOnly-260114.bin` |
| Taille | 33 488 896 octets (~31.94 MB) |
| SHA-256 | `46e65fb708f325f6830d6f14ecbba3d357dbdab6a939e13fdf2206e147c063d2` |
| Source | [`waveshareteam/ESP32-S3-Touch-AMOLED-1.75C`](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/tree/main/Firmware) — dépôt officiel Waveshare |
| Version | Suffixe `260114` (probablement 2026-01-14 d'après le pattern de nommage Waveshare) |

## Format et offset

Image **merged** complète à flasher à **`0x0`** (validé en session 2026-05-17 sur `amoled175Silver`). Contenu vérifié :

| Offset dans le fichier | Contenu |
|---|---|
| `0x0` | Bootloader 2nd-stage ESP-IDF v5.5.2 (entry `0x403c8930`) |
| `0x8000` | Table de partitions vendor |
| `0x110000` | App `factory` (9 MB) |
| `0x11F0000` + `0x1AF0000` | Assets + storage (2 partitions spiffs, 9 MB + 5 MB) |

Table de partitions vendor : `nvsfactory` (200K @ 0x9000) — `nvs` (840K @ 0x3B000) — `otadata` (8K @ 0x10D000) — `phy_init` (4K @ 0x10F000) — `factory` (9M @ 0x110000) — `ota_0` (4032K @ 0xA10000) — `ota_1` (4032K @ 0xE00000) — `assets` spiffs (9M @ 0x11F0000) — `storage` spiffs (5M @ 0x1AF0000). Total ≈ 32 MB cohérent avec la flash du device.

## Pré-requis

PlatformIO Core installé (voir [docs/install/macos.md](../../../docs/install/macos.md) ou [windows.md](../../../docs/install/windows.md)) — on utilise le `python` + `esptool.py` embarqués dans `~/.platformio/penv/`.

## Commande

### macOS / Linux

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
    write_flash 0x0 devices/amoled_175c/firmware/ESP32-S3-Touch-AMOLED-1.75C-FactoryOnly-260114.bin
```

### Windows (PowerShell)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM<n> --baud 921600 `
    write_flash 0x0 devices\amoled_175c\firmware\ESP32-S3-Touch-AMOLED-1.75C-FactoryOnly-260114.bin
```

## Après flash

Validé 2026-05-17 sur `amoled175Silver` : auto-reset esptool OK, démo factory démarre.

## Pour re-flasher l'un de nos projets ensuite

```bash
./build.sh amoled_175c <projet> --upload
```
