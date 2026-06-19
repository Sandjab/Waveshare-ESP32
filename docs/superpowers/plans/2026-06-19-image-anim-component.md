# Plan d'implémentation — composant `image_anim` (image animée)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ajouter au dashboard Rich_Telemetry un composant `image_anim` qui affiche un GIF/série d'images : frame à volonté (état on/off via `/update` ou `bind`) et lecture d'animation (loop fini/infini, période réglable, stop).

**Architecture :** Approche A de la spec — pack RGB565A8 mono-fichier (N frames brutes concaténées, `/aimg` endpoint + sweep d'orphelins), rendu `lv_img` dont la frame courante (`c.value`) est avancée par un tick maison (`dash_tick_aimg`, jumeau de `dash_tick_countdown`) ; le navigateur décode/convertit, le device n'affiche que du RGB565 prêt. Réutilise les champs `image_src/image_w/image_h` du `Component` (factorisation autorisée par la spec).

**Tech Stack :** C++/Arduino + LVGL 8.x + LittleFS (firmware) ; ArduinoJson 7 (parse/update) ; JS ES modules + `ImageDecoder` (WebCodecs) côté designer ; tests Unity (`pio test -e native`) + `node --test`.

**Spec :** `docs/superpowers/specs/2026-06-19-image-anim-component-design.md`

---

## Conventions de nommage (canoniques — à utiliser à l'identique partout)

| Concept | Nom |
|---|---|
| Type (string layout/schema) | `image_anim` |
| Enum firmware | `COMP_IMAGE_ANIM` (avant `COMP_COUNT`) |
| Def schéma | `comp_image_anim` |
| Endpoint / dossier / extension | `/aimg` · `/aimg` (LittleFS) · `.565p` |
| Clé du pack | `image_src` (réutilisé) — hash FNV-1a, `bg_key_valid` |
| Dims de frame | `image_w`, `image_h` (réutilisés) |
| Module designer | `designer/js/image-anim-asset.js` |

**Champs `Component` ajoutés** (config) : `aimg_frames` (int), `aimg_period` (uint16, ms), `aimg_rest` (int), `aimg_loop` (int, 0=∞), `aimg_autoplay` (bool).
**Champs `Component` ajoutés** (état lecture) : `aimg_playing` (bool), `aimg_period_ms` (uint16, période active), `aimg_loops_left` (int32, -1=∞), `aimg_last_ms` (uint32). **La frame courante = `c.value`** (champ existant).

---

## Structure des fichiers

**Firmware :**
- Modif `src/config.h` — constantes `AIMG_*`.
- Modif `src/dashboard.h` — enum + champs `Component` + déclaration `dash_tick_aimg`.
- Modif `src/dashboard.cpp` — `COMP_NAMES`, parse, `apply_image_anim`+`APPLY[]`, `context_apply`, `dash_tick_aimg`.
- Modif `src/view.cpp` — buffers PSRAM, `aimg_load_component`, `build_image_anim`/`sync_image_anim`+`VIEW[]`, libération au rebuild.
- Modif `src/main.cpp` — appel `dash_tick_aimg` dans `loop()`.
- Modif `src/api.cpp` — endpoints `/aimg` (POST/GET) + sweep `.565p`.
- Modif `test/test_core/test_main.cpp` — tests natifs (parse, apply, tick, bind).

**Designer :**
- Modif `schema/layout.schema.json` (+ copie `data/schema/layout.schema.json`) — `comp_image_anim`.
- Create `designer/js/image-anim-asset.js` — décodage GIF/images → pack + cache d'aperçu.
- Create `designer/tests/image-anim-asset.test.js` — tests purs.
- Modif `designer/js/registry.js` — entrée `image_anim`.
- Modif `designer/js/render.js` — `buildImageAnim`.
- Modif `designer/js/device.js` — `uploadAimg`/`fetchAimg`.
- Modif `designer/js/app.js` — upload + rehydrate des packs.
- Modif `designer/js/inspector.js` — éditeur de frames bespoke.
- Modif `designer/js/validate.js` — limites `image_anim`.

**Docs :** Modif `docs/index.html` — section du composant.

---

# Phase 1 — Logique firmware (testable en natif)

> L'env `native` ne compile que `dashboard.cpp/format/color/nav_logic/context`. Toute la Phase 1 garde `pio test -e native` vert. **Le build esp32s3 reste rouge** dès la Task 2 (le `static_assert` de `VIEW[]` dans view.cpp réclame une ligne) **jusqu'à la Task 8** — c'est attendu ; ne pas lancer `./build.sh` avant la fin de la Phase 2.

## Task 1 : Constantes `AIMG_*`

**Files:**
- Modify: `src/config.h`

- [ ] **Step 1 : Ajouter les constantes après le bloc `IMG_*` (fin du fichier)**

```c
#define AIMG_MAX_W      360                                   // image animee : frame <= ecran
#define AIMG_MAX_H      360
#define AIMG_PX_BYTES   3                                     // RGB565A8 (2 couleur + 1 alpha), comme l'image statique
#define AIMG_MAX_FRAMES 32                                    // nombre max de frames par pack
#define AIMG_MAX_BYTES  1572864                               // ~1,5 Mo : plafond du pack par composant
#define AIMG_DIR        "/aimg"                               // repertoire LittleFS des packs animes
```

- [ ] **Step 2 : Commit**

```bash
git add src/config.h
git commit -m "feat(Rich_Telemetry): constantes AIMG_* (image animee)"
```

## Task 2 : Enum + champs `Component` + déclaration tick

**Files:**
- Modify: `src/dashboard.h`

- [ ] **Step 1 : Ajouter `COMP_IMAGE_ANIM` à l'enum (avant `COMP_COUNT`)**

Remplacer la ligne `enum CompType { ... COMP_IMAGE, COMP_COUNT };` par :

```c
enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND, COMP_CHART, COMP_METER, COMP_IMAGE, COMP_IMAGE_ANIM, COMP_COUNT };
```

- [ ] **Step 2 : Ajouter les champs au `struct Component`**

Après la ligne `int image_w, image_h;` (config image statique), ajouter la config animée :

```c
    // image_anim : config (la cle/dims reutilisent image_src/image_w/image_h)
    int      aimg_frames;            // nombre de frames du pack ; 0 = pas d'asset
    uint16_t aimg_period;            // periode inter-frame par defaut (ms)
    int      aimg_rest;              // frame affichee au repos / apres un play fini
    int      aimg_loop;              // nb de passes par defaut d'un play (0 = infini)
    bool     aimg_autoplay;          // demarre la lecture au chargement de la page
```

Après la ligne `int16_t hist[...]; int hist_count;` (bloc état), ajouter l'état de lecture :

```c
    bool     aimg_playing;           // image_anim : lecture en cours (la frame courante = champ value)
    uint16_t aimg_period_ms;         // image_anim : periode active (surcharge aimg_period via /update)
    int32_t  aimg_loops_left;        // image_anim : passes restantes ; -1 = infini
    uint32_t aimg_last_ms;           // image_anim : millis() du dernier avancement
```

- [ ] **Step 3 : Déclarer `dash_tick_aimg` (près de `dash_tick_countdown`)**

Après `void dash_tick_countdown(Dashboard* d, uint32_t elapsed_s);` ajouter :

```c
void dash_tick_aimg(Dashboard* d, uint32_t now_ms);   // image_anim : avance la frame des composants en lecture
```

- [ ] **Step 4 : Commit** (le build esp32s3 devient temporairement rouge — attendu)

```bash
git add src/dashboard.h
git commit -m "feat(Rich_Telemetry): enum + champs Component pour image_anim"
```

## Task 3 : Parse du layout `image_anim` (TDD natif)

**Files:**
- Modify: `src/dashboard.cpp` (`COMP_NAMES` ~ligne 30, parse ~ligne 97)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Écrire le test (avant `int main()` à la fin du fichier, à côté des autres `test_layout_*`)**

```c
static const char* LAYOUT_AIMG =
  "{\"components\":{"
  "  \"sp\":{\"type\":\"image_anim\",\"src\":\"abcd1234\",\"w\":64,\"h\":64,"
  "         \"frames\":6,\"period\":80,\"rest_frame\":2,\"loop\":3,\"autoplay\":true}},"
  " \"pages\":[{\"name\":\"P\",\"place\":[{\"ref\":\"sp\",\"anchor\":\"CENTER\"}]}]}";

void test_layout_image_anim_parsed(void) {
    static Dashboard d; char err[64];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err)));
    Component& c = d.components[0];
    TEST_ASSERT_EQUAL_INT(COMP_IMAGE_ANIM, c.type);
    TEST_ASSERT_EQUAL_STRING("abcd1234", c.image_src);
    TEST_ASSERT_EQUAL_INT(64, c.image_w);
    TEST_ASSERT_EQUAL_INT(6,  c.aimg_frames);
    TEST_ASSERT_EQUAL_INT(80, c.aimg_period);
    TEST_ASSERT_EQUAL_INT(2,  c.aimg_rest);
    TEST_ASSERT_EQUAL_INT(3,  c.aimg_loop);
    TEST_ASSERT_TRUE(c.aimg_autoplay);
    // autoplay : demarre en lecture, frame 0, 3 passes
    TEST_ASSERT_TRUE(c.aimg_playing);
    TEST_ASSERT_EQUAL_INT(0, c.value);
    TEST_ASSERT_EQUAL_INT(3, c.aimg_loops_left);
}
```

Enregistrer le test dans le runner (ajouter près des autres `RUN_TEST(test_layout_*)`) :

```c
    RUN_TEST(test_layout_image_anim_parsed);
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: FAIL (champs `aimg_*` à 0, `image_anim` parse en `COMP_NONE` → `dash_set_layout` renvoie false).

- [ ] **Step 3 : Ajouter `image_anim` à `COMP_NAMES`**

Étendre le tableau `COMP_NAMES` (la ligne `{ "chart", ... }, { "image", COMP_IMAGE },`) avec :

```c
    { "image_anim", COMP_IMAGE_ANIM },
```

- [ ] **Step 4 : Parser les champs `aimg_*` + init repos/autoplay**

Dans `dash_set_layout`, juste après `c.image_h = o["h"] | 0;` (~ligne 97), ajouter :

```c
        c.aimg_frames   = o["frames"] | 0;
        if (c.aimg_frames > AIMG_MAX_FRAMES) c.aimg_frames = AIMG_MAX_FRAMES;
        if (c.aimg_frames < 0)               c.aimg_frames = 0;
        c.aimg_period   = o["period"] | 100;
        c.aimg_rest     = o["rest_frame"] | 0;
        if (c.aimg_rest < 0) c.aimg_rest = 0;
        if (c.aimg_frames > 0 && c.aimg_rest >= c.aimg_frames) c.aimg_rest = c.aimg_frames - 1;
        c.aimg_loop     = o["loop"] | 0;
        c.aimg_autoplay = o["autoplay"] | false;
        if (c.type == COMP_IMAGE_ANIM) {                    // n'ecrase value que pour ce type
            c.value = c.aimg_rest;                          // frame initiale = repos
            if (c.aimg_autoplay && c.aimg_frames > 0) {
                c.aimg_playing    = true;
                c.aimg_loops_left = (c.aimg_loop <= 0) ? -1 : c.aimg_loop;
                c.aimg_period_ms  = c.aimg_period ? c.aimg_period : 100;
                c.aimg_last_ms    = 0;
                c.value           = 0;
            }
        }
```

- [ ] **Step 5 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: PASS (tous les tests, dont `test_layout_image_anim_parsed`).

- [ ] **Step 6 : Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry): parse layout image_anim + init repos/autoplay"
```

## Task 4 : `apply_image_anim` (/update) + `APPLY[]` (TDD natif)

**Files:**
- Modify: `src/dashboard.cpp` (zone des `apply_*` ~ligne 225, table `APPLY[]` ~ligne 229)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Écrire les tests**

```c
void test_update_aimg_frame_jumps_and_stops(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));   // autoplay=true au depart
    dash_apply_update(&d, "{\"sp\":{\"frame\":4}}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(4, d.components[0].value);
    TEST_ASSERT_FALSE(d.components[0].aimg_playing);
}
void test_update_aimg_frame_clamps(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));   // 6 frames
    dash_apply_update(&d, "{\"sp\":{\"frame\":99}}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(5, d.components[0].value);      // clamp a frames-1
}
void test_update_aimg_play_sets_state(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));
    dash_apply_update(&d, "{\"sp\":{\"play\":true,\"loop\":2,\"period\":40}}", unk, sizeof(unk));
    Component& c = d.components[0];
    TEST_ASSERT_TRUE(c.aimg_playing);
    TEST_ASSERT_EQUAL_INT(40, c.aimg_period_ms);
    TEST_ASSERT_EQUAL_INT(2, c.aimg_loops_left);
    TEST_ASSERT_EQUAL_INT(0, c.value);
}
void test_update_aimg_play_loop0_infinite(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));
    dash_apply_update(&d, "{\"sp\":{\"play\":true,\"loop\":0}}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(-1, d.components[0].aimg_loops_left);   // 0 -> infini
}
void test_update_aimg_stop(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));
    dash_apply_update(&d, "{\"sp\":{\"stop\":true}}", unk, sizeof(unk));
    TEST_ASSERT_FALSE(d.components[0].aimg_playing);
}
```

Enregistrer dans le runner :

```c
    RUN_TEST(test_update_aimg_frame_jumps_and_stops);
    RUN_TEST(test_update_aimg_frame_clamps);
    RUN_TEST(test_update_aimg_play_sets_state);
    RUN_TEST(test_update_aimg_play_loop0_infinite);
    RUN_TEST(test_update_aimg_stop);
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: FAIL de compilation (`APPLY[]` n'a pas `COMP_COUNT` lignes → `static_assert`) ou échec des cas.

- [ ] **Step 3 : Écrire `apply_image_anim` (après `apply_image` ~ligne 227)**

```c
static void apply_image_anim(Component& c, JsonVariantConst v) {
    if (v["stop"] | false) { c.aimg_playing = false; return; }
    if (v["frame"].is<int>()) {
        int fr = v["frame"];
        if (c.aimg_frames > 0) { if (fr < 0) fr = 0; if (fr >= c.aimg_frames) fr = c.aimg_frames - 1; }
        else fr = 0;
        c.value = fr;
        c.aimg_playing = false;
        return;
    }
    if (v["play"] | false) {
        int per  = v["period"] | (int)(c.aimg_period ? c.aimg_period : 100);
        int loop = v["loop"]   | c.aimg_loop;            // 0 = infini
        c.aimg_period_ms  = (uint16_t)(per > 0 ? per : 100);
        c.aimg_loops_left = (loop <= 0) ? -1 : loop;
        c.aimg_playing    = true;
        c.aimg_last_ms    = 0;
        c.value           = 0;
    }
}
```

- [ ] **Step 4 : Ajouter l'entrée à `APPLY[]`**

Après `/* COMP_IMAGE    */ apply_image,` ajouter :

```c
    /* COMP_IMAGE_ANIM */ apply_image_anim,
```

- [ ] **Step 5 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: PASS.

- [ ] **Step 6 : Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry): /update image_anim (frame/play/stop)"
```

## Task 5 : `context_apply` — bind = frame d'état (TDD natif)

**Files:**
- Modify: `src/dashboard.cpp` (`context_apply` switch ~ligne 305, juste avant `default:`)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Écrire les tests**

```c
static const char* LAYOUT_AIMG_BIND =
  "{\"components\":{"
  "  \"sp\":{\"type\":\"image_anim\",\"src\":\"abcd1234\",\"w\":64,\"h\":64,"
  "         \"frames\":4,\"bind\":\"st\"}},"
  " \"pages\":[{\"name\":\"P\",\"place\":[{\"ref\":\"sp\",\"anchor\":\"CENTER\"}]}]}";

void test_ctxapply_aimg_bind_selects_frame(void) {
    static Dashboard d; char err[64];
    dash_set_layout(&d, LAYOUT_AIMG_BIND, err, sizeof(err));
    dash_set_context(&d, "{\"st\":3}", 1000);
    context_apply(&d);
    TEST_ASSERT_EQUAL_INT(3, d.components[0].value);
}
void test_ctxapply_aimg_bind_clamps(void) {
    static Dashboard d; char err[64];
    dash_set_layout(&d, LAYOUT_AIMG_BIND, err, sizeof(err));
    dash_set_context(&d, "{\"st\":9}", 1000);             // > frames-1
    context_apply(&d);
    TEST_ASSERT_EQUAL_INT(3, d.components[0].value);      // clamp a 3
}
void test_ctxapply_aimg_bind_ignored_while_playing(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG_BIND, err, sizeof(err));
    dash_apply_update(&d, "{\"sp\":{\"play\":true}}", unk, sizeof(unk));  // value -> 0, playing
    dash_set_context(&d, "{\"st\":3}", 1000);
    context_apply(&d);
    TEST_ASSERT_EQUAL_INT(0, d.components[0].value);      // bind ignore pendant la lecture
}
```

Enregistrer :

```c
    RUN_TEST(test_ctxapply_aimg_bind_selects_frame);
    RUN_TEST(test_ctxapply_aimg_bind_clamps);
    RUN_TEST(test_ctxapply_aimg_bind_ignored_while_playing);
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: FAIL (`value` reste à `aimg_rest`=0, le bind n'est pas traité).

- [ ] **Step 3 : Ajouter le cas dans `context_apply` (juste avant `default: break;`)**

```c
            case COMP_IMAGE_ANIM:                       // bind = frame d'etat, seulement a l'arret
                if (!c.aimg_playing && v.type == CTX_NUM && c.aimg_frames > 0) {
                    int32_t nv = (int32_t)v.num;
                    if (nv < 0) nv = 0;
                    if (nv >= c.aimg_frames) nv = c.aimg_frames - 1;
                    if (c.value != nv) { c.value = nv; changed = true; }
                }
                break;
```

- [ ] **Step 4 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: PASS.

- [ ] **Step 5 : Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry): context_apply image_anim (bind = frame d'etat)"
```

## Task 6 : `dash_tick_aimg` — moteur d'avance de frame (TDD natif)

**Files:**
- Modify: `src/dashboard.cpp` (après `dash_tick_countdown` ~ligne 327)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Écrire les tests**

```c
void test_aimg_tick_advances_after_period(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));
    dash_apply_update(&d, "{\"sp\":{\"play\":true,\"loop\":0,\"period\":50}}", unk, sizeof(unk));
    dash_tick_aimg(&d, 1000);                  // 1er tick : pose last, n'avance pas (frame 0 affichee)
    TEST_ASSERT_EQUAL_INT(0, d.components[0].value);
    dash_tick_aimg(&d, 1040);                  // < periode : rien
    TEST_ASSERT_EQUAL_INT(0, d.components[0].value);
    dash_tick_aimg(&d, 1060);                  // >= periode : frame 0 -> 1
    TEST_ASSERT_EQUAL_INT(1, d.components[0].value);
    TEST_ASSERT_TRUE(d.components[0].dirty);
}
void test_aimg_tick_finite_loop_settles_to_rest(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));   // 6 frames, rest_frame=2
    dash_apply_update(&d, "{\"sp\":{\"play\":true,\"loop\":1,\"period\":10}}", unk, sizeof(unk));
    uint32_t t = 1000;
    dash_tick_aimg(&d, t);                                // pose last (frame 0)
    for (int i = 0; i < 6; i++) { t += 10; dash_tick_aimg(&d, t); }  // 0->1->2->3->4->5->wrap
    // au wrap apres frame 5 : 1 passe terminee -> stop, frame = rest (2)
    TEST_ASSERT_FALSE(d.components[0].aimg_playing);
    TEST_ASSERT_EQUAL_INT(2, d.components[0].value);
}
void test_aimg_tick_infinite_keeps_playing(void) {
    static Dashboard d; char err[64], unk[64];
    dash_set_layout(&d, LAYOUT_AIMG, err, sizeof(err));   // 6 frames
    dash_apply_update(&d, "{\"sp\":{\"play\":true,\"loop\":0,\"period\":10}}", unk, sizeof(unk));
    uint32_t t = 1000;
    dash_tick_aimg(&d, t);
    for (int i = 0; i < 14; i++) { t += 10; dash_tick_aimg(&d, t); }  // > 2 tours
    TEST_ASSERT_TRUE(d.components[0].aimg_playing);       // infini : ne s'arrete jamais seul
}
```

Enregistrer :

```c
    RUN_TEST(test_aimg_tick_advances_after_period);
    RUN_TEST(test_aimg_tick_finite_loop_settles_to_rest);
    RUN_TEST(test_aimg_tick_infinite_keeps_playing);
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: FAIL de lien (`dash_tick_aimg` non défini).

- [ ] **Step 3 : Implémenter `dash_tick_aimg` (après `dash_tick_countdown`)**

```c
void dash_tick_aimg(Dashboard* d, uint32_t now_ms) {
    for (int i = 0; i < d->comp_count; i++) {
        Component& c = d->components[i];
        if (c.type != COMP_IMAGE_ANIM || !c.aimg_playing) continue;
        if (c.aimg_frames <= 0) { c.aimg_playing = false; continue; }
        if (c.aimg_last_ms == 0) { c.aimg_last_ms = now_ms; continue; }   // 1er tick : montre frame 0 une periode
        uint16_t per = c.aimg_period_ms ? c.aimg_period_ms : 100;
        if ((now_ms - c.aimg_last_ms) < per) continue;
        c.aimg_last_ms = now_ms;
        int32_t nf = c.value + 1;
        if (nf >= c.aimg_frames) {
            nf = 0;
            if (c.aimg_loops_left > 0) {                  // -1 = infini : jamais decremente
                c.aimg_loops_left--;
                if (c.aimg_loops_left == 0) {             // derniere passe terminee
                    c.aimg_playing = false;
                    nf = (c.aimg_rest >= 0 && c.aimg_rest < c.aimg_frames) ? c.aimg_rest : 0;
                }
            }
        }
        c.value = nf;
        c.dirty = true;
        d->values_dirty = true;
    }
}
```

- [ ] **Step 4 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native -f test_core`
Expected: PASS (toute la suite native).

- [ ] **Step 5 : Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry): dash_tick_aimg (avance de frame, loop fini/infini)"
```

---

# Phase 2 — Rendu firmware + endpoints (vérifiés on-device / curl)

## Task 7 : Rendu LVGL — chargement PSRAM, build, sync, `VIEW[]`, libération

**Files:**
- Modify: `src/view.cpp`

- [ ] **Step 1 : Déclarer les buffers (après `s_img_dsc` ~ligne 24)**

```c
// Images animees : pack RGB565A8 multi-frames en PSRAM + un descripteur lv_img par frame.
static uint8_t*      s_aimg_buf[MAX_COMPONENTS] = {0};
static lv_img_dsc_t* s_aimg_dsc[MAX_COMPONENTS] = {0};   // tableau de c.aimg_frames descripteurs (PSRAM)
```

- [ ] **Step 2 : Ajouter `aimg_load_component` (après `img_load_component` ~ligne 455)**

```c
// Charge /aimg/<src>.565p en PSRAM (pack RGB565A8 de N frames) et remplit N descripteurs.
// Idempotent. false si invalide (asset absent, dims/compte nuls, taille incoherente, alloc ratee).
static bool aimg_load_component(Dashboard* d, int idx) {
    if (idx < 0 || idx >= d->comp_count || idx >= MAX_COMPONENTS) return false;
    Component& c = d->components[idx];
    if (!c.image_src[0] || c.image_w <= 0 || c.image_h <= 0 || c.aimg_frames <= 0) return false;
    if (s_aimg_buf[idx]) return true;                      // deja charge
    if (c.aimg_frames > AIMG_MAX_FRAMES) return false;
    size_t frame_bytes = (size_t)c.image_w * c.image_h * AIMG_PX_BYTES;
    size_t need = frame_bytes * (size_t)c.aimg_frames;
    if (need == 0 || need > (size_t)AIMG_MAX_BYTES) return false;
    char path[40];
    snprintf(path, sizeof(path), "%s/%s.565p", AIMG_DIR, c.image_src);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if ((size_t)f.size() != need) { f.close(); return false; }
    uint8_t* buf = (uint8_t*)heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
    if (!buf) { f.close(); return false; }
    size_t rd = f.read(buf, need);
    f.close();
    if (rd != need) { heap_caps_free(buf); return false; }
    lv_img_dsc_t* dscs = (lv_img_dsc_t*)heap_caps_malloc(sizeof(lv_img_dsc_t) * (size_t)c.aimg_frames, MALLOC_CAP_SPIRAM);
    if (!dscs) { heap_caps_free(buf); return false; }
    for (int fr = 0; fr < c.aimg_frames; fr++) {
        lv_img_dsc_t& dsc = dscs[fr];
        memset(&dsc, 0, sizeof(dsc));
        dsc.header.always_zero = 0;
        dsc.header.cf  = LV_IMG_CF_TRUE_COLOR_ALPHA;
        dsc.header.w   = c.image_w;
        dsc.header.h   = c.image_h;
        dsc.data       = buf + (size_t)fr * frame_bytes;
        dsc.data_size  = frame_bytes;
    }
    s_aimg_buf[idx] = buf;
    s_aimg_dsc[idx] = dscs;
    return true;
}
```

- [ ] **Step 3 : Ajouter `build_image_anim` + `sync_image_anim` (après `build_image` ~ligne 278)**

```c
static void build_image_anim(lv_obj_t* parent, Component& c, Placement& q,
                             lv_obj_t** main, lv_obj_t**, lv_obj_t**) {
    lv_obj_t* img = lv_img_create(parent);
    int idx = q.comp_index;
    if (idx >= 0 && idx < MAX_COMPONENTS && s_aimg_buf[idx] && s_aimg_dsc[idx]) {
        int fr = c.value;
        if (fr < 0 || fr >= c.aimg_frames) fr = 0;
        lv_img_set_src(img, &s_aimg_dsc[idx][fr]);
    } else {                                              // asset non charge : placeholder borde
        lv_obj_set_size(img, c.image_w > 0 ? c.image_w : 120, c.image_h > 0 ? c.image_h : 120);
        lv_obj_set_style_border_width(img, 1, 0);
        lv_obj_set_style_border_color(img, lv_color_hex(0x4B5563), 0);
        lv_obj_set_style_border_opa(img, LV_OPA_COVER, 0);
    }
    lv_obj_align(img, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = img;
}
static void sync_image_anim(Component& c, Placement& q, lv_obj_t* main, lv_obj_t*, lv_obj_t*) {
    int idx = q.comp_index;
    if (idx < 0 || idx >= MAX_COMPONENTS || !s_aimg_buf[idx] || !s_aimg_dsc[idx]) return;
    int fr = c.value;
    if (fr < 0 || fr >= c.aimg_frames) fr = 0;
    lv_img_set_src(main, &s_aimg_dsc[idx][fr]);           // dsc distinct/frame -> refresh garanti
}
```

- [ ] **Step 4 : Ajouter l'entrée à `VIEW[]`**

Après `/* COMP_IMAGE    */ { build_image, nullptr     },` ajouter :

```c
    /* COMP_IMAGE_ANIM */ { build_image_anim, sync_image_anim },
```

- [ ] **Step 5 : Charger l'asset au rebuild + libérer la PSRAM**

Dans la boucle de libération de `view_rebuild` (~ligne 464), après le bloc qui libère `s_img_buf[i]`, ajouter :

```c
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (s_aimg_buf[i]) { heap_caps_free(s_aimg_buf[i]); s_aimg_buf[i] = nullptr; }
        if (s_aimg_dsc[i]) { heap_caps_free(s_aimg_dsc[i]); s_aimg_dsc[i] = nullptr; }
    }
