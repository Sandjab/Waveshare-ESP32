# Restauration du firmware d'usine — ESP32-S3-Knob-Touch-LCD-1.8

Procédure pour remettre la carte dans son état d'origine (démo Waveshare : encoder + LVGL + audio + haptics, etc.) — typiquement après avoir flashé un de nos projets.

## Fichiers

Le firmware d'usine est livré en **deux binaires séparés** car la carte porte deux MCU (cf. [`CLAUDE.md`](../CLAUDE.md) — dual-MCU architecture) :

| Cible | Nom | Taille | SHA-256 | mtime |
|---|---|---|---|---|
| ESP32-S3 (principal) | `WX-ESP32S3-KNOB_V1.2.bin` | 2 138 224 octets (~2.04 MB) | `f7c1cc18b687559f3bd69e5c9ab526bc61c2b2d9c502f38367f7f2bfe4ff8e87` | 2025-06-09 |
| ESP32 (secondaire) | `ESP32-KNOB_ESP32_0.bin` | 1 130 672 octets (~1.08 MB) | `0c1c21b9822d4c2d80d58534b33eb0083880de4ed7354a38b4c78ba51757349d` | 2025-05-27 |

## Source

Extraits de l'archive officielle [`ESP32-S3-Knob-Touch-LCD-1.8-Demo.zip`](https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-Demo.zip) (Waveshare wiki, ~65.5 MB, daté 2025-06-20), dossier `ESP32-S3-Knob-Touch-LCD-1.8-Demo/Firmware/`. Le pré-fixe `WX` correspond probablement à « Waveshare » ; `V1.2` est la version applicative.

## Format et offset — à valider

> **Non documenté par Waveshare dans l'archive** (pas de README de flash dans le ZIP démo). Vu la taille, ces binaires sont vraisemblablement des images **app-only**, pas des images merged. L'hypothèse retenue est l'offset standard ESP-IDF pour la partition app = **`0x10000`**.
>
> Le firmware vendor ne contient probablement **pas** de bootloader / table de partitions de remplacement — la carte garde donc le bootloader + table actuels (de notre projet, le cas échéant). Si tu veux un état strictement « as-shipped », il faudrait aussi flasher bootloader + partitions, qui ne sont pas distribués séparément.
>
> À confirmer par observation au premier flash.

## Note sur l'ESP32 secondaire

D'après les notes accumulées sur ce device, l'ESP32 secondaire peut être en mode `sleep-forever` dans le firmware Waveshare d'origine — auquel cas reflasher uniquement le `.bin` ESP32-S3 principal suffit à restaurer la démo visible. Le `.bin` secondaire est archivé par sécurité ; à n'utiliser que si on a vraiment perdu le firmware de la seconde puce, et seulement après avoir identifié comment l'exposer en flash (USB-to-UART externe probablement requis — le pont USB de la carte ne pilote que l'ESP32-S3).

## Pré-requis

PlatformIO Core installé (voir [docs/install/macos.md](../../../docs/install/macos.md) ou [windows.md](../../../docs/install/windows.md)) — on utilise le `python` + `esptool.py` embarqués dans `~/.platformio/penv/`.

## Commande — ESP32-S3 principal uniquement (cas standard)

### macOS / Linux

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
    write_flash 0x10000 devices/knob/firmware/WX-ESP32S3-KNOB_V1.2.bin
```

### Windows (PowerShell)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM<n> --baud 921600 `
    write_flash 0x10000 devices\knob\firmware\WX-ESP32S3-KNOB_V1.2.bin
```

## Pour re-flasher l'un de nos projets ensuite

```bash
./build.sh knob <projet> --upload
```
