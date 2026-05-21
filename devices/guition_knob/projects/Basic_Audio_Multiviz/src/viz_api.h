#pragma once

#include <stdint.h>
#include <stddef.h>

// Données audio passées à chaque viz à chaque tour de loop.
// Pointeurs valables uniquement le temps du render() courant.
struct AudioFrame {
    const int16_t* wave;       // 512 samples PCM bruts (post DC removal)
    const double*  fft_mag;    // 256 bins de magnitude FFT
    const float*   bands;      // 13 bandes log normalisées [0..1] (post peak-meter)
    float          rms;        // RMS normalisé [0..1]
    bool           transient;  // true si onset détecté ce tick
    uint32_t       t_ms;       // millis() au tick courant
};

// Une visualisation = init/render/deinit + nom.
struct Visualizer {
    const char* name;
    void (*init)();
    void (*render)(const AudioFrame&);
    void (*deinit)();
};
