# Waveshare ESP32-S3-Touch-AMOLED-1.8

<img src="docs/images/ESP32-S3-Touch-AMOLED-1.8.png" width="200" alt="ESP32-S3-Touch-AMOLED-1.8">

Dev repo pour le [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm) - montre connectee avec ecran AMOLED 368x448, touch, audio, IMU, RTC et PMIC.

## Projets

| Projet | Description | Peripheriques utilises |
|--------|-------------|------------------------|
| [Basic_Blink](projects/Basic_Blink/) | Ecran clignotant vert/rouge | Display QSPI (SH8601), brightness via cmd 0x51 |

D'autres exemples de reference (Arduino + ESP-IDF) sont dans `docs/demo-code/`.

## Structure

```
devices/amoled/
├── lib/
│   └── amoled_hw/               # Lib device-specific (pins, display init)
├── projects/
│   └── Basic_Blink/             # Ecran clignotant
├── firmware/                    # Firmware d'usine archive (FactoryXiaozhi_250805)
└── docs/
    ├── demo-code/
    │   ├── Arduino/             # 16 exemples Arduino
    │   └── ESP-IDF/             # 6 exemples ESP-IDF
    ├── schematics/              # Schemas
    └── product.pdf              # Datasheet produit
```

## Build et flash

Depuis la racine du monorepo :

```powershell
# Windows
.\build.ps1 amoled                                     # Build tous les projets AMOLED
.\build.ps1 amoled Basic_Blink                         # Build Basic_Blink seul
.\build.ps1 amoled Basic_Blink -Upload                 # Build + flash (auto-detect port)
.\build.ps1 amoled Basic_Blink -Upload -Port COM13 -Monitor
.\build.ps1 amoled -Clean                              # Clean + rebuild tout
```

```bash
# macOS / Linux
./build.sh amoled                                      # Build tous les projets AMOLED
./build.sh amoled Basic_Blink                          # Build Basic_Blink seul
./build.sh amoled Basic_Blink --upload                 # Build + flash (auto-detect port)
./build.sh amoled Basic_Blink --upload --port /dev/cu.usbmodem* --monitor
./build.sh amoled --clean                              # Clean + rebuild tout
```

## Peripheriques

| IC | Fonction | I2C Addr |
|----|----------|----------|
| SH8601 | AMOLED QSPI 368x448 | — |
| FT3168 | Touch capacitif | 0x38 |
| ES8311 | Codec audio (speaker + mic) | 0x18 |
| AXP2101 | PMIC (batterie, charging, ADC) | 0x34 |
| QMI8658 | IMU 6 axes (accel + gyro) | 0x6B |
| PCF85063 | RTC temps reel | 0x51 |
| XCA9554 | I/O expander 8-bit | 0x20 |