```

Dans la boucle des placements (~ligne 496), après `if (c.type == COMP_IMAGE) img_load_component(d, q.comp_index);` ajouter :

```c
            if (c.type == COMP_IMAGE_ANIM) aimg_load_component(d, q.comp_index);
```

- [ ] **Step 6 : Vérifier que le build esp32s3 redevient vert**

Run (depuis la racine du repo) : `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS (le `static_assert` de `VIEW[]` est satisfait ; compilation OK).

- [ ] **Step 7 : Commit**

```bash
git add src/view.cpp
git commit -m "feat(Rich_Telemetry firmware): rendu lv_img image_anim (load PSRAM + build/sync)"
```

## Task 8 : Brancher le tick dans `loop()`

**Files:**
- Modify: `src/main.cpp` (`loop()` ~ligne 89-103)

- [ ] **Step 1 : Appeler `dash_tick_aimg` avant le `view_sync`**

Dans `loop()`, juste avant la ligne `if (g_dash.values_dirty) view_sync(&g_dash);`, ajouter :

```c
    dash_tick_aimg(&g_dash, now_ms);     // avance les frames des image_anim en lecture (marque dirty)
```

(`now_ms` est déjà calculé plus haut dans `loop()`.)

- [ ] **Step 2 : Build**

Run: `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS.

- [ ] **Step 3 : Commit**

```bash
git add src/main.cpp
git commit -m "feat(Rich_Telemetry firmware): tick image_anim dans loop()"
```

## Task 9 : Endpoints `/aimg` (POST/GET) + sweep des orphelins

**Files:**
- Modify: `src/api.cpp` (sweep dans `h_set_layout` ~ligne 139 ; handlers ~ligne 330 ; routes ~ligne 346)

- [ ] **Step 1 : Ajouter le sweep des packs (après le bloc sweep `/img` ~ligne 139, avant `S->send(200, ...)`)**

```c
    // Sweep : supprime les packs /aimg/*.565p que plus aucun composant image_anim ne reference.
    {
        String victims[16]; int nv = 0;
        File dir = LittleFS.open(AIMG_DIR);
        if (dir && dir.isDirectory()) {
            for (File e = dir.openNextFile(); e && nv < 16; e = dir.openNextFile()) {
                String full = e.name();
                e.close();
                int slash = full.lastIndexOf('/');
                String b = (slash >= 0) ? full.substring(slash + 1) : full;
                if (!b.endsWith(".565p")) continue;
                String key = b.substring(0, b.length() - 5);   // ".565p" = 5 caracteres
                bool referenced = false;
                for (int c = 0; c < D->comp_count; c++)
                    if (D->components[c].type == COMP_IMAGE_ANIM && key.length() &&
                        strcmp(D->components[c].image_src, key.c_str()) == 0) { referenced = true; break; }
                if (!referenced) victims[nv++] = String(AIMG_DIR) + "/" + b;
            }
            dir.close();
        }
        for (int i = 0; i < nv; i++) LittleFS.remove(victims[i]);
    }
