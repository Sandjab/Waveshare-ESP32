#pragma once
#include "sounds.h"

// Note bi-fréquence : fa=0 -> silence (pause) ; fb=0 -> mono.
struct DualNote { float fa; float fb; uint16_t dur_ms; };

struct DualToneDef {
    const char*     name;
    const DualNote* notes;
    uint8_t         count;
    float           gain;   // 0..1
};

// Générateur one-shot pour les tonalités téléphoniques : somme de deux sinus
// simultanés (la plupart des tons d'appel sont bi-fréquence : DTMF, tonalité,
// occupation…). La cadence on/off est encodée dans la table (note à fa=0 = silence).
// Attaque/release courts pour déclicker. Aucune dépendance HW -> testable en natif.
class DualToneSound : public Sound {
public:
    explicit DualToneSound(const DualToneDef* def) : def_(def) {}
    const char* name() const override { return def_->name; }
    void trigger() override;
    bool render(int16_t* stereo, size_t frames) override;

private:
    const DualToneDef* def_;
    int      note_     = 0;
    uint32_t note_pos_ = 0;
    uint32_t note_len_ = 0;
    float    pa_       = 0.0f;   // phase tonalité basse / mono
    float    pb_       = 0.0f;   // phase tonalité haute
    bool     done_     = false;

    void start_note(int i);
};
