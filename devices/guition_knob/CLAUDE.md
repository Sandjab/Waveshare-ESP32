# Guition JC3636K718 — Knob avec anneau RGB

Carte rotative 1.8" 360×360 (ST77916 QSPI) avec encoder, audio DAC PCM5100A, micro I2S, SD card, haptics DRV2605 + LRA, et **anneau 13 WS2812 (GRB) sur GPIO 0** — fonctionnellement très proche du Waveshare ESP32-S3-Knob-Touch-LCD-1.8, mais avec un pinout entièrement différent et l'anneau RGB en plus.

## Repo Structure

```
devices/guition_knob/
├── lib/guition_knob_hw/             # GPIO defs, display, LVGL, encoder
├── projects/
│   └── Basic_Blink/                 # Ecran clignotant (raw display)
└── docs/
    ├── datasheets/                  # ST77916 INI, PCM5100A pdf, ESP32-S3R8 pdf
    ├── demo-code/                   # Code demo Guition (Arduino + ESP-IDF, dont led_strip)
    ├── dimensions/                  # Photos cotes mécaniques
    ├── instructions/                # PDFs vendeur (utilisation, getting started)
    ├── schematics/                  # JC3636K718.pdf + JC3636K718_P.pdf
    └── images/                      # (à compléter)
```

Shared : `shared/lib/qspi_panel/` (driver `esp_lcd_sh8601` — déjà réutilisé par Knob).

## Différences clés vs Waveshare Knob

