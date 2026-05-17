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

## Format et offset — à valider

> **Non documenté par Waveshare dans l'archive.** L'hypothèse retenue est qu'il s'agit d'une **image merged** (bootloader + partitions + app + assets) à flasher à l'offset `0x0`, par analogie avec le firmware Guition de format identique (`devices/guition_knob/firmware/JC3636K718_V1.1.bin`, 12 MB, merged @ 0x0). À confirmer par observation au premier flash — si la carte ne boote pas, essayer offset `0x10000` (app-only) avant tout autre diagnostic.

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

## État de la carte avant flash

Non observé sur cette carte à ce stade — la procédure manuelle BOOT + RESET pour forcer le ROM bootloader reste l'option fiable si l'auto-reset esptool échoue.

## Après flash

Non observé. La démo Waveshare « XiaoZhi AI » est censée démarrer (écran + voice chat + tous les périphériques on-board exercés).

## Pour re-flasher l'un de nos projets ensuite

```bash
./build.sh amoled <projet> --upload
```
