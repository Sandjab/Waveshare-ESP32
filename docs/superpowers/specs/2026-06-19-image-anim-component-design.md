# Composant `image_anim` — image animée (Rich_Telemetry)

Date : 2026-06-19
Device : Guition K718 — projet `devices/guition_knob/projects/Rich_Telemetry`
Statut : design validé (brainstorm), prêt pour plan d'implémentation.

## Contexte

Le dashboard config-driven Rich_Telemetry expose une famille de composants définis dans
**4 couches synchronisées** :

1. **Registre designer** — `designer/js/registry.js` (`COMPONENTS`) : palette, défauts, champs
   d'inspecteur, géométrie initiale.
2. **Vtable firmware** — `src/view.cpp` (`VIEW[]`, build/sync) + `src/dashboard.cpp`
   (`APPLY[]`, `apply_*` pour `/update`) + `context_apply` (bind).
3. **Schéma JSON** — `schema/layout.schema.json` (+ copie servie `data/schema/`).
4. **Aperçu designer** — `designer/js/render.js` (`build*`, double-maintenance du rendu firmware).

Le composant **image statique** (`image`, `COMP_IMAGE`) vient d'être ajouté et fixe le **contrat
d'asset** réutilisé ici : le **navigateur** décode/rasterise/convertit, le **device n'affiche que
du RGB565 déjà prêt**. Une image placée = un fichier brut RGB565A8 (3 o/px, alpha) sur LittleFS
(`/img/<clé>.565a`), clé = hash FNV-1a du contenu, dims (`image_w/image_h`) portées par le
`Component` (hors-fichier), chargé en PSRAM via `lv_img_dsc_t`, uploadé par `POST /image`
multipart + sweep des orphelins.

Les précédents **`led_ring`** (modes `SPINNER/BLINK/BREATHE` + `led_period_ms`, avancés par
`led_ring_tick` toutes les 33 ms) et **`sound`** (`snd_pending` one-shot) fixent le **pattern d'anim
maison** : un comportement temporel piloté par un tick périodique et déclenché via `/update`.

## Objectif

Un composant qui reçoit un **GIF animé** ou une **série d'images**, et permet à l'exécution :

