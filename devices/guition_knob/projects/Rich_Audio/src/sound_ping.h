#pragma once
#include "sounds.h"

// Ping — notification/cloche cristalline : synthèse additive de 4 partiels
// inharmoniques (1, 2, 2.76, 5.4 × fondamentale) à enveloppe exponentielle
// décroissante. Un coup par trigger() puis s'éteint. Portage one-shot de
// Basic_Audio_Ping (qui re-déclenchait toutes les 1.5 s).
class PingSound : public Sound {
public:
    const char* name() const override { return "Ping"; }
    void trigger() override;
    bool render(int16_t* stereo, size_t frames) override;

private:
    static constexpr int kPartials = 4;

    struct Partial {
        float freq;          // Hz
        float amp;           // pic (0..1)
        float decay_tau_s;   // env = exp(-t/tau)
        float phase;
        float phase_inc;     // 2π·f/SR
    };

    Partial  partials_[kPartials];
    uint32_t pos_ = 0;       // frames écoulés depuis le trigger
};
