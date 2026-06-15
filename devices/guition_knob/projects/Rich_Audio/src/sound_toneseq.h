#pragma once
#include "sounds.h"

// Timbre de l'oscillateur.
enum ToneWave : uint8_t { TW_SINE, TW_SQUARE, TW_TRI };
// Enveloppe d'amplitude appliquée à chaque note.
enum ToneEnv  : uint8_t { TE_PLUCK, TE_PAD };   // PLUCK = decay exp ; PAD = sustain

struct ToneNote {
    float    freq;     // Hz ; 0 = silence (pause)
    uint16_t dur_ms;
};

struct ToneSeqDef {
    const char*     name;
    const ToneNote* notes;
    uint8_t         count;
    ToneWave        wave;
    ToneEnv         env;
    float           gain;   // 0..1 (les sons système n'ont pas tous le même niveau)
};

// Générateur générique one-shot : joue une séquence de notes monophonique (un timbre,
// une enveloppe). Une seule classe, pilotée par des tables -> des dizaines de sons
// « système » (bips, dings, carillons, erreurs, jingles, power-up) sans code dédié.
// Attaque/release courts pour déclicker les bords de note (utile pour les carrés).
class ToneSeqSound : public Sound {
public:
    explicit ToneSeqSound(const ToneSeqDef* def) : def_(def) {}
    const char* name() const override { return def_->name; }
    void trigger() override;
    bool render(int16_t* stereo, size_t frames) override;

private:
    const ToneSeqDef* def_;
    int      note_     = 0;      // index de la note courante
    uint32_t note_pos_ = 0;      // frames écoulés dans la note courante
    uint32_t note_len_ = 0;      // durée de la note courante (frames)
    float    phase_    = 0.0f;
    bool     done_     = false;

    void start_note(int i);
};
