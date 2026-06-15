#include "sound_ping.h"
#include "config.h"
#include <math.h>

static constexpr float    GAIN_MAX = 0.50f;
// Strike Note 880 Hz ≈ A5 -> ping cristallin. {freq, amp, tau}
static constexpr float P_FREQ[4] = {  880.0f, 1760.0f, 2429.0f, 4752.0f };
static constexpr float P_AMP [4] = {  1.00f,  0.50f,   0.30f,   0.15f   };
static constexpr float P_TAU [4] = {  0.50f,  0.30f,   0.20f,   0.10f   };

// Durée totale : le partiel le plus long (tau=0.5s) tombe à ~e^-3 (5%) après 1.5 s.
static constexpr uint32_t TOTAL_FRAMES = (uint32_t)(AUDIO_SAMPLE_RATE * 1500 / 1000);

void PingSound::trigger() {
    for (int i = 0; i < kPartials; i++) {
        partials_[i].freq        = P_FREQ[i];
        partials_[i].amp         = P_AMP[i];
        partials_[i].decay_tau_s = P_TAU[i];
        partials_[i].phase       = 0.0f;                 // attaque nette
        partials_[i].phase_inc   = 2.0f * (float)M_PI * P_FREQ[i] / AUDIO_SAMPLE_RATE;
    }
    pos_ = 0;
}

bool PingSound::render(int16_t* stereo, size_t frames) {
    // Enveloppes évaluées une fois par bloc (~5.8 ms) : variation < 7 % sur le
    // partiel le plus court, inaudible (cf. Basic_Audio_Ping).
    float env[kPartials];
    float t = (float)pos_ / (float)AUDIO_SAMPLE_RATE;
    for (int i = 0; i < kPartials; i++) {
        env[i] = expf(-t / partials_[i].decay_tau_s) * partials_[i].amp;
    }

    for (size_t i = 0; i < frames; i++) {
        float mix = 0.0f;
        for (int k = 0; k < kPartials; k++) {
            mix += env[k] * sinf(partials_[k].phase);
            partials_[k].phase += partials_[k].phase_inc;
            if (partials_[k].phase >= 2.0f * (float)M_PI) partials_[k].phase -= 2.0f * (float)M_PI;
        }
        mix *= GAIN_MAX;
        if (mix > 1.0f)  mix = 1.0f;
        if (mix < -1.0f) mix = -1.0f;
        int16_t s = (int16_t)(mix * 32767.0f);
        stereo[i * 2]     = s;
        stereo[i * 2 + 1] = s;
    }
    pos_ += frames;
    return pos_ < TOTAL_FRAMES;
}
