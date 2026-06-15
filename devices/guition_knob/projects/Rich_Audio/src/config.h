#pragma once

// Rich_Audio — banc d'essai de sons sur le Guition JC3636K718.
// Paramètres partagés entre le moteur audio, les générateurs et l'UI.

// --- Chaîne audio I2S (PCM5100A DAC -> NS4150B ampli mono) ---
#define AUDIO_SAMPLE_RATE   44100u
#define AUDIO_FRAMES_PER_BUF 256    // taille de bloc rendu/écrit en I2S (~5.8 ms)

// --- Volume maître (entier, piloté par l'encodeur ; appliqué dans le moteur) ---
// 0 = silence, AUDIO_VOL_STEPS = pleine échelle. Chaque générateur intègre déjà
// son propre headroom (~0.5 pleine échelle) ; le volume n'est qu'une fraction.
#define AUDIO_VOL_STEPS     20
#define AUDIO_VOL_DEFAULT   4       // ~doux au boot (cohérent avec Basic_Audio_*)
