# Multi-Visualizer pour Guition K718 — `Basic_Audio_Multiviz`

**Date** : 2026-05-21
**Auteur** : Jean-Paul Gavini (avec Claude)
**Statut** : Design — en attente d'implémentation
**Device cible** : Guition JC3636K718 (`devices/guition_knob/`)

## Contexte

`Basic_Audio_Visualizer` (commit `df23628`) montre que la chaîne mic PDM → FFT 512 → 13 bandes log fonctionne sur le Guition K718. C'est aujourd'hui une démo monolithique : un seul rendu (13 barres radiales) hardcodé dans `main.cpp`.

L'objectif de cette nouvelle démo : cycler entre plusieurs visualisations audio-réactives en tournant l'encodeur — inspiré de l'esprit Winamp (AVS / MilkDrop / G-Force / Geiss). Le set vise 12 vizs hétérogènes (spectrales, temporelles, géométriques, particulaires, atmosphériques) pour exploiter pleinement l'écran rond 360×360 + l'anneau 13 WS2812.

## Objectifs

- **Cycler** entre 12 visualisations via la rotation de l'encodeur (1 cran = viz suivante / précédente).
- **OSD** discret en haut d'écran qui affiche `"N/12 — Nom"` pendant ~1.2 s après chaque switch, puis disparaît.
- **Tick haptique** sur chaque switch si le DRV2605 est détecté (probe au boot).
- **Pipeline audio mutualisé** : la frame audio (waveform + FFT + bandes + RMS + transient flag) est calculée une seule fois par tour de loop et passée à la viz active.
- **Init/deinit propre par viz** : on libère les objets LVGL d'une viz en sortie avant d'initialiser la suivante (pas d'empilement, pas de fuite).
- **Conserver** `Basic_Audio_Visualizer` intact comme démo minimale ("hello world" mic+FFT).

## Non-objectifs (YAGNI)

- **Pas de persistance NVS** du viz courant entre reboots — démarre toujours sur viz #1.
- **Pas d'interaction touch** dans la V1 (la rotation suffit). Hook conceptuel laissé ouvert pour V2 (freeze / reset).
- **Pas de vrai fractal Julia/Mandelbrot** : remplacé par un kaléidoscope géométrique qui donne le feel fractal sans le coût pixel-par-pixel (~1-2 fps sinon).
- **Pas de transition cross-fade** entre vizs : switch instantané (l'OSD signale le changement).
- **Pas de menu / settings écran** : la démo est minimaliste, l'encodeur fait tout.

## Décisions de design

### D1 — Architecture : registry de `Visualizer` (struct + fn pointers)

Chaque viz vit dans son propre fichier `viz_<nom>.cpp` et expose une constante `Visualizer` :

```cpp
// viz_api.h
struct AudioFrame {
    const int16_t* wave;       // 512 samples bruts (waveform)
    const double*  fft_mag;    // 256 bins de magnitude FFT
    const float*   bands;      // 13 bandes log normalisées [0..1] (post peak-meter)
    float          rms;        // RMS normalisé [0..1]
    bool           transient;  // true si onset détecté ce tick
    uint32_t       t_ms;       // millis() au tick
};

struct Visualizer {
    const char* name;
    void (*init)();
    void (*render)(const AudioFrame&);
    void (*deinit)();
};
```

`main.cpp` tient un `const Visualizer* visualizers[12]` et un `int current_viz`. Sur switch :
```cpp
visualizers[current_viz]->deinit();
current_viz = (current_viz + delta + 12) % 12;
visualizers[current_viz]->init();
osd_show(current_viz);
haptic_tick();
```

Justification : pattern AVS de Winamp ; ajouter une viz = 1 fichier + 1 ligne dans le tableau ; pas d'allocations dynamiques, pas de vtable.

### D2 — Liste des 12 visualisations, dans l'ordre de cyclage

| # | Nom | Concept | Primitives LVGL | Anneau 13 LEDs |
|---|---|---|---|---|
| 1 | Spectrum Radial | 13 barres radiales, hue rouge (graves) → violet (aigus), longueur ∝ bande. Port du code de `Basic_Audio_Visualizer`. | 13 × `lv_line` | 13 bandes (mirror) |
| 2 | Oscilloscope | Waveform brute 512 pts sur ligne horizontale centrée, vert phosphorescent. | 1 × `lv_line` (512 pts) | Toutes même couleur, val ∝ RMS |
| 3 | Spectrum Bars | Le classique Winamp : 13 barres verticales depuis le bas, "peak caps" qui retombent lentement. | 13 × `lv_bar` + 13 × `lv_obj` (caps) | 13 bandes (mirror) |
| 4 | Peak Meter | Disque plein au centre, rayon ∝ RMS, hue qui tourne lentement (~10°/s). Flash blanc full-screen 80 ms sur transient. | 1 × `lv_arc` (cercle plein) + 1 × `lv_obj` (flash) | Wash pulsé, couleur du disque |
| 5 | Lissajous | X = `wave[2i]`, Y = `wave[2i+1]`, 256 points, ligne fine cyan. Centre = milieu d'écran, échelle dynamique. | 1 × `lv_line` (256 pts) | Dégradé HSV qui tourne |
| 6 | Tunnel | 8 anneaux concentriques (radii 30, 50, 70, 90, 110, 130, 150, 170), épaisseur ∝ bande correspondante, hue rotate selon `t_ms`. | 8 × `lv_arc` | Wash hue qui rotate |
| 7 | Beat Bloom | Sur chaque transient : un cercle blanc explose du centre (rayon 0 → 180 px en 200 ms), couleur ∝ bande dominante. 3 cercles fantômes décolorant en arrière-plan (trails). | 4 × `lv_arc` | Flash blanc sur transient |
| 8 | Starfield | 24 particules (positions internes), angle réparti uniformément, rayon avance vers l'extérieur à vitesse ∝ bass. Wrap au bord. Hue ∝ angle. Onset → boost vitesse +50 % pendant 200 ms. | 24 × `lv_obj` (petits cercles 6 px) | 13 bandes (mirror) |
| 9 | Geiss Stripes | 13 bandes horizontales pleines couvrant l'écran, gradient HSV qui ondule (rotation lente du hue), brightness ∝ bande correspondante. | 13 × `lv_obj` (rectangles pleins) | 13 bandes (mirror) |
| 10 | G-Wave | Anneaux concentriques émis du centre vers l'extérieur sur chaque transient (et tous les ~400 ms en mode "calm" sans transient). Hue ∝ moment d'émission. Effet "lens-flare" psychédélique. Pool circulaire de 6 anneaux réutilisables. | 6 × `lv_arc` | Wash qui rotate |
| 11 | Kaleidoscope | 8 segments en symétrie radiale (1/8 de cercle chacun). Chaque segment rend un motif géométrique (3-5 lignes courbées) déformé par les basses (paramètre d'amplitude du motif ∝ bande 0). Feel "fractal" sans le coût Mandelbrot. | 8 × 5 = 40 × `lv_line` courtes | Wash hue rotate |
| 12 | Matrix Rain | ~18 colonnes verticales de caractères verts qui tombent. Vitesse de chute par colonne ∝ bande des aigus (bandes 8-12, moyennées). Brightness du bas (blanc, "tête de la goutte") vers le haut (vert sombre, "trail"). Caractères ASCII random renouvelés à chaque cycle de descente. | 18 × `lv_label` (string multi-ligne) | Toutes vert, val ∝ bandes aigus |

### D3 — Encoder UX : "snappy"

- 1 cran de molette = passe à la viz suivante immédiatement (`delta = ±1`).
- Pas d'anti-bounce.
- OSD instant fade-in, 800 ms plein, 400 ms fade-out (total ~1.2 s).
- Tick haptique synchronisé avec le switch (si DRV présent).
- Wrap circulaire (après #12 → #1, avant #1 → #12).

### D4 — Audio pipeline mutualisé

Le code mic + FFT + bandes de `Basic_Audio_Visualizer` est extrait vers un module dédié `audio_pipeline.{h,cpp}`. API :

```cpp
// audio_pipeline.h
void audio_pipeline_init();        // mic_init + FFT setup + band layout
const AudioFrame& audio_pipeline_tick();  // blocking read + FFT + bands + transient
```

Ajout par rapport à l'existant : **transient detection**. Méthode simple :

```
energy = sum(fft_mag[1..256])
ring_buffer.push(energy)  // taille 20
avg = mean(ring_buffer)
transient = (energy > 1.5 * avg) && (t_ms - last_transient_ms > 80)
```

Cooldown 80 ms évite les "doubles tirs" sur un même kick. Seuil 1.5× ajustable.

### D5 — OSD

`lv_label` placé sur `lv_layer_top()` (donc au-dessus de toute viz qui se contenterait de `lv_scr_act()`). Aligné `LV_ALIGN_TOP_MID, 0, 24`. Police `lv_font_montserrat_20`, blanc sur fond noir semi-transparent (rectangle `lv_obj_t` 80 % opacité, padding 8 px).

API minimale :
```cpp
// osd.h
void osd_init();
void osd_show(int viz_index);  // déclenche fade-in
void osd_tick();               // appelé chaque frame, gère le fade-out
```

L'OSD est **owned by main**, jamais touché par les vizs (qui travaillent sur `lv_scr_act()` uniquement).

### D6 — Haptic

Probe DRV2605 au boot (`drv.begin()`), pareil que `Hue_Encoder`. Si présent :
```cpp
drv.useLRA(); drv.selectLibrary(6); drv.setMode(DRV2605_MODE_INTTRIG);
// sur chaque switch :
drv.setWaveform(0, 7);  // Soft Bump (plus discret que Strong Click)
drv.setWaveform(1, 0);
drv.go();
```

Si absent : `drv_ok = false`, aucun appel haptique. La démo fonctionne sans (juste sans tick).

### D7 — Touch screen : non utilisé en V1

Le bus I2C est déjà nécessaire pour le DRV2605, donc `Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL)` est appelé de toute façon — le touch est connecté physiquement mais ignoré par le code. Hook V2 conceptuel : tap = freeze viz (utile pour photographier), double-tap = retour à viz #1.

### D8 — Persistance : aucune

`current_viz` vit en RAM. Au reboot, on démarre sur viz #1 (Spectrum Radial). Évite NVS et évite un boot bloqué sur une viz buggée.

### D9 — Vizs "rectangulaires" sur écran rond : effet hublot accepté

L'écran est rond 360×360 (les pixels hors du cercle ne sont pas visibles). Trois vizs sont naturellement "rectangulaires" : #3 Spectrum Bars (barres verticales), #9 Geiss Stripes (bandes horizontales), #12 Matrix Rain (colonnes verticales). Décision : **on accepte l'effet "hublot"** — le contenu rectangulaire est rogné par le cercle. C'est cohérent avec l'esthétique CRT/oscillo, et ça évite la complication de calculer une largeur utile par hauteur (chord du cercle).

Les vizs concernées sont libres de placer leur contenu plein écran (`0..360` en X et Y) sans se soucier du masque circulaire — l'écran physique fait le crop.

### D10 — Defaults ring brightness, FFT, bandes

Repris de `Basic_Audio_Visualizer` :
- Ring brightness : 200/255 (≈ 78 %, OK USB 500 mA).
- LVGL buffer : 72 lignes (cf. `guition_lvgl_init(72)`).
- Sample rate : 16 kHz, FFT size 512 (~32 ms window).
- 13 bandes log entre 100 Hz et 7 kHz.
- Peak follower : attack instant, release `peak = 0.97 * peak + 0.03 * max_band`.
- Peak floor : 20 000 (évite l'auto-gain extrême en silence).
- Band display : peak meter par bande, attack instant, release `band = 0.75 * band + 0.25 * new`.

## Architecture cible

```
devices/guition_knob/projects/Basic_Audio_Multiviz/
├── platformio.ini                # mêmes deps que Basic_Audio_Visualizer + nothing more
└── src/
    ├── main.cpp                  # setup, loop, encoder polling, viz dispatch
    ├── lv_conf.h                 # copié de Basic_Audio_Visualizer
    ├── viz_api.h                 # struct AudioFrame, struct Visualizer
    ├── audio_pipeline.h
    ├── audio_pipeline.cpp        # mic PDM init + capture + FFT + bands + transient
    ├── osd.h
    ├── osd.cpp                   # label fade-in/out
    ├── viz_spectrum_radial.cpp   # #1
    ├── viz_oscillo.cpp           # #2
    ├── viz_spectrum_bars.cpp     # #3
    ├── viz_peak_meter.cpp        # #4
    ├── viz_lissajous.cpp         # #5
    ├── viz_tunnel.cpp            # #6
    ├── viz_beat_bloom.cpp        # #7
    ├── viz_starfield.cpp         # #8
    ├── viz_geiss_stripes.cpp     # #9
    ├── viz_g_wave.cpp            # #10
    ├── viz_kaleidoscope.cpp      # #11
    └── viz_matrix_rain.cpp       # #12
```

`platformio.ini` reprend strictement celui de `Basic_Audio_Visualizer` (mêmes `lib_deps` : `lvgl@^8.4.0`, `Adafruit NeoPixel`, `arduinoFFT@^2.0.4`). Ajout d'une seule lib si DRV2605 utilisé : `adafruit/Adafruit DRV2605 Library`.

## Data flow

```
loop():
  AudioFrame af = audio_pipeline_tick();  // ~32 ms (blocking I2S read)
  int delta = encoder_consume_delta();    // lit + reset enc_delta atomique
  if (delta) {
      visualizers[current_viz]->deinit();
      current_viz = (current_viz + delta + 12) % 12;
      visualizers[current_viz]->init();
      osd_show(current_viz);
      haptic_tick();
  }
  visualizers[current_viz]->render(af);   // viz écrit sur lv_scr_act() + ring framebuffer
  rgb_ring_show();                         // push framebuffer
  osd_tick();                              // gère le fade
  lv_timer_handler();
  delay(1);
```

## Performance budget

Cible : **~25 fps** (40 ms/frame).

| Étage | Coût estimé |
|---|---|
| `i2s_channel_read` blocking 512 samples @ 16 kHz | ~32 ms (incompressible — c'est le temps réel de capture) |
| FFT 512 + bandes | ~3 ms |
| Transient detect | < 0.1 ms |
| Render viz (typique) | 1-3 ms |
| Render viz (lourde : Matrix Rain) | jusqu'à 8 ms |
| `rgb_ring_show()` (13 LEDs WS2812) | ~0.4 ms |
| `lv_timer_handler()` + flush LVGL | 3-5 ms |
| **Total** | **~40-50 ms/frame ⇒ 20-25 fps** |

Le goulot d'étranglement est le blocking I2S read (32 ms). Pour gagner en fps, il faudrait soit (a) réduire la FFT à 256 (perd de la résolution fréquentielle, gagne 16 ms), soit (b) passer en double-buffering async I2S (complexifie significativement). On reste sur 512 samples pour cohérence avec `Basic_Audio_Visualizer` et lisibilité.

Si une viz précise tombe à < 15 fps, on la profile et on la simplifie (ex : Matrix Rain → moins de colonnes, moins de chars).

## Risques identifiés

- **Matrix Rain** : 18 `lv_label` ré-écrits à chaque frame de descente pourraient être lents (string allocations + LVGL re-layout). Mitigation : réécrire seulement la colonne qui descend ce tick (pas toutes), throttler à ~10 Hz.
- **Geiss Stripes** : 13 rectangles plein écran avec couleur changeante = beaucoup de pixels à blit. Mitigation : utiliser `lv_obj` avec `bg_color` (pas un canvas) — LVGL optimise les rectangles unicolores.
- **G-Wave** : pool d'anneaux animés ; risque de "fuite" si on alloue/désalloue des `lv_arc` ; mitigation : pool circulaire pré-alloué de 6 `lv_arc` réutilisés en réinitialisant rayon + opacité.
- **Beat Bloom** : 4 `lv_arc` en surimpression peuvent dégrader le framerate si l'opacité multiple les blits ; mitigation : limiter à 3 trails actifs simultanément.
- **Init/deinit cost** : créer/détruire 13-40 objets LVGL à chaque switch peut causer un freeze visible (~50-100 ms ?). Mitigation : si problème, on remplace `deinit() / init()` par un système `show() / hide()` qui garde tous les objets vivants — au prix de plus de RAM permanente.

## Stratégie d'implémentation (haut niveau, le plan détaillé suivra)

1. **Scaffolding** : créer le projet, copier `lv_conf.h`, créer `viz_api.h`, copier le `platformio.ini` de `Basic_Audio_Visualizer`.
2. **Audio pipeline modularisé** : extraire le code mic + FFT + bandes de `Basic_Audio_Visualizer` vers `audio_pipeline.cpp`, ajouter la transient detection.
3. **Encoder + main loop squelette** : init encoder, lecture delta, dispatch vers une viz unique (la #1).
4. **Viz #1 — Spectrum Radial** : port du rendu existant dans `viz_spectrum_radial.cpp`. Vérifier que tout fonctionne identiquement à `Basic_Audio_Visualizer`.
5. **OSD + haptic** : implémenter OSD et tick haptique, tester avec viz #1 seule.
6. **Vizs #2–#12** : une par une, dans l'ordre de la table D2. Chacune avec un test visuel : tourner la molette, observer.
7. **Profiling** : mesurer le framerate par viz, corriger les vizs lentes.
8. **README** : courte note dans `devices/guition_knob/README.md` pour pointer la nouvelle démo.

## Critères de succès (definition of done)

- [ ] Le projet `Basic_Audio_Multiviz` build sans warning et flash sur le Guition.
- [ ] Les 12 vizs sont implémentées et accessibles en tournant l'encodeur.
- [ ] Le switch est instantané (perçu < 100 ms d'un viz à l'autre, OSD compris).
- [ ] L'OSD affiche correctement `"N/12 — Nom"` pendant ~1.2 s puis disparaît.
- [ ] Tick haptique sur switch si DRV2605 détecté (testé : présent ou absent gracefully).
- [ ] Framerate ≥ 20 fps pour chaque viz (≥ 25 fps idéalement pour les plus simples).
- [ ] `Basic_Audio_Visualizer` reste intact (build et fonctionne identiquement).
- [ ] Pas de fuite mémoire après 100+ switches (mesuré avec `ESP.getFreeHeap()` log au reboot avant/après cycling intensif).