| Aspect | Waveshare Knob | Guition K718 |
|---|---|---|
| LCD CS / CLK / D0-D3 / RST / BL | 14 / 13 / 15-18 / 21 / 47 | 12 / 11 / 13-16 / 17 / 21 |
| Encoder A / B (driver convention) | 8 / 7 | **1 / 2** (silkscreen labels GPIO 2 = A and 1 = B, swapped in `guition_pins.h` — see Gotchas) |
| I2C SDA / SCL | 11 / 12 | **9 / 10** |
| Touch INT / RST | non exposés | 7 / 8 |
| SD CMD / CLK / D0-D3 | 3 / 4 / 5-6-42-2 | **38 / 39 / 40-41-48-47** |
| Audio I2S BCK / WS / DO / SpkEN | (n/a dans Basic_*) | 3 / 45 / 42 / 46 |
| Mic SCK / Data | (n/a) | 5 / 4 |
| Battery monitor | — | 6 (DAC) |
| **RGB ring data** | — | **0** (13 WS2812 GRB) |
| Haptic DRV2605 + LRA | présent (I2C, 0x5A) | présent (I2C 0x5A sur bus partagé avec touch SDA:9 SCL:10 — confirmé schéma `JC3636K718.pdf`. GPIOs `HAPTIC_TRIG` / `HAPTIC_EN` existent mais leur assignation n'est pas dans le pinconfig vendor — à valider) |

## Init ST77916

La séquence d'init est **identique** entre Waveshare et Guition (vérifié 181/185 entrées byte-identiques contre le `.INI` Guition). `guition_lcd_init.h` est une copie du `knob_lcd_init.h`. Si un jour on veut factoriser, candidat naturel pour `shared/lib/`.

## Gotchas

- **Encoder phases A/B swappées** : le silkscreen et le `pinconfig.h` vendor étiquettent GPIO 2 = A et GPIO 1 = B, mais avec ce mapping le driver `bidi_switch_knob` (convention A→`+1`, B→`-1`, calibrée sur le Waveshare Knob) compte à l'envers — CW fait diminuer. On swap dans [`lib/guition_knob_hw/guition_pins.h`](lib/guition_knob_hw/guition_pins.h) : `PIN_ENC_A = 1`, `PIN_ENC_B = 2`. Confirmé par test croisé avec le Waveshare Knob (`Basic_LVGL_Meter`) — 2026-05-17.
- **GPIO 0 dual role** : BOOT strap + RGB ring data. WS2812 idle = low ⇒ pas de conflit tant qu'on n'écrit pas avant la fin du boot.
- **GPIO 46 = enable de l'ampli speaker (NS4150B), pas mute du DAC** : PCM5100A `XSMT` est tiré sur 3V3 = always unmuted. Le speaker passe par un ampli mono Class D activé par GPIO46 (high = ampli on). Le jack 3.5mm (CN3, PJ-342) a un switch de détection qui ouvre l'entrée du NS4150 quand un casque est inséré → speaker coupé automatiquement, casque reçoit le line-out direct du DAC.
- **DRV2605 + LRA présents** (sur le bus I2C partagé avec touch, adresse 0x5A) bien que **non utilisés par la démo vendor** — j'ai initialement et à tort répété que le Guition n'avait pas d'haptics. Le port de `Hue_Encoder` Waveshare → Guition est donc à priori jouable sans retirer les appels haptics. Restera à valider : les pins `HAPTIC_TRIG` et `HAPTIC_EN` du schéma n'ont pas d'assignation GPIO dans le pinconfig vendor — soit elles sont laissées flottantes (auto-trigger via I2C uniquement, possible), soit elles sont câblées et il faudra les retrouver sur le schéma.
- **Mic = PDM (pas I2S Philips), et PDM RX exige `I2S_NUM_0` sur l'ESP32-S3** : le mic n'a que 2 broches (`PIN_MIC_SCK=5`, `PIN_MIC_DATA=4`, pas de WS) ⇒ interface PDM. Confirmé en runtime par `Basic_Audio_Visualizer` — 2026-05-20. Le mic est **non utilisé par toutes les démos vendor**, c'est la première démo qui le fait parler. Gotcha IDF associé : `i2s_channel_init_pdm_rx_mode()` retourne `ESP_ERR_INVALID_ARG` si le channel est créé sur `I2S_NUM_1` (« This channel handle is registered on I2S1, but PDM is only supported on I2S0 »). Toujours créer le channel mic avec `I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER)`.
- **PDM mic — offset DC sur le `wave_buf` brut** : les samples PDM sortis par `i2s_channel_read()` ont un offset DC non nul (constante près de zéro mais significative devant un signal faible). Dans `Basic_Audio_Multiviz`, `audio_pipeline.cpp` retire la moyenne **uniquement dans le buffer FFT** (`v_re = wave_buf - mean`) mais expose `wave_buf` tel quel via `af.wave`. Toute viz qui dessine `af.wave` directement (oscillo, lissajous, …) doit **soustraire la moyenne de la trame avant tout scaling** — sinon augmenter le gain décale la ligne de base au lieu d'étirer le signal. Voir `viz_oscillo.cpp:viz_render()` pour le pattern (compute mean / subtract / scale). Si un jour ce piège revient une 3e fois, candidat naturel pour faire la DC removal dans `audio_pipeline_tick()` (en place sur `wave_buf`) — mais c'est un changement de comportement pour TOUTES les visus existantes, à faire délibérément.
- **Pinout 100% différent du Knob Waveshare** malgré l'IC ST77916 commun.
- **Pas un Guition JC3636W518** non plus (notre repo en parlait à propos du Knob). K718 et W518 sont deux modèles Guition distincts avec des pinouts différents.

## First flash — mode download manuel obligatoire

À la sortie d'usine, la carte tourne le **firmware vendor Guition** qui expose un USB custom (`VID:PID=303A:4001` ou `4002`, "ESP USB DEVICE" / "N7 Workshop") — probablement HID pour piloter leur menu/écran. Il n'expose **pas** de port série CDC, donc :
- `pio device list` ne voit aucun port utilisable
- l'auto-detect de `build.sh`/`build.ps1` (qui cherche `VID:PID=303A:1001`) échoue
- esptool ne peut pas piloter d'auto-reset DTR/RTS

Solution : forcer le **ROM bootloader** de l'ESP32-S3 en mode download manuel :

1. Maintenir **BOOT** appuyé
2. Brancher le câble USB (ou appuyer brièvement sur **RESET** si déjà branché)
3. Relâcher BOOT

La carte ré-énumère alors en `VID:PID=303A:1001` ("USB JTAG_serial debug unit" / Espressif) avec un `/dev/cu.usbmodem*` (ou `COM*`) exposé. À partir de là, `./build.sh guition_knob <projet> --upload` (ou `.\build.ps1 ... -Upload`) trouve le port et flashe normalement.

Note : le firmware vendor a aussi un **mode USB Mass Storage** (monté en FAT32 503 MB sur `/Volumes/NO NAME` côté Mac, `COM*` MSC côté Windows), mais ce n'est **pas** le mode par défaut au boot — il faut l'activer explicitement via une entrée de menu (« reboot to MSC ») sur l'écran. Donc on ne tombe dessus que volontairement.

Une fois notre firmware en place (avec `-DARDUINO_USB_CDC_ON_BOOT=1` dans `platformio.ini`), le CDC est exposé en permanence et l'auto-reset esptool fonctionne — la procédure manuelle n'est plus nécessaire pour les flashs suivants.

### Restauration du firmware vendor

Le binaire d'usine `JC3636K718_V1.1.bin` est archivé dans [`firmware/`](firmware/). Procédure complète de re-flash (commande esptool, durée, état post-flash, etc.) : [`firmware/README.md`](firmware/README.md).

## LVGL Documentation

Comme pour le Knob : Context7 `/websites/lvgl_io_8_4` ou `/websites/lvgl_io_master`.