```

- [ ] **Step 2 : Ajouter les handlers (après `h_image_get` ~ligne 330)**

```c
// --- POST /aimg?key=<hex> : upload d'un pack image animee RGB565A8 (N frames concatenees) ---
static File   s_aimg_up;
static size_t s_aimg_written = 0;
static const char* AIMG_TMP = AIMG_DIR "/_upload.tmp";

static void h_aimg_upload() {
    HTTPUpload& up = S->upload();
    if (up.status == UPLOAD_FILE_START) {
        if (!LittleFS.exists(AIMG_DIR)) LittleFS.mkdir(AIMG_DIR);
        s_aimg_written = 0;
        s_aimg_up = LittleFS.open(AIMG_TMP, "w");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_aimg_up) s_aimg_written += s_aimg_up.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_aimg_up) s_aimg_up.close();
    }
}
static void h_aimg_done() {
    String key = S->arg("key");
    // Borne (<= AIMG_MAX_BYTES) + multiple de 3 (RGB565A8). Validation forte (== N*w*h*3) au chargement.
    if (s_aimg_written == 0 || s_aimg_written > (size_t)AIMG_MAX_BYTES || (s_aimg_written % AIMG_PX_BYTES) != 0) {
        LittleFS.remove(AIMG_TMP);
        S->send(400, "text/plain", "bad size\n"); return;
    }
    if (!bg_key_valid(key.c_str())) {
        LittleFS.remove(AIMG_TMP);
        S->send(400, "text/plain", "bad key\n"); return;
    }
    String dst = String(AIMG_DIR) + "/" + key + ".565p";
    LittleFS.remove(dst);
    if (!LittleFS.rename(AIMG_TMP, dst)) {
        LittleFS.remove(AIMG_TMP);
        S->send(500, "text/plain", "FS rename failed\n"); return;
    }
    S->send(200, "application/json", "{\"ok\":true}\n");
}
static void h_aimg_get() {
    String key = S->arg("key");
    if (!bg_key_valid(key.c_str())) { S->send(400, "text/plain", "bad key\n"); return; }
    String path = String(AIMG_DIR) + "/" + key + ".565p";
    File f = LittleFS.open(path, "r");
    if (!f) { S->send(404, "text/plain", "not found\n"); return; }
    S->streamFile(f, "application/octet-stream");
    f.close();
}
```

- [ ] **Step 3 : Enregistrer les routes (après les routes `/image` ~ligne 346)**

```c
    server.on("/aimg", HTTP_POST, h_aimg_done, h_aimg_upload);
    server.on("/aimg", HTTP_GET,  h_aimg_get);
