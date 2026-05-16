# Restauration du firmware d'usine — JC3636K718

Procédure pour remettre la carte dans son état d'origine (menu Guition, animations, audio, MSC opt-in via menu, etc.) — typiquement après avoir flashé un de nos projets.

## Fichier

| Champ | Valeur |
|---|---|
| Nom | `JC3636K718_V1.1.bin` |
| Taille | 12 324 864 octets (~11.75 MB) |
| Format | Image **merged** (bootloader + partitions + app + assets) à flasher à l'offset `0x0` |
| Source | Archive vendor Guition (`9-Burn/Burn operation instructions/` dans le ZIP livré) |
| Version | V1.1 (datée 30 janvier 2025 selon le `mtime` du fichier vendor) |

## Pré-requis

PlatformIO Core installé (voir [docs/install/macos.md](../../../../docs/install/macos.md) ou [windows.md](../../../../docs/install/windows.md)) — on utilise le `python` + `esptool.py` embarqués dans `~/.platformio/penv/`.

## État de la carte avant flash

Deux cas :

- **Notre firmware tourne** (Basic_Blink / Basic_RGB_Ring / etc. avec `-DARDUINO_USB_CDC_ON_BOOT=1`) → un `/dev/cu.usbmodem*` ou `COM*` est exposé, l'auto-reset esptool fonctionne → aller direct au flash.
- **Firmware vendor déjà en place** (cas peu fréquent : tu veux juste re-flasher pour rafraîchir) → pas de CDC exposé, il faut entrer en mode download manuel (BOOT + RESET, voir [CLAUDE.md](../../CLAUDE.md#first-flash--mode-download-manuel-obligatoire)) avant le flash.

## Commande

### macOS / Linux

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
    write_flash 0x0 devices/guition_knob/docs/firmware/JC3636K718_V1.1.bin
```

Remplacer `/dev/cu.usbmodem*` par le port réel si le shell ne l'expand pas, ou laisser esptool deviner avec `--port AUTO`.

### Windows (PowerShell)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" `
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
    --chip esp32s3 --port COM<n> --baud 921600 `
    write_flash 0x0 devices\guition_knob\docs\firmware\JC3636K718_V1.1.bin
```

## Durée

~60 s à 921 600 baud (12 MB compressés en ~2.3 MB, débit effectif ~1.6 Mbit/s).

## Après flash

- L'écran affiche le menu/animation Guition.
- Le device ré-énumère en `VID:PID=303A:4001` ou `4002` ("ESP USB DEVICE" / "N7 Workshop") — pas de CDC série.
- Le mode USB MSC (volume FAT32 503 MB monté en `/Volumes/NO NAME` côté Mac) n'est **pas** automatique : il faut l'activer via une entrée de menu (« reboot to MSC ») sur la carte.

## Pour re-flasher l'un de nos projets ensuite

Brancher la carte et entrer en mode download manuel (BOOT + RESET), puis :

```bash
./build.sh guition_knob Basic_Blink --upload
```

Une fois notre firmware en place, les flashs suivants ne nécessitent plus la séquence BOOT+RESET (le CDC reste actif).
