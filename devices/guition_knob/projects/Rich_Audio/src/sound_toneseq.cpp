#include "sound_toneseq.h"
#include "config.h"
#include <math.h>

static constexpr float    GAIN_MAX   = 0.40f;
static constexpr uint32_t ATTACK_FR  = (uint32_t)(AUDIO_SAMPLE_RATE * 3 / 1000);   // 3 ms
static constexpr uint32_t RELEASE_FR = (uint32_t)(AUDIO_SAMPLE_RATE * 6 / 1000);   // 6 ms

static inline uint32_t ms_to_frames(uint16_t ms) {
    return (uint32_t)((uint32_t)ms * AUDIO_SAMPLE_RATE / 1000);
}

static inline float osc(ToneWave w, float phase) {
    switch (w) {
        case TW_SQUARE: return phase < (float)M_PI ? 1.0f : -1.0f;
        case TW_TRI: {
            float x = phase / (float)M_PI;                 // [0,2)
            return (x < 1.0f) ? (2.0f * x - 1.0f) : (3.0f - 2.0f * x);
        }
        default: return sinf(phase);                       // TW_SINE
    }
}

void ToneSeqSound::start_note(int i) {
    note_     = i;
    note_pos_ = 0;
    note_len_ = ms_to_frames(def_->notes[i].dur_ms);
    if (note_len_ == 0) note_len_ = 1;
    phase_    = 0.0f;                                       // reset par note (déclické par l'attaque)
}

void ToneSeqSound::trigger() {
    done_ = false;
    start_note(0);
}

bool ToneSeqSound::render(int16_t* stereo, size_t frames) {
    for (size_t k = 0; k < frames; k++) {
        float s = 0.0f;
        if (!done_) {
            const ToneNote& n = def_->notes[note_];
            if (n.freq > 0.0f) {
                // attaque (ramp) * corps (pluck=decay / pad=sustain) * release (déclick fin de note)
                float atk = (note_pos_ < ATTACK_FR) ? (float)note_pos_ / (float)ATTACK_FR : 1.0f;
                float body = 1.0f;
                if (def_->env == TE_PLUCK) {
                    float t   = (float)note_pos_ / (float)AUDIO_SAMPLE_RATE;
                    float tau = (float)note_len_ / (float)AUDIO_SAMPLE_RATE * 0.4f;
                    body = expf(-t / tau);
                }
                uint32_t rem = (note_len_ > note_pos_) ? (note_len_ - note_pos_) : 0;
                float rel = (rem < RELEASE_FR) ? (float)rem / (float)RELEASE_FR : 1.0f;

                s = osc(def_->wave, phase_) * atk * body * rel * def_->gain * GAIN_MAX;
                phase_ += 2.0f * (float)M_PI * n.freq / (float)AUDIO_SAMPLE_RATE;
                if (phase_ >= 2.0f * (float)M_PI) phase_ -= 2.0f * (float)M_PI;
            }
            if (++note_pos_ >= note_len_) {
                if (note_ + 1 < def_->count) start_note(note_ + 1);
                else done_ = true;
            }
        }
        if (s > 1.0f)  s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t v = (int16_t)(s * 32767.0f);
        stereo[k * 2]     = v;
        stereo[k * 2 + 1] = v;
    }
    return !done_;
}