```

- [ ] **Step 4 : Build**

Run: `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS.

- [ ] **Step 5 : Commit**

```bash
git add src/api.cpp
git commit -m "feat(Rich_Telemetry firmware): endpoints /aimg (upload/get) + sweep orphelins"
```

---

# Phase 3 — Designer

## Task 10 : Schéma `comp_image_anim`

**Files:**
- Modify: `schema/layout.schema.json` (def `comp_image` ~ligne 207 ; `component.oneOf` ~ligne 82)
- Modify: `data/schema/layout.schema.json` (copie servie)
- Test: `designer/tests/schema.test.js`, `designer/tests/registry.test.js` (conformité, déjà existante)

- [ ] **Step 1 : Écrire les tests dans `designer/tests/schema.test.js`**

```js
test('schema : image_anim valide (src/w/h/frames/period/loop/autoplay)', () => {
  const l = base();
  l.components.sp = { type: 'image_anim', src: 'abcd1234', w: 64, h: 64, frames: 6, period: 80, rest_frame: 2, loop: 3, autoplay: true };
  l.pages[0].place.push({ ref: 'sp', anchor: 'CENTER' });
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : image_anim rejette frames > 32', () => {
  const l = base();
  l.components.sp = { type: 'image_anim', src: 'abcd1234', w: 64, h: 64, frames: 99 };
  l.pages[0].place.push({ ref: 'sp', anchor: 'CENTER' });
  assert.equal(validate(l).valid, false);
});
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: FAIL (`image_anim` inconnu du schéma → `additionalProperties`/`oneOf` rejette, ou le 2e test passe par accident — vérifier le 1er échoue).

- [ ] **Step 3 : Ajouter la def `comp_image_anim` (après la def `comp_image`, avant `"page"`)**

```json
    "comp_image_anim": {
      "type": "object",
      "additionalProperties": false,
      "required": ["type"],
      "description": "Image animee : pack RGB565A8 multi-frames (N frames brutes concatenees), rasterise par le navigateur a w×h, uploade via POST /aimg?key=. Runtime via /update : {\"frame\":K} | {\"play\":true,\"loop\":L,\"period\":ms} | {\"stop\":true}. bind = frame d'etat au repos (clamp 0..frames-1). Place via anchor/dx/dy.",
      "properties": {
        "type": { "const": "image_anim" },
        "src": { "$ref": "#/$defs/ascii", "description": "Cle d'asset (hash FNV-1a du pack). Absente tant qu'aucune animation n'est choisie." },
        "w": { "type": "integer", "minimum": 1, "maximum": 360, "description": "Largeur d'une frame (px) = largeur de l'asset." },
        "h": { "type": "integer", "minimum": 1, "maximum": 360, "description": "Hauteur d'une frame (px) = hauteur de l'asset." },
        "frames": { "type": "integer", "minimum": 1, "maximum": 32, "description": "Nombre de frames du pack. Borne AIMG_MAX_FRAMES=32." },
        "period": { "type": "integer", "minimum": 1, "description": "Temps inter-frame par defaut (ms). Defaut 100. Surchargable via /update." },
        "rest_frame": { "type": "integer", "minimum": 0, "description": "Frame affichee au repos / apres un play fini. Defaut 0." },
        "loop": { "type": "integer", "minimum": 0, "description": "Nb de passes par defaut d'un play. 0 = infini (jusqu'a stop). Defaut 0." },
        "autoplay": { "type": "boolean", "description": "Demarre la lecture au chargement de la page. Defaut false." },
        "bind": { "$ref": "#/$defs/ascii", "description": "Variable du contexte (pull) : selectionne la frame d'etat (clamp 0..frames-1) quand l'anim est a l'arret." }
      }
    },
