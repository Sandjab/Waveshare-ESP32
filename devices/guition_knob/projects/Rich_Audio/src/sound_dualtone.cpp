#include "sound_dualtone.h"
#include "config.h"
#include <math.h>

static constexpr float    GAIN_MAX   = 0.45f;
static constexpr uint32_t ATTACK_FR  = (uint32_t)(AUDIO_SAMPLE_RATE * 4 / 1000);   // 4 ms
static constexpr uint32_t RELEASE_FR = (uint32_t)(AUDIO_SAMPLE_RATE * 6 / 1000);   // 6 ms

static inline uint32_t ms_to_frames(uint16_t ms) {
    return (uint32_t)((uint32_t)ms * AUDIO_SAMPLE_RATE / 1000);
}

void DualToneSound::start_note(int i) {
    note_     = i;
    note_pos_ = 0;
    note_len_ = ms_to_frames(def_->notes[i].dur_ms);
    if (note_len_ == 0) note_len_ = 1;
    pa_ = 0.0f;
    pb_ = 0.0f;
}

void DualToneSound::trigger() {
    done_ = false;
    start_note(0);
}

bool DualToneSound::render(int16_t* stereo, size_t frames) {
    for (size_t k = 0; k < frames; k++) {
        float s = 0.0f;
        if (!done_) {
            const DualNote& n = def_->notes[note_];
            if (n.fa > 0.0f) {                          // fa==0 -> silence (pause de cadence)
                float atk = (note_pos_ < ATTACK_FR) ? (float)note_pos_ / (float)ATTACK_FR : 1.0f;
                uint32_t rem = (note_len_ > note_pos_) ? (note_len_ - note_pos_) : 0;
                float rel = (rem < RELEASE_FR) ? (float)rem / (float)RELEASE_FR : 1.0f;

                float v = sinf(pa_);
                pa_ += 2.0f * (float)M_PI * n.fa / (float)AUDIO_SAMPLE_RATE;
                if (pa_ >= 2.0f * (float)M_PI) pa_ -= 2.0f * (float)M_PI;
                if (n.fb > 0.0f) {                      // somme des deux tonalités (-6 dB chacune)
                    v = 0.5f * (v + sinf(pb_));
                    pb_ += 2.0f * (float)M_PI * n.fb / (float)AUDIO_SAMPLE_RATE;
                    if (pb_ >= 2.0f * (float)M_PI) pb_ -= 2.0f * (float)M_PI;
                }
                s = v * atk * rel * def_->gain * GAIN_MAX;
            }
            if (++note_pos_ >= note_len_) {
                if (note_ + 1 < def_->count) start_note(note_ + 1);
                else done_ = true;
            }
        }
        if (s > 1.0f)  s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t o = (int16_t)(s * 32767.0f);
        stereo[k * 2]     = o;
        stereo[k * 2 + 1] = o;
    }
    return !done_;
}
