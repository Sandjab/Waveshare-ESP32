# epaper_154g_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `epaper154g_pins.h` | GPIO definitions (e-paper SPI, rails de puissance, I2S/ES8311, I2C, SD, batterie, boutons, LED) |

## Notes

- Pas encore de driver e-paper porté ici. Le driver vendor (`EPD_1in54g` : `DEV_Config` + `EPD_1in54g` + `GUI_Paint`) vit dans `../../docs/demo-code/Arduino/examples/08_E_paper_test/` — à porter dans cette lib quand un premier projet display en aura besoin.
- **Logique inversée sur les rails** : `PIN_EPD_PWR` (6) et `PIN_AUDIO_PWR` (42) sont actifs **LOW** ; `PIN_VBAT_PWR` (17) est actif **HIGH** (cf. `board_power_bsp` vendor).
- LED verte GPIO 3 active **LOW**.