```

- [ ] **Step 4 : Ajouter la ref à `component.oneOf` (après `{ "$ref": "#/$defs/comp_image" }`)**

```json
        { "$ref": "#/$defs/comp_image_anim" }
```

(Penser à la virgule sur la ligne `comp_image` précédente.)

- [ ] **Step 5 : Synchroniser la copie servie**

Run: `cp devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json devices/guition_knob/projects/Rich_Telemetry/data/schema/layout.schema.json`

- [ ] **Step 6 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (schema.test.js ; registry.test.js reste rouge tant que le registre n'a pas `image_anim` — Task 12 — c'est attendu).

- [ ] **Step 7 : Commit**

```bash
git add schema/layout.schema.json data/schema/layout.schema.json designer/tests/schema.test.js
git commit -m "feat(Rich_Telemetry schema): type image_anim"
```

## Task 11 : Module d'asset `image-anim-asset.js` (décodage + pack)

**Files:**
- Create: `designer/js/image-anim-asset.js`
- Create: `designer/tests/image-anim-asset.test.js`

- [ ] **Step 1 : Écrire les tests purs (sans canvas/ImageDecoder)**

`designer/tests/image-anim-asset.test.js` :

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { packFrames, referencedAimgKeys } from '../js/image-anim-asset.js';

test('packFrames : concatene les frames et compte N', () => {
  const f0 = new Uint8Array([1, 2, 3]);
  const f1 = new Uint8Array([4, 5, 6]);
  const { bytes, frames } = packFrames([f0, f1]);
  assert.equal(frames, 2);
  assert.deepEqual([...bytes], [1, 2, 3, 4, 5, 6]);
});

test('packFrames : cle stable pour le meme contenu', () => {
  const a = packFrames([new Uint8Array([1, 2, 3])]).key;
  const b = packFrames([new Uint8Array([1, 2, 3])]).key;
  assert.equal(a, b);
  assert.match(a, /^[0-9a-f]{16}$/);
});

test('referencedAimgKeys : src des composants image_anim, dedupliques', () => {
  const state = { components: {
    a: { type: 'image_anim', src: 'aaaa' },
    b: { type: 'image_anim', src: 'aaaa' },   // doublon
    c: { type: 'image_anim' },                // pas de src
    d: { type: 'image', src: 'zzzz' },        // pas une anim
  } };
  assert.deepEqual(referencedAimgKeys(state).sort(), ['aaaa']);
});
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: FAIL (`image-anim-asset.js` n'existe pas).

- [ ] **Step 3 : Créer `designer/js/image-anim-asset.js`**

```js
// Conversion GIF/serie d'images -> pack RGB565A8 multi-frames (N frames brutes concatenees) pour le
// composant image_anim. Le NAVIGATEUR decode/rasterise/convertit ; le device n'affiche que du
// RGB565A8 deja pret (cf. view.cpp aimg_load_component). Reutilise la conversion d'image-asset.js et
// le hash de bg-image.js (meme device, meme contrat). previewUrl(key, frame) sert l'apercu.
import { SWAP, fnv1a64Hex } from './bg-image.js';
import { rgba8888ToRgb565a8, rgb565a8ToRgba8888 } from './image-asset.js';

// --- Pur (testable hors navigateur) : assemble des octets de frames deja convertis en pack + cle. ---
export function packFrames(frameBytesList) {
  const total = frameBytesList.reduce((s, f) => s + f.length, 0);
  const pack = new Uint8Array(total);
  let off = 0;
  for (const f of frameBytesList) { pack.set(f, off); off += f.length; }
  return { key: fnv1a64Hex(pack), bytes: pack, frames: frameBytesList.length };
}

export function referencedAimgKeys(state) {
  return [...new Set(Object.values(state.components || {})
    .map(c => (c && c.type === 'image_anim') ? c.src : null).filter(Boolean))];
}

