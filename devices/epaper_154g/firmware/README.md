# Restauration du firmware d'usine — ESP32-S3-ePaper-1.54G

Procédure pour remettre la carte dans son état d'origine après avoir flashé un de nos projets.

## Fichier

| Champ | Valeur |
|---|---|
| Nom | `ESP32-S3-ePaper-1.54G.bin` |
| Taille | 3 805 648 octets (~3,6 MB) |
| Format | Image **merged** (bootloader + partitions + app) à flasher à l'offset `0x0` — vérifié : magic `0xE9` à `0x0` + table de partitions (`0xAA50`, entrées nvs/otadata/phy) à `0x8000` |
| Source | GitHub vendor [waveshareteam/ESP32-S3-ePaper-1.54G](https://github.com/waveshareteam/ESP32-S3-ePaper-1.54G), dossier `Firmware/` (clone du 2026-06-09) |
| Version | Build **XiaoZhi v2.0.1** (projet `xiaozhi`, compilé 2026-05-13 — lu dans les logs de boot), mode « PhotoPainter » |

## Pré-requis

PlatformIO Core installé (voir [docs/install/macos.md](../../../docs/install/macos.md) ou [windows.md](../../../docs/install/windows.md)) — on utilise le `python` + `esptool.py` embarqués dans `~/.platformio/penv/`.

## Commande

### macOS / Linux

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
    write_flash 0x0 devices/epaper_154g/firmware/ESP32-S3-ePaper-1.54G.bin
```

### Windows (PowerShell)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM<n> --baud 921600 `
    write_flash 0x0 devices\epaper_154g\firmware\ESP32-S3-ePaper-1.54G.bin
```

## Notes

- **Une carte TF (FAT32) est requise** : sans SD montable, l'init du firmware échoue (`sdcard_bsp` timeout `0x107` → `init Failure`) et **rien ne s'affiche** — l'écran e-paper reste figé sur son image précédente. Observé sur les logs série le 2026-06-09.
- **La carte doit contenir le contenu SD vendor** : copier le contenu de [`../docs/demo-code/XiaoZhi/02 SDCARD/`](../docs/demo-code/XiaoZhi/) à la **racine** de la carte (`bmp/` avec `config.txt` + BMP 200×200, `index.txt`, `fileList.txt`, `02_sys_ap_img/`, `03_sys_ap_html/`). Sinon : `sdscan: Failed to open directory: /sdcard/bmp` → écran toujours vide (observé 2026-06-09). `index.txt` semble piloter l'image courante ; `bmp/config.txt` contient la config PhotoPainter (timer + endpoint IA vendor).
- Le firmware usine (comme nos projets avec `-DARDUINO_USB_CDC_ON_BOOT=1`) expose un port série CDC natif : l'auto-reset esptool fonctionne dans les deux sens, aucune séquence BOOT manuelle n'est nécessaire.
- Pour re-flasher un de nos projets ensuite : `./build.sh epaper_154g Basic_Blink --upload`.
