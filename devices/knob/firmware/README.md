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

## Format et offset

`WX-ESP32S3-KNOB_V1.2.bin` est en réalité une **image merged complète** à flasher à **`0x0`** (validé en session 2026-05-17 par lecture flash + analyse du fichier source). Le binaire contient :

| Offset dans le fichier | Contenu | Signature |
|---|---|---|
| `0x0` | Bootloader 2nd-stage ESP-IDF | header `e9 04 02 4f`, entry `0x403c98ac`, chaînes `"load partition table error!"`, `"Factory app partition"` |
| `0x8000` | Table de partitions vendor | magic `aa 50`, layout : `nvs / otadata / app0=3M @ 0x10000 / app1=3M @ 0x310000 / spiffs / coredump` |
| `0x10000` | App principale | header `e9 06 02 4f`, entry `0x4037f3dc` |

L'hypothèse initiale "app-only à `0x10000`" était fausse : flasher à `0x10000` écrit le bootloader vendor à la place de l'app → écran noir, l'ancien bootloader cherche une app valide et n'en trouve pas.

Table de partitions vendor différente de celle de nos projets PlatformIO (`app0` = 3 MB chez Waveshare vs 6.25 MB chez nous) — sans incidence, le merged.bin réinstalle bootloader + partitions + app de façon cohérente.

## Note sur l'ESP32 secondaire

D'après les notes accumulées sur ce device, l'ESP32 secondaire peut être en mode `sleep-forever` dans le firmware Waveshare d'origine — auquel cas reflasher uniquement le `.bin` ESP32-S3 principal suffit à restaurer la démo visible. Le `.bin` secondaire est archivé par sécurité ; à n'utiliser que si on a vraiment perdu le firmware de la seconde puce, et seulement après avoir identifié comment l'exposer en flash (USB-to-UART externe probablement requis — le pont USB de la carte ne pilote que l'ESP32-S3).

## Pré-requis

PlatformIO Core installé (voir [docs/install/macos.md](../../../docs/install/macos.md) ou [windows.md](../../../docs/install/windows.md)) — on utilise le `python` + `esptool.py` embarqués dans `~/.platformio/penv/`.

## Commande — ESP32-S3 principal uniquement (cas standard)

### macOS / Linux

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
    write_flash 0x0 devices/knob/firmware/WX-ESP32S3-KNOB_V1.2.bin
```

### Windows (PowerShell)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM<n> --baud 921600 `
    write_flash 0x0 devices\knob\firmware\WX-ESP32S3-KNOB_V1.2.bin
```

## Pour re-flasher l'un de nos projets ensuite

```bash
./build.sh knob <projet> --upload
```
