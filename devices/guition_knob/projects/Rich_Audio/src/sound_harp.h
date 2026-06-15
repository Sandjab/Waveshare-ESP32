#pragma once
#include "sounds.h"

// Harp — arpège Cmaj7 (C4, E4, G4, B4) en synthèse Karplus-Strong, joué UNE fois
// par trigger() puis s'éteint. Portage one-shot de Basic_Audio_Harp (qui bouclait).
// Chaque corde est un buffer circulaire (longueur N = SR/freq) initialisé en bruit
// blanc au pluck, puis filtré sample-par-sample (moyenne glissante amortie).
class HarpSound : public Sound {
public:
    const char* name() const override { return "Harp"; }
    void trigger() override;
    bool render(int16_t* stereo, size_t frames) override;

private:
    static constexpr int    kVoices       = 4;
    static constexpr size_t kMaxStringLen = 200;   // >= SR/220Hz

    struct Voice {
        float  buf[kMaxStringLen];
        size_t len;
        size_t idx;
    };

    Voice    voices_[kVoices];
    uint32_t pos_         = 0;   // frames écoulés depuis le trigger
    int      next_pluck_  = 0;   // index de la prochaine corde à pincer
    uint32_t next_pluck_at_ = 0; // frame du prochain pluck
    uint32_t rng_         = 0x1234567u;

    void  voice_init(Voice& v, float freq);
    void  voice_pluck(Voice& v);
    float voice_next(Voice& v);
    float frand();               // bruit blanc [-1,1], PRNG local (pas Arduino random())
};