// --- Cache d'apercu (navigateur). cle -> { bytes, frames, w, h, urls:[dataURL/frame] }. ---
const _cache = new Map();
export function packBytes(key)  { return _cache.get(key)?.bytes  || null; }
export function frameCount(key) { return _cache.get(key)?.frames || 0; }
export function previewUrls(key){ return _cache.get(key)?.urls   || []; }
export function previewUrl(key, frame = 0) {
  const e = _cache.get(key);
  return e ? (e.urls[frame] || e.urls[0] || null) : null;
}

function frameDataUrl(bytes, off, w, h) {
  const cnv = document.createElement('canvas'); cnv.width = w; cnv.height = h;
  const ctx = cnv.getContext('2d');
  const img = ctx.createImageData(w, h);
  img.data.set(rgb565a8ToRgba8888(bytes.subarray(off, off + w * h * 3), SWAP));
  ctx.putImageData(img, 0, 0);
  return cnv.toDataURL();
}
function cachePack(key, bytes, frames, w, h) {
  const fb = w * h * 3;
  const urls = [];
  for (let i = 0; i < frames; i++) urls.push(frameDataUrl(bytes, i * fb, w, h));
  _cache.set(key, { bytes, frames, w, h, urls });
}

// Rasterise un drawable (VideoFrame/ImageBitmap) a w×h (etire) -> octets RGB565A8 (1 frame).
function frameToBytes(drawable, w, h) {
  const cnv = document.createElement('canvas'); cnv.width = w; cnv.height = h;
  const ctx = cnv.getContext('2d');
  ctx.clearRect(0, 0, w, h);
  ctx.drawImage(drawable, 0, 0, w, h);
  return rgba8888ToRgb565a8(ctx.getImageData(0, 0, w, h).data, SWAP);
}

// Assemble des drawables en pack -> { key, bytes, frames, w, h } + cache d'apercu.
export function framesToAsset(drawables, w, h) {
  const list = drawables.map(d => { const b = frameToBytes(d, w, h); d.close?.(); return b; });
  const { key, bytes, frames } = packFrames(list);
  cachePack(key, bytes, frames, w, h);
  return { key, bytes, frames, w, h };
}

// Decode un GIF anime -> { drawables, periodMs } (periode = moyenne des durees de frames du GIF).
export async function decodeGif(file) {
  const dec = new ImageDecoder({ data: await file.arrayBuffer(), type: file.type || 'image/gif' });
  await dec.tracks.ready;
  const count = dec.tracks.selectedTrack?.frameCount || 1;
  const drawables = []; let totalUs = 0;
  for (let i = 0; i < count; i++) {
    const { image } = await dec.decode({ frameIndex: i });
    drawables.push(image);
    totalUs += (image.duration || 0);            // microsecondes
  }
  const periodMs = (count > 0 && totalUs > 0) ? Math.round(totalUs / count / 1000) : 100;
  return { drawables, periodMs };
}

// Plusieurs fichiers image -> { drawables, periodMs } (tries par nom).
export async function decodeImages(files) {
  const sorted = [...files].sort((a, b) => a.name.localeCompare(b.name));
  const drawables = [];
  for (const f of sorted) drawables.push(await createImageBitmap(f));
  return { drawables, periodMs: 100 };
}

// Rehydrate depuis le device (pack brut) -> reconstruit le cache d'apercu.
export function rehydrate(key, bytes, w, h, frames) { cachePack(key, bytes, frames, w, h); }
```

- [ ] **Step 4 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (image-anim-asset.test.js ; registry.test.js encore rouge — Task 12).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/image-anim-asset.js designer/tests/image-anim-asset.test.js
git commit -m "feat(Rich_Telemetry designer): image-anim-asset (decode GIF/images -> pack RGB565A8)"
```

## Task 12 : Registre + aperçu `buildImageAnim`

**Files:**
- Modify: `designer/js/registry.js` (import ~ligne 7 ; entrée `image` ~ligne 86)
- Modify: `designer/js/render.js` (import ~ligne 5 ; après `buildImage` ~ligne 340)
- Test: `designer/tests/registry.test.js` (conformité, existante)

- [ ] **Step 1 : Ajouter `buildImageAnim` à l'import de `registry.js`**

Remplacer la ligne d'import depuis `render.js` par :

```js
import { buildLabel, buildReadout, buildBar, buildRing, buildChart, buildMeter, buildImage, buildImageAnim } from './render.js';
```

- [ ] **Step 2 : Ajouter l'entrée `image_anim` à `COMPONENTS` (après l'entrée `image`)**

```js
  image_anim: {
    label: 'Image animée',
    defaults: () => ({ type: 'image_anim', w: 120, h: 120, period: 100, rest_frame: 0, loop: 0, autoplay: false }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['src', 'Animation', 'image_anim'], ['period', 'Période (ms)', 'num'],
                 ['rest_frame', 'Frame repos', 'num'], ['loop', 'Boucles (0=∞)', 'num'],
                 ['autoplay', 'Autoplay', 'bool'], ['bind', 'Variable (pull)', 'asciitext']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
    mockFields: [],
    build: (comp) => buildImageAnim(comp),
  },
```

(`frames` n'est PAS dans `defaults()` : un composant fraîchement déposé n'a pas encore d'asset → `frames` absent reste valide au schéma. Il sera posé à l'upload.)

- [ ] **Step 3 : Ajouter l'import de l'aperçu dans `render.js`**

Sous la ligne `import { previewUrl } from './image-asset.js';` ajouter :

```js
import { previewUrl as aimgPreviewUrl } from './image-anim-asset.js';
```

- [ ] **Step 4 : Ajouter `buildImageAnim` (après `buildImage`)**

```js
export function buildImageAnim(comp) {
  const wrap = document.createElement('div');
  wrap.className = 'w w-image';
  wrap.style.width  = (comp.w || 120) + 'px';
  wrap.style.height = (comp.h || 120) + 'px';
  // Apercu statique = frame de repos (parite avec le device a l'arret).
  const url = comp.src ? aimgPreviewUrl(comp.src, comp.rest_frame || 0) : null;
  if (url) {
    const img = document.createElement('img');
    img.className = 'w-image-img';
    img.src = url;
    img.style.width = '100%'; img.style.height = '100%';
    img.style.display = 'block'; img.style.objectFit = 'fill';
    wrap.appendChild(img);
  } else {
    wrap.classList.add('w-image--empty');
  }
  return wrap;
}
```

- [ ] **Step 5 : Lancer — conformité du registre verte**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (registry.test.js : `image_anim` ∈ registre ∩ schéma).

- [ ] **Step 6 : Commit**

```bash
git add designer/js/registry.js designer/js/render.js
git commit -m "feat(Rich_Telemetry designer): registre image_anim + apercu buildImageAnim"
```

## Task 13 : Transport device (`uploadAimg`/`fetchAimg`) + upload/rehydrate (`app.js`)

**Files:**
- Modify: `designer/js/device.js` (après `fetchImage` ~ligne 90)
- Modify: `designer/js/app.js` (imports ~ligne 4-6 ; rehydrate ~ligne 153 ; upload ~ligne 173)

- [ ] **Step 1 : Ajouter `uploadAimg`/`fetchAimg` à `device.js`**

```js
// POST /aimg?key=<hex> : upload d'un pack image animee RGB565A8 (multipart, streame en LittleFS).
export async function uploadAimg(base, key, bytes) {
  const fd = new FormData();
  fd.append('img', new Blob([bytes], { type: 'application/octet-stream' }), key + '.565p');
  const r = await fetch(clean(base) + '/aimg?key=' + encodeURIComponent(key), { method: 'POST', body: fd });
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json().catch(() => ({}));
}

// GET /aimg?key=<hex> : recupere les octets du pack (Uint8Array), ou null si 404.
export async function fetchAimg(base, key) {
  const r = await fetch(clean(base) + '/aimg?key=' + encodeURIComponent(key));
  if (r.status === 404) return null;
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return new Uint8Array(await r.arrayBuffer());
}
```

- [ ] **Step 2 : Étendre les imports de `app.js`**

Ajouter `uploadAimg, fetchAimg` à l'import depuis `./device.js` (ligne 4), et après l'import d'`image-asset.js` (ligne 6) ajouter :

```js
import { referencedAimgKeys, packBytes as aimgPackBytes, previewUrl as aimgPreviewUrl, rehydrate as rehydrateAimg } from './image-anim-asset.js';
```

