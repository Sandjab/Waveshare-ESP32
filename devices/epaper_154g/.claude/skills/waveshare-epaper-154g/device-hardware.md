# ESP32-S3-ePaper-1.54G — Hardware

## Architecture

- **SoC** : ESP32-S3-PICO-1 (LGA56, rev v0.2 sur notre exemplaire) — flash 8 MB (GD, QIO) et PSRAM 8 MB (AP, mode octal/OPI) intégrées au package. Vérifié par `esptool flash_id` ; le sdkconfig vendor confirme `CONFIG_SPIRAM_MODE_OCT` + `CONFIG_ESPTOOLPY_FLASHMODE_QIO`.
- **Charge batterie** : ETA6098 ; tension VBAT lue sur GPIO 4 (ADC1_CH3, diviseur, atten 12 dB).
- **Maintien d'alim** : GPIO 17 `VBAT_PWR` HIGH pour tenir le rail batterie (power latch) ; bouton PWR sur GPIO 18.

## E-Paper

- Panel 1.54" 200×200, 4 couleurs (noir/blanc/jaune/rouge), 2 bits/pixel.
- Contrôleur non nommé dans le code vendor — driver `EPD_1in54g` (`DEV_Config.cpp` + `EPD_1in54g.cpp` + `GUI_Paint.cpp` dans `docs/demo-code/Arduino/examples/08_E_paper_test/`).
- SPI2 : DC 10, CS 11, SCK 12, MOSI 13, RST 9, BUSY 8. Alim panel via GPIO 6 (**LOW = on**).
- Full refresh ~20 s, fast refresh ~15 s. Le BUSY pin reste haut pendant le refresh.
- Côté ESP-IDF, `09_E_Paper_Test` montre une intégration LVGL (rendu en RAM puis push e-paper).

## Chaîne audio

- ES8311 codec (I2C 0x18 sur SDA 47 / SCL 48) en I2S : MCLK 14, BCLK 15, LRCK 38, DOUT 45 (playback), DIN 16 (micro).
- Sortie speaker : rail audio GPIO 42 **LOW** + ampli GPIO 46 (`PA_CTRL`) **HIGH**. Demo vendor : 24 kHz, 16 bits mono, MCLK = 256×Fs.
- Connecteur speaker MX1.25 2 pins.

## Capteurs / RTC

- SHTC3 (I2C 0x70) : température + humidité — démo `03_I2C_SHTC3`.
- PCF85063 (I2C 0x51) : RTC — démos `02_I2C_PCF85063` et `11_RTC_Sleep_Test` (réveil deep-sleep via RTC + bouton BOOT GPIO 0 en ext wakeup).

## Stockage

- TF card en SDMMC **1-bit** : CLK 39, CMD 41, D0 40 — démo `04_SD_Card`.

## Divers

- LED verte GPIO 3, active LOW.
- UART0 : TX 43 / RX 44.
- 2× headers femelles 6 pins 2.54 mm pour extension (UART/I2C/GPIO réservés).
- Schéma : `docs/schematics/ESP32-S3-Touch-ePaper-1.54-Schematic.pdf` — seul PDF fourni par le repo vendor du 1.54G ; nommé d'après la variante Touch (PCB vraisemblablement partagé, non vérifié).
