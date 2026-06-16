#pragma once
#include <stddef.h>
#include <stdint.h>

// Interface d'un « son » de la library. Volontairement agnostique de la source :
// un son peut être synthétisé (Harp, Ping) ou, à terme, lire un sample WAV embarqué.
// Aucune dépendance Arduino/I2S ici -> les générateurs sont compilables et testables
// en natif (cf. test/). Le timing est compté en *frames* (pas en millis) pour rester
// déterministe et indépendant du matériel.
//
// Convention d'amplitude : render() écrit des frames stéréo entrelacées (L, R) déjà
// mises à l'échelle int16, headroom inclus (~0.5 pleine échelle). Le volume maître est
// appliqué *en aval* par le moteur audio — un générateur ne connaît pas le volume.
class Sound {
public:
    virtual ~Sound() {}

    // Nom affiché dans la liste de l'UI.
    virtual const char* name() const = 0;

    // (Re)démarre le son depuis le début (un « one-shot » : il finit par s'arrêter).
    virtual void trigger() = 0;

    // Remplit `frames` frames stéréo dans `stereo` (2*frames int16 : L,R,L,R...).
    // Retourne true tant que le son produit du signal ; false une fois terminé
    // (ce bloc, qui peut contenir la fin de la traîne, est alors le dernier).
    virtual bool render(int16_t* stereo, size_t frames) = 0;
};

// --- Registre statique de la library (plat, ordonné par catégorie) ---
int          sounds_count();          // nombre de sons disponibles
Sound*       sounds_get(int index);   // instance (nullptr si index hors bornes)
const char*  sounds_name(int index);  // nom (chaîne vide si index hors bornes)

// --- Catégories (pages de l'UI) ---
// Chaque catégorie est une tranche contiguë [start, start+count) du registre plat.
struct SoundCategory { const char* name; int start; int count; };
int                  categories_count();
const SoundCategory* category_get(int index);   // nullptr si index hors bornes
