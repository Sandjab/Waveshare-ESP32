#include "sound_harp.h"
#include "config.h"
#include <math.h>

// Constantes de synthèse (cf. Basic_Audio_Harp).
static constexpr float    PLUCK_AMPLITUDE = 0.30f;
static constexpr float    DAMPING         = 0.996f;   // 1.0 = corde infinie
static constexpr float    GAIN_MAX        = 0.50f;    // headroom 4 voix
static constexpr float    STRING_FREQ[4]  = { 261.63f, 329.63f, 392.00f, 493.88f }; // C4 E4 G4 B4

// Timing en frames (déterministe, indépendant du matériel).
static constexpr uint32_t PLUCK_INTERVAL_FRAMES = (uint32_t)(AUDIO_SAMPLE_RATE * 280 / 1000);  // 280 ms
static constexpr uint32_t TAIL_FRAMES           = (uint32_t)(AUDIO_SAMPLE_RATE * 1500 / 1000);  // traîne
// 4 plucks (aux frames 0, I, 2I, 3I) + traîne après le dernier.
static constexpr uint32_t TOTAL_FRAMES          = PLUCK_INTERVAL_FRAMES * 3 + TAIL_FRAMES;

// PRNG xorshift32 local : bruit blanc déterministe, sans dépendre de random() (Arduino),
// ce qui rend le générateur compilable et reproductible en test natif.
float HarpSound::frand() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return ((float)rng_ / 2147483648.0f) - 1.0f;   // ~[-1, 1)
}

void HarpSound::voice_init(Voice& v, float freq) {
    v.len = (size_t)(AUDIO_SAMPLE_RATE / freq + 0.5f);
    if (v.len > kMaxStringLen) v.len = kMaxStringLen;
    v.idx = 0;
    for (size_t i = 0; i < v.len; i++) v.buf[i] = 0.0f;
}

void HarpSound::voice_pluck(Voice& v) {
    for (size_t i = 0; i < v.len; i++) v.buf[i] = frand() * PLUCK_AMPLITUDE;
}

float HarpSound::voice_next(Voice& v) {
    float out = v.buf[v.idx];
    size_t next = v.idx + 1;
    if (next >= v.len) next = 0;
    v.buf[v.idx] = 0.5f * (v.buf[v.idx] + v.buf[next]) * DAMPING;   // K-S low-pass amorti
    v.idx = next;
    return out;
}

void HarpSound::trigger() {
    rng_ = 0x1234567u;                       // reproductible d'un trigger à l'autre
    for (int i = 0; i < kVoices; i++) voice_init(voices_[i], STRING_FREQ[i]);
    voice_pluck(voices_[0]);                 // première corde immédiatement
    pos_ = 0;
    next_pluck_ = 1;
    next_pluck_at_ = PLUCK_INTERVAL_FRAMES;
}

bool HarpSound::render(int16_t* stereo, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        if (next_pluck_ < kVoices && pos_ >= next_pluck_at_) {
            voice_pluck(voices_[next_pluck_]);
            next_pluck_++;
            next_pluck_at_ += PLUCK_INTERVAL_FRAMES;
        }
        float mix = voice_next(voices_[0]) + voice_next(voices_[1])
                  + voice_next(voices_[2]) + voice_next(voices_[3]);
        mix *= GAIN_MAX;
        if (mix > 1.0f)  mix = 1.0f;
        if (mix < -1.0f) mix = -1.0f;
        int16_t s = (int16_t)(mix * 32767.0f);
        stereo[i * 2]     = s;
        stereo[i * 2 + 1] = s;
        pos_++;
    }
    return pos_ < TOTAL_FRAMES;
}
