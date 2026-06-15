# Rich_Audio — banc d'essai de sons (Guition JC3636K718)

Projet pour **tester différents sons** sur le Guition : parcourir une library de sons
organisée en **catégories navigables au swipe**, en déclencher la lecture au tactile, et
régler un **volume maître** à l'encodeur. Pensé comme un cadre extensible — ajouter un son
= une ligne au registre.

Catégories (pages, swipe gauche/droite) :

| Catégorie | Contenu | Statut copyright |
|---|---|---|
| **Rich** | Harp (Karplus-Strong), Ping (cloche additive) — portés de `Basic_Audio_*` | original |
| **UI** | Beep, Ding, Ding Dong, Notify, Message, Pop, Tick, Chime, Bell, Confirm, Cancel… | originaux |
| **FX** | Tada, Error, Coin, Power Up/Down, Sweep Up/Down, Alarm, Low Batt, Unlock | originaux |
| **Telephony** | Dial Tone, Busy, Ringback, Reorder, SIT, DTMF 1/5/0/# | **fidèles** aux normes Bellcore / ITU-T Q.23 (faits techniques) |
| **Classics** | Fur Elise, Ode to Joy, Gran Vals, Westminster | **domaine public** (compositeurs †>100 ans), rendus comme suites de notes |
| **Windows** | Win Startup/Shutdown/Error/Notify/Ding | **originaux** évocateurs du style |
| **iOS** | iOS Note/Tritone/Unlock/Sent/Lock | **originaux** évocateurs du style |

> Les vrais sons de marque (Windows/iOS/Mac) sont des œuvres protégées : volontairement
> **non reproduits**. Seuls la téléphonie (normalisée) et les classiques (domaine public)
> sont rendus fidèlement ; les catégories de plateforme sont des créations originales.

## Pilotage

| Action | Geste |
|---|---|
| Changer de catégorie (page) | **Swipe** gauche/droite (droite = suivant) |
| Déclencher un son | **Tap** sur un item de la liste |
| Faire défiler une liste longue | **Scroll vertical** au doigt |
| Volume maître | **Rotation de l'encodeur** (0 → 20) |

> L'encodeur du Guition **n'a pas de bouton-poussoir** (cf. `device-hardware.md`), donc
> la rotation ne sert qu'au volume ; navigation et déclenchement sont tactiles.
> Une rangée de points en haut indique la catégorie active.

## Architecture

```
src/
  main.cpp           setup (display+LVGL, touch, audio, UI, encodeur) ; loop (encodeur + UI + LVGL)
  config.h           SR 44.1 kHz, frames/bloc, plage & défaut de volume
  sounds.h/.cpp      interface Sound + registre plat + table de catégories + tous les presets
  sound_harp.h/.cpp  générateur Karplus-Strong (arpège joué 1×)
  sound_ping.h/.cpp  générateur cloche additive (1 coup)
  sound_toneseq.h/.cpp  générateur générique mono : séquence de notes + timbre + enveloppe
  sound_dualtone.h/.cpp générateur bi-fréquence (téléphonie : 2 sinus simultanés)
  audio_engine.h/.cpp  I2S + tâche FreeRTOS dédiée, volume maître, gestion PA_MUTE
  ui.h/.cpp          LVGL : pages par catégorie + nav swipe + points, liste tactile, arc volume
  encoder_input.h/.cpp  encodeur -> volume
  touch_cst816.h/.cpp   CST816 -> indev LVGL (repris de Rich_Telemetry)
  lv_conf.h          config LVGL
lib/                 esp_lcd_touch + esp_lcd_touch_cst816s (vendorisés)
test/test_core/      tests natifs des générateurs (one-shot, signal, registre)
```

### Moteur audio
Une **tâche FreeRTOS dédiée** (cœur 0) rend en continu le son actif — ou du silence —
puis l'écrit en I2S (`i2s_channel_write` bloquant, qui cadence la tâche au temps réel).
Le rendu est ainsi **découplé de LVGL** : un flush SPI qui bloque la boucle principale ne
provoque pas d'underrun audio.

Communication boucle → tâche sans verrou : une *queue* de longueur 1 (`xQueueOverwrite`)
pour la requête de lecture, un entier `volatile` pour le volume. Seule la tâche audio
touche les objets `Sound`.

L'**ampli NS4150B** (`PIN_PA_MUTE`/GPIO 46) reste **allumé en permanence** (comme
`Basic_Audio_*`). On l'avait d'abord coupé à l'idle pour éviter le souffle, mais son
soft-start (~centaines de ms) avalait alors entièrement les sons courts (Beep/Pop/Tick) :
ampli toujours on = tous les sons audibles, au prix d'un léger souffle au repos.

### Interface `Sound` (extensible)
```cpp
class Sound {
  virtual const char* name() const = 0;
  virtual void   trigger() = 0;                              // (re)démarre, one-shot
  virtual bool   render(int16_t* stereo, size_t frames) = 0; // remplit L/R ; false = terminé
};
```
Volontairement **sans dépendance Arduino/I2S** : les générateurs se compilent et se
testent en natif. Le timing est compté en *frames* (déterministe), pas en `millis()`.
L'interface est agnostique de la source : un futur son pourra lire un sample WAV embarqué
au lieu de synthétiser.

### Ajouter un son
Dans `sounds.cpp` :
- **Preset mono** : table `ToneNote[]` (freq Hz / durée ms ; freq 0 = pause) + `DEFSND(...)`
  (timbre `TW_*`, enveloppe `TE_*`, gain).
- **Preset bi-fréquence** (téléphonie) : table `DualNote[]` (fa, fb, durée) + `DEFDUAL(...)`.
- **Classe dédiée** : pour une synthèse spécifique (ou un futur lecteur de sample WAV),
  `sound_xxx.h/.cpp` avec `class XxxSound : public Sound`.

Puis insérer l'instance dans `g_registry[]` **au sein de sa catégorie** (le registre est
ordonné par catégorie) et ajuster le `start`/`count` de `g_categories[]` en conséquence.
Le test natif `test_categories_tile_registry` échoue si les offsets ne sont plus cohérents.
Pour les tests natifs, ajouter le `.cpp` du générateur au `build_src_filter` de l'env `native`.

## Build / flash

```bash
./build.sh guition_knob Rich_Audio            # build
./build.sh guition_knob Rich_Audio --upload   # build + flash
```

## Tests (natifs, sans matériel)

```bash
pio test -e native -d devices/guition_knob/projects/Rich_Audio
```

Vérifient que **chaque** son du registre est one-shot (terminaison bornée), produit
réellement du signal, rejoue après extinction, a un nom unique, et que les **catégories
pavent exactement** le registre (tranches contiguës, sans trou — l'UI mappe chaque item de
page vers un index global via ces offsets).

## Hors scope (v1)

Pas de WiFi/REST, pas de persistance (le volume repart au défaut au boot), pas d'animation
de l'anneau RGB. Le registre de sons rend ces ajouts simples ultérieurement.
