#pragma once

#include "viz_api.h"

// Initialise le mic PDM (I2S_NUM_0), prépare la FFT et le layout des 13 bandes.
// À appeler une fois dans setup() avant audio_pipeline_tick().
void audio_pipeline_init();

// Bloque ~32 ms pour capturer 512 samples @ 16 kHz, calcule FFT + bandes + RMS
// + transient detect. Retourne une AudioFrame valide jusqu'au prochain appel.
const AudioFrame& audio_pipeline_tick();
