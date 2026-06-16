#pragma once

// Moteur audio Rich_Audio. Possède l'I2S et une tâche FreeRTOS dédiée qui rend en
// continu le son actif (ou du silence). Découplé de LVGL pour éviter les underruns
// quand le flush SPI bloque la boucle principale.
//
// Modèle de threads :
//  - audio_play() / audio_set_volume() sont appelés depuis la boucle (cœur LVGL) et
//    ne font que poster une requête / écrire un entier -> aucun verrou nécessaire.
//  - La tâche audio est la SEULE à toucher les objets Sound (trigger + render).

void audio_begin();             // init I2S + lance la tâche audio
void audio_play(int sound_index); // (re)déclenche le son d'index donné dans le registre
void audio_set_volume(int step);  // 0..AUDIO_VOL_STEPS (clampé)
int  audio_get_volume();
bool audio_is_playing();          // true tant qu'un son est en cours (feedback UI)