- [ ] **Step 3 : Rehydrater les packs au chargement**

Dans la boucle de rehydrate (après le bloc `if (ic.type !== 'image' ...) ... rehydrateImage(...)`, ~ligne 156), ajouter dans la même boucle sur les composants :

```js
        if (ic.type === 'image_anim' && ic.src && ic.w > 0 && ic.h > 0 && ic.frames > 0 && !aimgPreviewUrl(ic.src)) {
          const b = await fetchAimg(base, ic.src);
          if (b) rehydrateAimg(ic.src, b, ic.w, ic.h, ic.frames);
        }
```

- [ ] **Step 4 : Uploader les packs avant `pushLayout`**

Après la boucle `for (const k of referencedImageKeys(model.state)) { ... uploadImage(...) }` (~ligne 175), ajouter :

```js
      for (const k of referencedAimgKeys(model.state)) {
        const bytes = aimgPackBytes(k);
        if (bytes) await uploadAimg(base, k, bytes);   // avant pushLayout (le sweep tourne au POST /layout)
      }
```

- [ ] **Step 5 : Vérifier au navigateur (pas de test unitaire — DOM/fetch)**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (aucune régression ; ces chemins sont vérifiés on-device en Task 16).

- [ ] **Step 6 : Commit**

```bash
git add designer/js/device.js designer/js/app.js
git commit -m "feat(Rich_Telemetry designer): upload/rehydrate des packs /aimg"
```

## Task 14 : Éditeur de frames (inspecteur)

