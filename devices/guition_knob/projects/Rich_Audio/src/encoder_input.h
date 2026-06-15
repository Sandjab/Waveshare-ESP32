#pragma once

// Encodeur rotatif -> volume maître. L'encodeur du Guition n'a PAS de bouton-poussoir
// (cf. device-hardware.md), donc rotation = volume, point. Le déclenchement des sons
// se fait au tactile (voir ui.cpp).
void encoder_begin();
void encoder_tick();   // appelé depuis loop() ; applique le delta et rafraîchit l'UI
