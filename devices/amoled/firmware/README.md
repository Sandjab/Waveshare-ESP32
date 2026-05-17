# Restauration du firmware d'usine — ESP32-S3-Touch-AMOLED-1.8

Procédure pour remettre la carte dans son état d'origine (démo « XiaoZhi AI », tous les périphériques on-board exercés) — typiquement après avoir flashé un de nos projets.

## Fichier

| Champ | Valeur |
|---|---|
| Nom | `ESP32-S3-Touch-AMOLED-1.8-FactoryXiaozhi_250805.bin` |
| Taille | 16 732 160 octets (~15.96 MB) |
| SHA-256 | `033ba27f0d1824835e90fe6b41d2db8c1f13cda7e1d80c82b3f7537dafb8dc8d` |
| Source | [`waveshareteam/ESP32-S3-Touch-AMOLED-1.8`](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8/tree/main/Firmware) — dépôt officiel Waveshare |
| Version | Tag `Xiaozhi_250805` (2025-08-05 d'après le suffixe daté du nom de fichier) |
| Sources d'origine | Fermées — le `README.txt` du dépôt vendor précise : « 出厂固件类源码公司均不对外开放 » / « factory firmware source code is not open to the public ». |

## Format et offset

Image **merged** complète à flasher à **`0x0`** (validé en session 2026-05-17 sur `amoled18Noir1`). Contenu vérifié :

| Offset dans le fichier | Contenu |
|---|---|
| `0x0` | Bootloader 2nd-stage ESP-IDF v5.5-beta1 (entry `0x403c8954`) |
| `0x8000` | Table de partitions vendor |
| `0x100000` | App `factory` XiaoZhi AI (4288K) |
| `0x12000` + `0xB30000` | Assets / modèles AI dans deux partitions spiffs (952K + 4884K) |

Table de partitions vendor : `nvs` (24K @ 0x9000) — `otadata` (8K @ 0xF000) — `phy_init` (4K @ 0x11000) — `model` spiffs (952K @ 0x12000) — `factory` (4288K @ 0x100000) — `ota_0` (6M @ 0x530000) — `storage` spiffs (4884K @ 0xB30000).

## Pré-requis

PlatformIO Core installé (voir [docs/install/macos.md](../../../docs/install/macos.md) ou [windows.md](../../../docs/install/windows.md)) — on utilise le `python` + `esptool.py` embarqués dans `~/.platformio/penv/`.

## Commande

### macOS / Linux

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
    write_flash 0x0 devices/amoled/firmware/ESP32-S3-Touch-AMOLED-1.8-FactoryXiaozhi_250805.bin
```

### Windows (PowerShell)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM<n> --baud 921600 `
    write_flash 0x0 devices\amoled\firmware\ESP32-S3-Touch-AMOLED-1.8-FactoryXiaozhi_250805.bin
```

## Après flash

Validé 2026-05-17 sur `amoled18Noir1` : auto-reset esptool OK, démo XiaoZhi AI démarre. La procédure manuelle BOOT + RESET reste l'option de secours si l'auto-reset échoue.

## Pour re-flasher l'un de nos projets ensuite

```bash
./build.sh amoled <projet> --upload
```
