# Waveshare ESP32-S3-Touch-AMOLED-1.8

Dev repo pour le [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm) - montre connectee avec ecran AMOLED 368x448, touch, audio, IMU, RTC et PMIC.

## Projets

Aucun projet pour l'instant. Les exemples de reference sont dans `docs/demo-code/`.

## Structure

```
devices/amoled/
├── lib/
│   └── amoled_hw/               # Lib device-specific (pins)
├── projects/                    # PlatformIO projects (a venir)
└── docs/
    ├── demo-code/
    │   ├── Arduino/             # 16 exemples Arduino
    │   └── ESP-IDF/             # 6 exemples ESP-IDF
    ├── schematics/              # Schemas
    └── product.pdf              # Datasheet produit
```

## Build

Depuis la racine du monorepo :

```powershell
.\build.ps1 amoled                     # Build tous les projets AMOLED
.\build.ps1 amoled Test01 -Upload      # Build + flash
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
