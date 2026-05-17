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

## Format et offset — à valider

> **Non documenté par Waveshare dans l'archive.** L'hypothèse retenue est qu'il s'agit d'une **image merged** (bootloader + partitions + app + assets) à flasher à l'offset `0x0`, par analogie avec :
> - le firmware AMOLED 1.8 (`FactoryXiaozhi_250805.bin`, 16 MB merged @ 0x0, hypothèse non encore vérifiée),
> - et le firmware Guition (`JC3636K718_V1.1.bin`, 12 MB merged @ 0x0, **observé OK**).
> À confirmer par observation au premier flash — si la carte ne boote pas, essayer `0x10000` (app-only).

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

## État de la carte avant flash

Non observé sur cette carte à ce stade.

## Après flash

Non observé.

## Pour re-flasher l'un de nos projets ensuite

```bash
./build.sh amoled_175c <projet> --upload
```
