# Waveshare ESP32-S3-Touch-AMOLED-1.75C

Dev repo pour le [Waveshare ESP32-S3-Touch-AMOLED-1.75C](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C) — montre ronde 466×466 (CO5300 QSPI), touch, audio (ES8311 + ES7210 AEC), PMIC AXP2101, IMU, dual-microphone array, speaker onboard.

## Projets

Aucun projet pour l'instant. Les exemples de référence vendor sont sur GitHub : [waveshareteam/ESP32-S3-Touch-AMOLED-1.75C](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/tree/main/examples) (Arduino + ESP-IDF).

## Structure

```
devices/amoled_175c/
├── lib/
│   └── amoled_175c_hw/             # Lib device-specific (pins ; display init à venir)
├── projects/                        # PlatformIO projects (à venir)
├── firmware/                        # Factory firmware archive
└── docs/
    ├── schematics/                  # PDF vendor
    ├── datasheets/                  # (à compléter)
    └── demo-code/                   # (à compléter)
```

## Build et flash

> Le premier projet n'existe pas encore. Une fois créé :

```powershell
# Windows
.\build.ps1 amoled_175c                                # Build tous les projets
.\build.ps1 amoled_175c <Project>                      # Build un projet seul
.\build.ps1 amoled_175c <Project> -Upload              # Build + flash (auto-detect port)
.\build.ps1 amoled_175c <Project> -Upload -Port COM13 -Monitor
.\build.ps1 amoled_175c -Clean
```

```bash
# macOS / Linux
./build.sh amoled_175c                                 # Build tous les projets
./build.sh amoled_175c <Project>                       # Build un projet seul
./build.sh amoled_175c <Project> --upload              # Build + flash (auto-detect port)
./build.sh amoled_175c <Project> --upload --port /dev/cu.usbmodem* --monitor
./build.sh amoled_175c --clean
```

## Périphériques

| IC | Fonction | Interface |
|----|----------|-----------|
| CO5300 | AMOLED QSPI 466×466 | QSPI |
| CST9217 | Touch capacitif | I2C (bus partagé) |
| ES8311 | Codec audio playback | I2S + I2C |
| ES7210 | AEC échantillonneur micro-array | I2S + I2C |
| AXP2101 | PMIC (batterie, charging, ADC) | I2C |
| QMI8658 | IMU 6 axes (accel + gyro) | I2C |