**Files:**
- Modify: `designer/js/inspector.js` (imports ~ligne 6 ; dispatch des champs ~ligne 243 ; helper bespoke près d'`imageField` ~ligne 223)

- [ ] **Step 1 : Étendre les imports de l'inspecteur**

Après `import { imageFileToAsset, previewUrl as imagePreviewUrl } from './image-asset.js';` ajouter :

```js
import { decodeGif, decodeImages, framesToAsset, previewUrls as aimgPreviewUrls } from './image-anim-asset.js';
```

- [ ] **Step 2 : Ajouter le helper `imageAnimField` (après `imageField`, avant `function render()`)**

```js
  // Champ « Animation » d'un composant image_anim : import GIF/serie d'images -> pack, bande de
  // vignettes (choix de la frame de repos), bouton Apercu (anime le canvas). Convertit au navigateur
  // a la taille COURANTE du composant (c.w×c.h). Commit en bloc : src + frames + w/h (+ period si GIF).
  let _aimgPreviewTimer = null;
  function imageAnimField(label, c) {
    const wrap = document.createElement('div'); wrap.className = 'insp-aimg';
    const row = document.createElement('div'); row.className = 'insp-row';
    const span = document.createElement('span'); span.className = 'insp-label'; span.textContent = label;
    row.appendChild(span);
    const file = document.createElement('input');
    file.type = 'file'; file.accept = 'image/*'; file.multiple = true; file.className = 'insp-bg-file';
    file.addEventListener('change', async () => {
      const fs = file.files; if (!fs || !fs.length) return;
      try {
        const w = c.w || 120, h = c.h || 120;
        const isGif = fs.length === 1 && /gif$/i.test(fs[0].type || fs[0].name);
        const { drawables, periodMs } = isGif ? await decodeGif(fs[0]) : await decodeImages([...fs]);
        const { key, frames } = framesToAsset(drawables, w, h);
        model.commit(st => {
          setComponentProp(st, sel.ref, 'src', key);
          setComponentProp(st, sel.ref, 'frames', frames);
          setComponentProp(st, sel.ref, 'w', w);
          setComponentProp(st, sel.ref, 'h', h);
          if (isGif) setComponentProp(st, sel.ref, 'period', periodMs);
          if ((c.rest_frame || 0) >= frames) setComponentProp(st, sel.ref, 'rest_frame', 0);
        });
      } catch (e) { console.error('image_anim:', e); }
      file.value = '';
    });
    row.appendChild(file);
    if (c.src) {
      const del = document.createElement('button');
      del.type = 'button'; del.className = 'insp-bg-reset'; del.textContent = '↺';
      del.title = "Retirer l'animation";
      del.addEventListener('click', () => model.commit(st => {
        setComponentProp(st, sel.ref, 'src', null);
        setComponentProp(st, sel.ref, 'frames', null);
      }));
      row.appendChild(del);
    }
    wrap.appendChild(row);
    // Bande de vignettes : clic = choisir la frame de repos (surlignee).
    const urls = c.src ? aimgPreviewUrls(c.src) : [];
    if (urls.length) {
      const strip = document.createElement('div'); strip.className = 'insp-aimg-strip';
      urls.forEach((u, i) => {
        const t = document.createElement('img'); t.className = 'insp-aimg-frame'; t.src = u;
        if (i === (c.rest_frame || 0)) t.classList.add('is-rest');
        t.title = 'Frame ' + i + ' — clic = frame de repos';
        t.addEventListener('click', () => model.commit(st => setComponentProp(st, sel.ref, 'rest_frame', i)));
        strip.appendChild(t);
      });
      wrap.appendChild(strip);
      // Bouton Apercu : anime le widget du canvas (hors modele) en honorant c.period.
      const play = document.createElement('button');
      play.type = 'button'; play.className = 'insp-aimg-play'; play.textContent = '▶ Aperçu';
      play.addEventListener('click', () => {
        if (_aimgPreviewTimer) { clearInterval(_aimgPreviewTimer); _aimgPreviewTimer = null; play.textContent = '▶ Aperçu'; return; }
        const node = canvasNodeFor && canvasNodeFor(sel.ref);   // <img> du widget (cf. Step 3)
        const imgEl = node ? node.querySelector('.w-image-img') : null;
        if (!imgEl) return;
        let f = 0;
        play.textContent = '⏸ Aperçu';
        _aimgPreviewTimer = setInterval(() => {
          f = (f + 1) % urls.length;
          imgEl.src = urls[f];
        }, Math.max(20, c.period || 100));
      });
      wrap.appendChild(play);
    }
    return wrap;
  }
```

- [ ] **Step 3 : Fournir `canvasNodeFor` (accès au nœud DOM du widget sélectionné)**

L'aperçu anime le `<img>` du widget rendu sur le canvas. Vérifier d'abord si l'inspecteur dispose déjà d'un accès au nœud du widget (chercher `rerenderCanvas`, `getWidgetNode`, ou un registre `sel.ref -> node` dans `canvas.js`). Deux cas :

- **S'il existe déjà** un getter de nœud par `ref`, l'utiliser : remplacer `canvasNodeFor && canvasNodeFor(sel.ref)` par l'appel réel.
- **Sinon**, se rabattre sur une requête CSS dans le conteneur du canvas. Remplacer la ligne `const node = canvasNodeFor && canvasNodeFor(sel.ref);` par :

```js
        const node = document.querySelector(`.canvas-stage [data-ref="${sel.ref}"]`);
```

(Vérifier le sélecteur réel des widgets dans `canvas.js` : classe du conteneur de scène et attribut portant le `ref`. Adapter `.canvas-stage`/`[data-ref=...]` à ce qui existe. Si les widgets ne portent pas le `ref` en attribut, ajouter `el.dataset.ref = ref;` à l'endroit où `canvas.js` crée le nœud du widget — modif minimale, une ligne.)

- [ ] **Step 4 : Brancher le champ dans le dispatch**

Dans `render()`, à côté de `if (kind === 'image') { body.appendChild(imageField(label, c)); continue; }` (~ligne 243), ajouter :

```js
      if (kind === 'image_anim') { body.appendChild(imageAnimField(label, c)); continue; }   // editeur bespoke
```

- [ ] **Step 5 : Vérifier au navigateur (manuel)**

Ouvrir le designer (`designer/index.html` en local, ou `http://<ip>/designer/`), déposer un `image_anim`, importer un GIF : la bande de vignettes apparaît, le clic change la frame de repos (surlignée), « ▶ Aperçu » anime le widget. `node --test` doit rester vert (pas de régression).

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS.

- [ ] **Step 6 : Commit**

```bash
git add designer/js/inspector.js designer/js/canvas.js designer/style.css
git commit -m "feat(Rich_Telemetry designer): editeur de frames image_anim (import GIF/serie, frame repos, apercu)"
```

> Note style : ajouter au besoin les règles CSS `.insp-aimg-strip`/`.insp-aimg-frame.is-rest`/`.insp-aimg-play` dans `designer/style.css` (miroir des classes `insp-bg-*` existantes : `.is-rest` = bordure d'accent).

## Task 15 : Limites `image_anim` (validation)

**Files:**
- Modify: `designer/js/validate.js` (bloc des limites firmware ~ligne 27-37)
- Test: `designer/tests/schema.test.js` (validateur sémantique)

- [ ] **Step 1 : Écrire le test**

```js
test('validate : image_anim au-dela du plafond memoire -> erreur', () => {
  const l = base();
  // 360*360*3*8 = 3 110 400 octets > 1 572 864
  l.components.sp = { type: 'image_anim', src: 'abcd1234', w: 360, h: 360, frames: 8 };
  l.pages[0].place.push({ ref: 'sp', anchor: 'CENTER' });
  const r = validate(l);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => /pack trop gros|trop de frames/.test(e)));
});
```

- [ ] **Step 2 : Lancer — échec attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: FAIL (aucune limite mémoire `image_anim` → `valid` reste true).

- [ ] **Step 3 : Ajouter les gardes (après les contrôles `LIM` de pages/placements, avant le calcul des warnings)**

```js
    // Limites image_anim (config.h : AIMG_MAX_FRAMES=32, AIMG_MAX_BYTES=1572864).
    Object.entries(layout?.components || {}).forEach(([id, c]) => {
      if (!c || c.type !== 'image_anim') return;
      if (c.frames > 32) errors.push(`composant « ${id} » : trop de frames (${c.frames}, max 32)`);
      const bytes = (c.w || 0) * (c.h || 0) * 3 * (c.frames || 0);
      if (bytes > 1572864) errors.push(`composant « ${id} » : pack trop gros (${bytes} o, max 1572864)`);
    });
```

- [ ] **Step 4 : Lancer — succès attendu**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS.

- [ ] **Step 5 : Commit**

```bash
git add designer/js/validate.js designer/tests/schema.test.js
git commit -m "feat(Rich_Telemetry designer): gardes de limites image_anim (frames/octets)"
```

---

# Phase 4 — Documentation + validation on-device

## Task 16 : Section du manuel `docs/index.html`

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/docs/index.html`

- [ ] **Step 1 : Repérer la section du composant `image` (statique)**

Run: `grep -n -i 'image' devices/guition_knob/projects/Rich_Telemetry/docs/index.html | head`
Lire la section « image » et l'entrée de sommaire (TOC) correspondantes pour calquer la structure (mêmes balises/classes).

- [ ] **Step 2 : Ajouter une section `image_anim` calquée sur celle de `image`**

En miroir exact du balisage de la section `image`, ajouter une section « Image animée (`image_anim`) » couvrant :
- **Source** : GIF animé (découpé par le navigateur, période importée) ou série d'images (triées par nom) ; conversion RGB565A8, pack mono-fichier `/aimg/<clé>.565p`.
- **Champs** : `src`, `w`, `h`, `frames`, `period` (ms), `rest_frame`, `loop` (0 = ∞), `autoplay`, `bind`.
- **Runtime `/update`** : `{"<id>":{"frame":K}}` (va à K, stoppe) · `{"<id>":{"play":true,"loop":L,"period":ms}}` (joue) · `{"<id>":{"stop":true}}` (stoppe → frame de repos).
- **`bind`** : variable = frame d'état au repos (clamp `0..frames-1`) ; ignorée pendant un play.
- **Limites** : `AIMG_MAX_FRAMES=32`, pack ≤ ~1,5 Mo/composant.

Ajouter aussi l'entrée correspondante dans le sommaire (TOC), comme pour `image`.

- [ ] **Step 3 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/docs/index.html
git commit -m "docs(Rich_Telemetry): manuel — section du composant image animee"
```

## Task 17 : Validation on-device (utilisateur, à distance)

> Le device (Guition) est branché au Mac de l'utilisateur, joignable en WiFi par IP directe. L'agent ne flashe pas ; il fournit les commandes et l'utilisateur exécute / renvoie les captures. (Mémoire : pas de `timeout` macOS ; capture `GET /screenshot` → `sips` PNG → envoi.)

- [ ] **Step 1 : Flasher firmware + FS**

```bash
cd /Users/.../Waveshare-ESP32
tools/stage_fs.sh guition_knob Rich_Telemetry          # stage designer/ + schema/ dans data/
./build.sh auto Rich_Telemetry --upload                # ou ./build.sh guition_knob Rich_Telemetry --upload
./build.sh guition_knob Rich_Telemetry --uploadfs      # LittleFS (designer embarque + schema)
```

- [ ] **Step 2 : Créer un layout avec un `image_anim` via le designer embarqué**

Ouvrir `http://<ip>/designer/`, déposer un `image_anim`, importer un GIF (≤ 200 px, ≤ 20 frames), régler `period`, « Pousser » le layout (upload du pack `/aimg` + `POST /layout`).

- [ ] **Step 3 : Vérifier l'asset et le rendu**

```bash
IP=192.168.1.35
curl -s "http://$IP/layout" | python3 -m json.tool | grep -A8 image_anim    # champs présents
curl -s "http://$IP/screenshot" -o /tmp/aimg.bmp && sips -s format png /tmp/aimg.bmp --out /tmp/aimg.png
```
Envoyer `/tmp/aimg.png` (frame de repos affichée).

- [ ] **Step 4 : Vérifier frame / play / stop / bind**

```bash
ID=sp     # id du composant
curl -s -X POST "http://$IP/update" -H 'Content-Type: application/json' -d "{\"$ID\":{\"frame\":3}}"      # va frame 3
curl -s -X POST "http://$IP/update" -H 'Content-Type: application/json' -d "{\"$ID\":{\"play\":true,\"loop\":0,\"period\":80}}"   # boucle
curl -s -X POST "http://$IP/update" -H 'Content-Type: application/json' -d "{\"$ID\":{\"stop\":true}}"     # stop -> repos
curl -s -X POST "http://$IP/context" -H 'Content-Type: application/json' -d "{\"st\":1}"                    # si bind=st : frame 1
```
Capturer `/screenshot` après chaque pour confirmer (l'animation est visible à l'œil ; le screenshot fige une frame).

- [ ] **Step 5 : Vérifier le sweep d'orphelins**

Changer l'image de l'`image_anim` (nouvelle clé), re-pousser, puis :
```bash
curl -s "http://$IP/aimg?key=<ancienne_cle>" -o /dev/null -w '%{http_code}\n'   # attendu 404 (balaye)
```

- [ ] **Step 6 : Checkpoint final**

Confirmer : rendu OK, play/frame/stop/bind OK, autoplay OK (page rechargée), sweep OK, `pio test -e native` + `node --test` verts, build esp32s3 vert.

---

## Auto-revue du plan (effectuée)

- **Couverture de la spec** : §1 données/format → Tasks 1,2,3,9 ; §2 runtime (tick/update/bind/autoplay) → Tasks 3,4,5,6,7,8 ; §3 designer → Tasks 10-15 ; §4 portée/tests/limites → tests dans chaque task + Tasks 15,17 ; manuel → Task 16. La checklist cross-couches de la spec (11 points) est couverte par les Tasks 1-16.
- **Placeholders** : aucun TODO/TBD ; les seuls renvois sont vers des fonctions existantes à calquer (montrées) — pas vers d'autres tasks.
- **Cohérence des types** : `image_anim`/`COMP_IMAGE_ANIM`/`comp_image_anim` ; champs `aimg_frames/aimg_period/aimg_rest/aimg_loop/aimg_autoplay` + `aimg_playing/aimg_period_ms/aimg_loops_left/aimg_last_ms` ; clé/dims = `image_src/image_w/image_h` (réutilisés) ; `previewUrl(key, frame)` ; `/aimg` + `.565p` — uniformes sur tout le plan.
- **Point ouvert assumé** (Task 14 Step 3) : l'accès au nœud DOM du widget pour l'aperçu animé dépend d'un détail de `canvas.js` à vérifier ; deux replis fournis (getter existant, sinon sélecteur CSS + `dataset.ref` minimal). Seule incertitude résiduelle, isolée et documentée.
