#pragma once

// UI tactile Rich_Audio (LVGL, écran rond 360×360).
// À appeler après guition_lvgl_init() (lv_init fait).
void ui_build();               // construit titre + statut + liste de sons + arc volume
void ui_set_volume(int step);  // met à jour l'arc + le libellé de volume
void ui_tick();                // rafraîchit le statut (Playing/Ready) — appelé depuis loop()