- d'afficher **une frame précise** (ex. état on/off) ;
- de jouer une **courte animation**, répétée `loop` fois (0 = infini jusqu'à `stop`), avec un
  **temps inter-frame** paramétrable.

Pilotage **push + bind** : commandes explicites via `/update`, *et* une variable liée (`bind`)
qui sélectionne la frame d'état au repos.

### Échelle visée

Sprites moyens : ~128–200 px, 5–20 frames (~67 Ko/frame en 150×150 → jusqu'à ~1 Mo tout-résident
en PSRAM, confortable sur l'ESP32-S3R8 8 Mo).

## Approche retenue (A) — pack mono-asset + tick maison

Décision (vs alternatives écartées) :

- **B — frames en assets séparés (liste de clés)** : permettrait la dédup et l'édition d'une
  frame isolée, mais impose une **liste** dans le schéma/firmware (N×`ID_LEN`), un sweep qui
  unionne toutes les listes, N allocations à suivre — plomberie disproportionnée à 5–20 frames.
- **C — widget natif `lv_animimg`** : son modèle se bat avec nos besoins (il *possède* l'image
  affichée → frame liée statique bancale ; durée = totale, pas une période ; `stop` = destruction)
  et introduit un **2ᵉ moteur d'anim** à côté du tick led/sound. Écarté (conformité au pattern maison).

**A** = le minimum de plomberie nouvelle : réutilise le contrat « fichier brut + dims dans le
composant » et le tick existant, couvre pile les besoins, marche avec `bind`.

## 1. Données & format d'asset

### Format du pack

- Fichier = **N frames RGB565A8 brutes concaténées**, soit exactement `N × w × h × 3` octets.
- Aucun en-tête : `N`, `w`, `h` vivent dans le `Component` (miroir du contrat image statique).
- Clé = **hash FNV-1a 64 bits** du pack (16 hex minuscules), réutilise `fnv1a64Hex` / `bg_key_valid`.
- Validation **navigateur ET firmware** : `taille_fichier == N × w × h × 3`. Incohérence → rejet
  (firmware : placeholder bordé, comme l'image statique non chargée).

### Stockage & endpoints

- Dossier LittleFS **`/aimg`**, extension **`.565p`** (`p` = pack). Séparé de `/img` pour ne pas
  perturber le sweep image statique (qui suppose 1 fichier = `image_w×image_h×3`).
- **`POST /aimg?key=<hex>`** (multipart, miroir de `h_image_upload`/`h_image_done`) + **`GET /aimg?key=<hex>`**.
- **Sweep parallèle** à celui de `/img` : supprime les `/aimg/*.565p` qu'aucun composant
  `image_anim` ne référence (1 clé/composant via le champ `src`).

### Champs config (schéma `image_anim`)

| Champ | Type | Défaut | Rôle |
|---|---|---|---|
| `src` | string (clé hex) | — | clé du pack `/aimg/<src>.565p` ; vide = pas d'asset |
| `w`, `h` | int | — | dimensions d'une frame (px) |
| `frames` | int | — | nombre N de frames du pack |
| `period` | int (ms) | 100 | temps inter-frame par défaut |
| `rest_frame` | int | 0 | frame affichée au repos / après un play fini |
| `loop` | int | 0 | nb de passes par défaut d'un play ; **0 = infini** |
| `autoplay` | bool | false | démarre la lecture au chargement de la page |
| `bind` | string | — | variable de contexte → frame d'état (réutilise le champ existant) |
| placement | `anchor`/`dx`/`dy` | — | comme l'image statique |

### Caps mémoire (à valider on-device)

- `AIMG_MAX_FRAMES = 32`
- `AIMG_MAX_BYTES ≈ 1 572 864` (1,5 Mo) par composant.
- Dépassement → firmware : refus gracieux (placeholder) ; designer : avertissement (toast) et
  blocage de l'upload, dans l'esprit des gardes de limites firmware déjà en place.

## 2. Comportement runtime (firmware)

### Struct `Component` — nouveaux champs

- **Réutilise `value`** comme **index de frame courante** (champ scalaire existant).
- Config : `aimg_src[ID_LEN]`, `aimg_w`, `aimg_h`, `aimg_frames`, `aimg_period_cfg`,
  `aimg_rest`, `aimg_loop_cfg`, `aimg_autoplay`.
- État de lecture : `aimg_playing` (bool), `aimg_period_ms` (période active), `aimg_loops_left`
  (int ; **-1 = infini**), `aimg_last_ms` (uint32, dernier avancement).

*(Noms indicatifs ; alignables sur les conventions de `Component` lors de l'implémentation. On peut
aussi factoriser avec `image_*` si jugé plus propre, mais garder des champs dédiés évite l'ambiguïté
de type au build/sync.)*

### Chargement PSRAM

- Au `view_rebuild`, charger `/aimg/<src>.565p` dans **un buffer PSRAM contigu** (jumeau de
  `load_image`), et remplir **N descripteurs `lv_img_dsc_t`** pointant dans ce buffer
  (`s_aimg_dsc[idx][f].data = buf + f*w*h*3`, `header.w/h = w/h`, format RGB565A8).
- `build_image_anim` crée un `lv_img`, applique l'alignement, et pose la `src` initiale =
  frame `rest_frame` (ou frame 0). Asset absent → placeholder bordé `w×h` (comme l'image statique).

### Tick d'animation

- Nouveau `aimg_tick(Dashboard* d, uint32_t now_ms)` appelé dans `loop()` à côté de `led_ring_tick`.
- Pour chaque composant `COMP_IMAGE_ANIM` avec `aimg_playing` :
  - si `now - aimg_last_ms < aimg_period_ms` → rien ;
  - sinon `aimg_last_ms = now` ; `value++` ; au wrap (`value >= frames`) → `value = 0` et, si
    `aimg_loops_left > 0`, décrémenter ; si retombé à 0 → `aimg_playing = false`,
    `value = rest_frame` (l'arrêt) ; `aimg_loops_left == -1` → boucle infinie (jamais décrémentée).
  - marquer `dirty` + `values_dirty`.
- Comme le tick countdown, `aimg_tick` n'avance que la **frame logique** ; c'est `view_sync` qui
  peint via `lv_img_set_src(img, &s_aimg_dsc[idx][value])`. Pas de moteur LVGL parallèle.

### `/update` — `apply_image_anim` (champs plats, style `led_ring`)

- `{"<id>": {"frame": K}}` → `value = clamp(K, 0, frames-1)`, `aimg_playing = false`.
- `{"<id>": {"play": true, "loop": L, "period": P}}` → `aimg_period_ms = P | aimg_period_cfg` ;
  `aimg_loops_left = (L|aimg_loop_cfg) == 0 ? -1 : (L|aimg_loop_cfg)` ; `aimg_playing = true` ;
  `aimg_last_ms = 0` ; `value = 0`.
- `{"<id>": {"stop": true}}` → `aimg_playing = false` ; retombe sur `rest_frame` (ou la frame liée
  au prochain `context_apply` si `bind`).

### `bind` (push + bind) — `context_apply`

- Cas `COMP_IMAGE_ANIM` : **seulement si `!aimg_playing`**, `value = clamp((int)var.num, 0, frames-1)`
  (+ `dirty`). C'est l'état au repos (on/off = 0/1, ou un index).
- Pendant un play, le bind est **ignoré** ; à la fin du play (settle), `context_apply` ré-impose la
  frame liée au cycle suivant. Règle mentale : **bind = état au repos ; play = transitoire**.

### `autoplay`

- Au `view_rebuild`, si `aimg_autoplay` et asset chargé → démarrer un play (loop = `aimg_loop_cfg`,
  period = `aimg_period_cfg`). Couvre le cas spinner décoratif sans `/update`.

## 3. Designer / UX

### Conversion d'asset — `designer/js/image-anim-asset.js` (miroir de `image-asset.js`)

- **Entrée GIF** : décodage via **`ImageDecoder`** (WebCodecs, Chromium — environnement de l'utilisateur).
  Pour chaque frame : `decode(index)` → dessin canvas → `getImageData` → `rgba8888ToRgb565a8`
  (réutilisé d'`image-asset.js`). Les **durées de frames** du GIF servent de défaut importé pour
  `period` (moyenne ou première).
- **Entrée série d'images** : sélection multi-fichiers, triés par nom ; chaque fichier décodé via
  `createImageBitmap` → rasterisé à `w×h` → converti.
- Frames concaténées en **un seul `Uint8Array`** (le pack) → `fnv1a64Hex` → cache → `POST /aimg`.
- `referencedAimgKeys(state)` pour l'upload/sweep côté `app.js` (miroir de `referencedImageKeys`).
- Rehydrate depuis le device (`GET /aimg`) au reload, comme l'image statique.

### Inspecteur

- Bande de **vignettes** des frames : réordonnancement, sélection de la **frame de repos**.
- Réglages : `period` (ms), cases `autoplay` et `loop` (0 = infini).
- Bouton **« Aperçu »** : anime le canvas via un timer JS honorant `period` → vrai WYSIWYG de
  l'animation (s'arrête à la dé-sélection / re-déclenchable).

### Rendu — `render.js` `buildImageAnim`

- Rend la **frame de repos** en statique (parité avec le device à l'arrêt). L'aperçu animé est géré
  par l'inspecteur, pas par le `build` (qui reste le rendu de référence statique).

## 4. Portée v1, limites, tests

### Hors périmètre v1 (extensions possibles)

- **Timing par frame** : v1 = `period` **uniforme** pour toute la séquence (le GIF n'importe qu'un
  défaut). Une durée par frame serait un tableau + lecture dans le tick — déféré (YAGNI).
- **Bind déclencheur d'anim** (variable franchissant un seuil → joue) : v1 = bind sélectionne la
  frame d'état, pas le déclenchement d'un play.
- **Dédup de frames** entre animations (conséquence du pack mono-asset).

### Tests

- **Designer (`node --test`)** :
  - `registry.test.js` — conformité : la clé `image_anim` existe et == type du schéma.
  - `image-anim-asset.test.js` — conversion d'une frame, concaténation du pack, hash, taille
    `N×w×h×3`, round-trip `rgb565a8 ↔ rgba8888`.
  - `schema.test.js` — `image_anim` valide/invalide selon les champs requis et les caps.
- **Firmware (`pio test`)** : parsing `/update` (`frame`/`play`/`stop`), logique du tick (wrap,
  loop fini/infini, settle), `context_apply` (bind ignoré pendant play), validation taille pack.
- **On-device** (par l'utilisateur, à distance — Guition `192.168.1.35`) : flash, `POST /aimg`,
  `/update` play/frame/stop, `bind`, `autoplay`, capture `/screenshot` → `sips` PNG → envoi.

### Checklist cross-couches (le « registre » à toucher de bout en bout)

1. `enum CompType` : `COMP_IMAGE_ANIM` avant `COMP_COUNT` (les `static_assert` de `VIEW[]`/`APPLY[]`
   garantissent qu'on n'en oublie aucune).
2. `Component` : champs config + état de lecture.
3. `dashboard.cpp` : parse layout (`image_anim`), `apply_image_anim` dans `APPLY[]`, cas
   `context_apply`.
4. `view.cpp` : `s_aimg_buf`/`s_aimg_dsc`, `load_image_anim`, `build_image_anim` + `sync` dans
   `VIEW[]`, `aimg_tick`, libération PSRAM au rebuild.
5. `main.cpp` : appel `aimg_tick` dans `loop()`.
6. `api.cpp` : endpoints `/aimg` (POST/GET) + sweep des orphelins `.565p`.
7. `config.h` : `AIMG_*`, `IMG`-style.
8. Designer : `registry.js`, `image-anim-asset.js`, `inspector.js`, `render.js`, `app.js`
   (upload/sweep/rehydrate), `validate.js`.
9. Schéma : `schema/layout.schema.json` (+ copie `data/schema/`).
10. Tests designer + firmware ci-dessus.
11. Manuel `docs/index.html` : section du nouveau composant (comme l'image statique).
