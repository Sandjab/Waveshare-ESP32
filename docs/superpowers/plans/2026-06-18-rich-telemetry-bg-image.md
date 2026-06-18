# Rich_Telemetry — Image de fond par page : plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Donner à chaque page du dashboard une image de fond optionnelle (override par page, comme la couleur de l'étape 1), convertie en RGB565 par le navigateur et uploadée en LittleFS, sans aucun décodage sur le MCU.

**Architecture:** Le designer (JS) décode/redimensionne n'importe quelle image en 360×360 RGB565 (fit `cover`), calcule une clé = hash de contenu (FNV-1a 64 bits), la met en cache pour l'aperçu, l'uploade au device (`POST /bgimage?key=`, multipart streamé en LittleFS) puis pousse le layout qui référence la clé via `pages[].background_image`. Le firmware charge le fichier `/bg/<clé>.565` en PSRAM et le pose en `bg_img` sur le conteneur de page (même ancrage que la couleur). Un sweep au `POST /layout` supprime les `/bg/*.565` orphelins.

**Tech Stack:** ESP32-S3 / Arduino (pioarduino) · LVGL 8.4 (`LV_COLOR_DEPTH=16`, **`LV_COLOR_16_SWAP=1`**) · LittleFS · ArduinoJson 7 · WebServer (Arduino) · designer ES modules + `node --test` · Unity (`pio test -e native`).

**Répertoire projet :** `devices/guition_knob/projects/Rich_Telemetry/` (toutes les commandes ci-dessous s'exécutent depuis là, sauf mention contraire).

**Spec :** `docs/superpowers/specs/2026-06-18-rich-telemetry-bg-image-design.md`.

**Contrainte mémoire connue :** la conversion RGBA→RGB565 doit produire des octets dans l'ordre attendu par LVGL avec `LV_COLOR_16_SWAP=1`. Le plan part de **octets big-endian (octet fort en premier)** comme défaut, derrière un drapeau `SWAP=true`. La **Task 14** valide on-device et bascule le drapeau (changement d'une ligne) si les couleurs sortent fausses.

**Note commits :** chaque commit termine par la ligne `Claude-Session: https://claude.ai/code/session_01Dx1wnFnR1mzCbAHVJa2N3H` (convention du repo). Les messages ci-dessous omettent cette ligne pour la lisibilité.

---

## Récapitulatif des fichiers touchés

**Firmware** (`src/`)
- `config.h` — Modify : constantes `BG_IMG_W/H/BYTES`, `BG_DIR`.
- `dashboard.h` — Modify : champ `Page::background_image` + déclaration `bg_key_valid`.
- `dashboard.cpp` — Modify : implémentation `bg_key_valid` + parse de `background_image`.
- `view.cpp` — Modify : chargement PSRAM + `bg_img` sur le conteneur de page + lifecycle (free au rebuild).
- `api.cpp` — Modify : routes `POST/GET /bgimage`, handlers d'upload, sweep des orphelins dans `h_set_layout`.
- `test/test_core/test_main.cpp` — Modify : tests natifs `bg_key_valid` + parse `background_image`.

**Schéma**
- `schema/layout.schema.json` — Modify : `pages[].background_image` (optionnel).

**Designer** (`designer/`)
- `js/bg-image.js` — Create : helpers purs (coverRect, RGBA↔RGB565, FNV-1a 64) + glue navigateur (conversion fichier, cache d'aperçu).
- `js/device.js` — Modify : `uploadBgImage`, `fetchBgImage`.
- `js/mutations.js` — Modify : `setPageBackgroundImage`, `effectivePageBgImage`.
- `js/inspector.js` — Modify : contrôle « Image de fond » dans le panneau page.
- `js/canvas.js` — Modify : aperçu de l'image en fond du stage (prime sur la couleur).
- `js/app.js` — Modify : upload des images référencées au « Pousser », fetch au « Charger ».
- `tests/bg-image.test.js` — Create : tests des helpers purs.
- `tests/mutations.test.js` — Modify : tests `setPageBackgroundImage` / `effectivePageBgImage`.

---

## Task 1 : Schéma — `pages[].background_image`

**Files:**
- Modify: `schema/layout.schema.json` (bloc `$defs.page`)

- [ ] **Step 1 : Ajouter la propriété au `$def` page**

Localiser le `$def` `page` dans `schema/layout.schema.json` (il contient déjà `name`, `background`, `place`). Ajouter, à côté de `background`, la propriété :

```json
"background_image": { "$ref": "#/$defs/ascii", "description": "Cle d'asset image de fond (hash de contenu, hex). Bytes RGB565 360x360 uploades via POST /bgimage?key=. Absent = pas d'image (la couleur de fond s'applique)." }
```

- [ ] **Step 2 : Vérifier que rien ne casse (designer)**

Run: `cd designer && node --test`
Expected: PASS, même total qu'avant (la propriété est optionnelle, aucun test existant ne la rejette).

- [ ] **Step 3 : Vérifier que rien ne casse (firmware natif, le test charge le schéma)**

Run: `pio test -e native`
Expected: PASS (`test_schema_types_all_resolve` inclus).

- [ ] **Step 4 : Commit**

```bash
git add schema/layout.schema.json
git commit -m "feat(Rich_Telemetry): schema pages[].background_image (etape 2)"
```

---

## Task 2 : Firmware — `bg_key_valid` (pur, testé en natif)

Garde anti-traversée de chemin : une clé valide est 1..16 caractères hex minuscules (`0-9a-f`). Tout le reste (vide, `/`, `.`, majuscules, trop long) est rejeté. Placée dans `dashboard.cpp` (compilé en natif → testable) car c'est une clé d'asset de layout.

**Files:**
- Modify: `src/dashboard.h` (déclaration)
- Modify: `src/dashboard.cpp` (implémentation)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Écrire les tests qui échouent**

Dans `test/test_core/test_main.cpp`, ajouter (par ex. après le groupe `test_hex_*`) :

```c
void test_bgkey_valid_hex(void)      { TEST_ASSERT_TRUE(bg_key_valid("a1b2c3d4e5f60718")); }   // 16 hex
void test_bgkey_valid_short(void)    { TEST_ASSERT_TRUE(bg_key_valid("0")); }
void test_bgkey_reject_empty(void)   { TEST_ASSERT_FALSE(bg_key_valid("")); }
void test_bgkey_reject_slash(void)   { TEST_ASSERT_FALSE(bg_key_valid("../x")); }
void test_bgkey_reject_dot(void)     { TEST_ASSERT_FALSE(bg_key_valid("a.b")); }
void test_bgkey_reject_upper(void)   { TEST_ASSERT_FALSE(bg_key_valid("ABCD")); }
void test_bgkey_reject_toolong(void) { TEST_ASSERT_FALSE(bg_key_valid("00112233445566778")); } // 17
```

Et enregistrer dans `main()` (à côté des `RUN_TEST(test_hex_*)`) :

```c
    RUN_TEST(test_bgkey_valid_hex);
    RUN_TEST(test_bgkey_valid_short);
    RUN_TEST(test_bgkey_reject_empty);
    RUN_TEST(test_bgkey_reject_slash);
    RUN_TEST(test_bgkey_reject_dot);
    RUN_TEST(test_bgkey_reject_upper);
    RUN_TEST(test_bgkey_reject_toolong);
```

- [ ] **Step 2 : Lancer les tests pour les voir échouer**

Run: `pio test -e native`
Expected: FAIL de compilation (`bg_key_valid` non déclaré).

- [ ] **Step 3 : Déclarer dans `dashboard.h`**

Ajouter près des autres prototypes libres (après la ligne `int dash_find(...)`) :

```c
bool bg_key_valid(const char* key);   // clé d'asset image de fond : 1..16 hex minuscules (garde de chemin)
```

- [ ] **Step 4 : Implémenter dans `dashboard.cpp`**

Ajouter en haut du fichier (après les includes / fonctions utilitaires existantes) :

```cpp
bool bg_key_valid(const char* key) {
    if (!key || !key[0]) return false;
    size_t n = 0;
    for (const char* p = key; *p; p++, n++) {
        if (n >= 16) return false;
        char c = *p;
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!hex) return false;
    }
    return true;
}
```

- [ ] **Step 5 : Lancer les tests**

Run: `pio test -e native`
Expected: PASS (tous les `test_bgkey_*` verts, plus les anciens).

- [ ] **Step 6 : Commit**

```bash
git add src/dashboard.h src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry): bg_key_valid (garde de cle d'asset, teste natif)"
```

---

## Task 3 : Firmware — parse `background_image` dans `Page`

**Files:**
- Modify: `src/dashboard.h` (champ `Page::background_image`)
- Modify: `src/dashboard.cpp` (parse dans la boucle pages)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Écrire le test qui échoue**

Dans `test/test_core/test_main.cpp`, ajouter (après `test_page_background_override_and_inherit`) :

```c
void test_page_background_image_parsed(void) {
    Dashboard d = {}; char err[80];
    static const char* LAYOUT_BGI =
      "{\"background\":\"#000000\",\"components\":{\"x\":{\"type\":\"label\",\"text\":\"hi\"}},"
      "\"pages\":[{\"name\":\"a\",\"background_image\":\"abc123\",\"place\":[]},"
                 "{\"name\":\"b\",\"place\":[]},"
                 "{\"name\":\"c\",\"background_image\":\"../evil\",\"place\":[]}]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_BGI, err, sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("abc123", d.pages[0].background_image);  // clé valide conservée
    TEST_ASSERT_EQUAL_STRING("",       d.pages[1].background_image);  // absente → vide
    TEST_ASSERT_EQUAL_STRING("",       d.pages[2].background_image);  // invalide → rejetée (vide)
}
```

Enregistrer dans `main()` (après `RUN_TEST(test_page_background_override_and_inherit);`) :

```c
    RUN_TEST(test_page_background_image_parsed);
```

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `pio test -e native`
Expected: FAIL de compilation (`Page` n'a pas de membre `background_image`).

- [ ] **Step 3 : Ajouter le champ dans `dashboard.h`**

Dans `struct Page` (juste après `uint32_t background;`) :

```c
    char      background_image[ID_LEN];   // clé d'asset (hash) ; vide = pas d'image (la couleur s'applique)
```

- [ ] **Step 4 : Parser dans `dashboard.cpp`**

Dans la boucle `for (JsonObjectConst pg : pages)`, juste après la ligne `p.background = parse_hex_color(...)`, ajouter :

```cpp
        const char* bgimg = pg["background_image"] | "";
        strlcpy(p.background_image, bg_key_valid(bgimg) ? bgimg : "", sizeof(p.background_image));
```

- [ ] **Step 5 : Lancer les tests**

Run: `pio test -e native`
Expected: PASS.

- [ ] **Step 6 : Commit**

```bash
git add src/dashboard.h src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry): parse pages[].background_image (override par page)"
```

---

## Task 4 : Firmware — constantes image (`config.h`)

**Files:**
- Modify: `src/config.h`

- [ ] **Step 1 : Ajouter les constantes**

Après les `#define` de chemins (`LAYOUT_PATH`, `SECRETS_PATH`) :

```c
#define BG_IMG_W       360
#define BG_IMG_H       360
#define BG_IMG_BYTES   (BG_IMG_W * BG_IMG_H * 2)   // RGB565 plein ecran = 259200
#define BG_DIR         "/bg"                        // repertoire LittleFS des fonds
```

- [ ] **Step 2 : Vérifier la compilation cible**

Run: `pio run -e esp32s3`
Expected: SUCCESS (constantes inertes pour l'instant).

- [ ] **Step 3 : Commit**

```bash
git add src/config.h
git commit -m "feat(Rich_Telemetry): constantes BG_IMG_* + BG_DIR"
```

---

## Task 5 : Firmware — rendu de l'image + lifecycle (`view.cpp`)

Esp-only (non compilé en natif) → validé on-device en Task 14. Charge `/bg/<clé>.565` en PSRAM, pose en `bg_img` sur le conteneur de page (par-dessus la couleur), et libère les buffers à chaque rebuild.

**Files:**
- Modify: `src/view.cpp`

- [ ] **Step 1 : Ajouter les includes + le registre PSRAM**

En tête de `src/view.cpp`, ajouter aux includes :

```cpp
#include <LittleFS.h>
#include "esp_heap_caps.h"
#include "config.h"
```

Sous les autres `static lv_obj_t* s_*` (après `static lv_obj_t* s_dots = nullptr;`) :

```cpp
static uint8_t*     s_bg_buf[MAX_PAGES] = {0};   // RGB565 en PSRAM par page (nullptr = pas d'image)
static lv_img_dsc_t s_bg_dsc[MAX_PAGES];
```

- [ ] **Step 2 : Helper de chargement (fonction statique, avant `view_rebuild`)**

```cpp
// Charge /bg/<cle>.565 en PSRAM et remplit s_bg_dsc[p]. false si pas de cle / fichier absent /
// mauvaise taille / alloc ratee -> on retombe alors sur la couleur de fond (deja posee).
static bool bg_load_page(Dashboard* d, int p) {
    const char* key = d->pages[p].background_image;
    if (!key[0]) return false;
    char path[40];
    snprintf(path, sizeof(path), "%s/%s.565", BG_DIR, key);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (f.size() != BG_IMG_BYTES) { f.close(); return false; }
    uint8_t* buf = (uint8_t*)heap_caps_malloc(BG_IMG_BYTES, MALLOC_CAP_SPIRAM);
    if (!buf) { f.close(); return false; }
    size_t rd = f.read(buf, BG_IMG_BYTES);
    f.close();
    if (rd != BG_IMG_BYTES) { heap_caps_free(buf); return false; }
    s_bg_buf[p] = buf;
    lv_img_dsc_t& dsc = s_bg_dsc[p];
    memset(&dsc, 0, sizeof(dsc));
    dsc.header.cf  = LV_IMG_CF_TRUE_COLOR;
    dsc.header.w   = BG_IMG_W;
    dsc.header.h   = BG_IMG_H;
    dsc.data       = buf;
    dsc.data_size  = BG_IMG_BYTES;
    return true;
}
```

- [ ] **Step 3 : Libérer au début du rebuild**

Dans `view_rebuild`, juste après `lv_obj_clean(scr);` (les objets qui référençaient les dsc sont alors détruits) :

```cpp
    for (int i = 0; i < MAX_PAGES; i++) {
        if (s_bg_buf[i]) { heap_caps_free(s_bg_buf[i]); s_bg_buf[i] = nullptr; }
    }
```

- [ ] **Step 4 : Poser l'image sur le conteneur de page**

Dans la boucle `for (int p = 0; p < d->page_count; p++)`, juste après les deux lignes `lv_obj_set_style_bg_color(cont, ...)` / `lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0)` :

```cpp
        if (bg_load_page(d, p))
            lv_obj_set_style_bg_img_src(cont, &s_bg_dsc[p], 0);   // image par-dessus la couleur
```

- [ ] **Step 5 : Compiler**

Run: `pio run -e esp32s3`
Expected: SUCCESS.

- [ ] **Step 6 : Commit**

```bash
git add src/view.cpp
git commit -m "feat(Rich_Telemetry): rendu image de fond par page (PSRAM + bg_img) + lifecycle"
```

---

## Task 6 : Firmware — endpoints `/bgimage` + sweep des orphelins (`api.cpp`)

Esp-only → validé via curl en Task 14. Upload multipart streamé (le binaire à octets nuls exclut `arg("plain")`), validation de taille (259200), `rename` du temp vers `/bg/<clé>.565`. Sweep dans `h_set_layout`.

**Files:**
- Modify: `src/api.cpp`

- [ ] **Step 1 : État d'upload + handlers (avant `api_register`)**

Ajouter (après le bloc `h_screenshot`, avant `void api_register`) :

```cpp
// --- POST /bgimage?key=<hex> : upload d'un fond RGB565 (360x360, 259200 octets) ---
// Multipart streame directement en LittleFS (pas de gros buffer RAM, supporte les octets nuls).
// Ecrit dans un fichier temp puis renomme vers /bg/<cle>.565 si la taille est exacte.
static File   s_bg_up;
static size_t s_bg_written = 0;
static const char* BG_TMP = BG_DIR "/_upload.tmp";

static void h_bgimage_upload() {
    HTTPUpload& up = S->upload();
    if (up.status == UPLOAD_FILE_START) {
        if (!LittleFS.exists(BG_DIR)) LittleFS.mkdir(BG_DIR);
        s_bg_written = 0;
        s_bg_up = LittleFS.open(BG_TMP, "w");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_bg_up) s_bg_written += s_bg_up.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_bg_up) s_bg_up.close();
    }
}

static void h_bgimage_done() {
    String key = S->arg("key");
    if (s_bg_written != BG_IMG_BYTES) {
        LittleFS.remove(BG_TMP);
        S->send(400, "text/plain", "bad size (expected 259200)\n"); return;
    }
    if (!bg_key_valid(key.c_str())) {
        LittleFS.remove(BG_TMP);
        S->send(400, "text/plain", "bad key\n"); return;
    }
    String dst = String(BG_DIR) + "/" + key + ".565";
    LittleFS.remove(dst);                       // rename echoue si la cible existe
    if (!LittleFS.rename(BG_TMP, dst)) {
        LittleFS.remove(BG_TMP);
        S->send(500, "text/plain", "FS rename failed\n"); return;
    }
    S->send(200, "application/json", "{\"ok\":true}\n");
}

static void h_bgimage_get() {
    String key = S->arg("key");
    if (!bg_key_valid(key.c_str())) { S->send(400, "text/plain", "bad key\n"); return; }
    String path = String(BG_DIR) + "/" + key + ".565";
    File f = LittleFS.open(path, "r");
    if (!f) { S->send(404, "text/plain", "not found\n"); return; }
    S->streamFile(f, "application/octet-stream");
    f.close();
}
```

- [ ] **Step 2 : Enregistrer les routes (dans `api_register`)**

Après `server.on("/screenshot", HTTP_GET, h_screenshot);` :

```cpp
    server.on("/bgimage", HTTP_POST, h_bgimage_done, h_bgimage_upload);  // done + upload handler
    server.on("/bgimage", HTTP_GET,  h_bgimage_get);
```

- [ ] **Step 3 : Sweep des orphelins dans `h_set_layout`**

Dans `h_set_layout`, juste avant `S->send(200, "application/json", "{\"ok\":true}\n");` (après le `persist_save` réussi) :

```cpp
    // Sweep : supprime les fonds /bg/*.565 que plus aucune page ne reference.
    {
        String victims[16]; int nv = 0;
        File dir = LittleFS.open(BG_DIR);
        if (dir && dir.isDirectory()) {
            for (File e = dir.openNextFile(); e && nv < 16; e = dir.openNextFile()) {
                String full = e.name();                 // peut etre "/bg/<x>.565" ou "<x>.565" selon le core
                e.close();
                int slash = full.lastIndexOf('/');
                String base = (slash >= 0) ? full.substring(slash + 1) : full;
                if (!base.endsWith(".565")) continue;   // ignore _upload.tmp et autres
                String key = base.substring(0, base.length() - 4);
                bool referenced = false;
                for (int p = 0; p < D->page_count; p++)
                    if (key.length() && strcmp(D->pages[p].background_image, key.c_str()) == 0) { referenced = true; break; }
                if (!referenced) victims[nv++] = String(BG_DIR) + "/" + base;
            }
            dir.close();
        }
        for (int i = 0; i < nv; i++) LittleFS.remove(victims[i]);
    }
```

- [ ] **Step 4 : Compiler**

Run: `pio run -e esp32s3`
Expected: SUCCESS.

- [ ] **Step 5 : Commit**

```bash
git add src/api.cpp
git commit -m "feat(Rich_Telemetry): endpoints POST/GET /bgimage + sweep des orphelins au POST /layout"
```

---

## Task 7 : Designer — helpers purs (`js/bg-image.js` + tests node)

Module nouveau. Les fonctions de cette task sont **pures** (pas de DOM/canvas) → testées sous `node --test`. La glue navigateur (createImageBitmap/canvas/cache) arrive en Task 9 dans le même fichier, sans s'exécuter à l'import.

**Files:**
- Create: `designer/js/bg-image.js`
- Create: `designer/tests/bg-image.test.js`

- [ ] **Step 1 : Écrire les tests qui échouent**

Créer `designer/tests/bg-image.test.js` :

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { coverRect, rgba8888ToRgb565, rgb565ToRgba8888, fnv1a64Hex, SWAP } from '../js/bg-image.js';

test('coverRect : source paysage → crop horizontal centré (carré cible)', () => {
  assert.deepEqual(coverRect(800, 600, 360, 360), { sx: 100, sy: 0, sw: 600, sh: 600 });
});
test('coverRect : source portrait → crop vertical centré (carré cible)', () => {
  assert.deepEqual(coverRect(600, 800, 360, 360), { sx: 0, sy: 100, sw: 600, sh: 600 });
});
test('coverRect : déjà au bon ratio → pleine source', () => {
  assert.deepEqual(coverRect(360, 360, 360, 360), { sx: 0, sy: 0, sw: 360, sh: 360 });
});

// SWAP=true (LV_COLOR_16_SWAP=1) : octet fort en premier.
test('rgba→565 : blanc opaque', () => {
  assert.deepEqual([...rgba8888ToRgb565(new Uint8ClampedArray([255,255,255,255]), true)], [0xFF, 0xFF]);
});
test('rgba→565 : rouge (swap → hi,lo)', () => {
  assert.deepEqual([...rgba8888ToRgb565(new Uint8ClampedArray([255,0,0,255]), true)], [0xF8, 0x00]);
});
test('rgba→565 : vert', () => {
  assert.deepEqual([...rgba8888ToRgb565(new Uint8ClampedArray([0,255,0,255]), true)], [0x07, 0xE0]);
});
test('rgba→565 : bleu', () => {
  assert.deepEqual([...rgba8888ToRgb565(new Uint8ClampedArray([0,0,255,255]), true)], [0x00, 0x1F]);
});
test('rgba→565 : sans swap = octets inversés', () => {
  assert.deepEqual([...rgba8888ToRgb565(new Uint8ClampedArray([255,0,0,255]), false)], [0x00, 0xF8]);
});

test('565→rgba : round-trip rouge (canaux reconstruits)', () => {
  const back = rgb565ToRgba8888(new Uint8Array([0xF8, 0x00]), true);
  assert.deepEqual([...back], [248, 0, 0, 255]);   // 31<<3 = 248
});

test('fnv1a64Hex : vecteur connu "a" → af63dc4c8601ec8c', () => {
  assert.equal(fnv1a64Hex(new Uint8Array([0x61])), 'af63dc4c8601ec8c');
});
test('fnv1a64Hex : déterministe et 16 hex', () => {
  const h = fnv1a64Hex(new Uint8Array([1,2,3,4]));
  assert.match(h, /^[0-9a-f]{16}$/);
  assert.equal(h, fnv1a64Hex(new Uint8Array([1,2,3,4])));
});

test('SWAP par défaut = true (LV_COLOR_16_SWAP=1)', () => {
  assert.equal(SWAP, true);
});
```

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `cd designer && node --test`
Expected: FAIL (`bg-image.js` introuvable).

- [ ] **Step 3 : Créer `designer/js/bg-image.js` (partie pure)**

```js
// Conversion image -> RGB565 pour les fonds de page. Le NAVIGATEUR fait tout le decodage
// (createImageBitmap + canvas) ; le device ne stocke/affiche que du RGB565 deja pret.
// LV_COLOR_16_SWAP=1 sur le device => octets ranges octet-fort-en-premier (SWAP=true).

export const BG_W = 360, BG_H = 360, BG_BYTES = BG_W * BG_H * 2;
export const SWAP = true;   // LV_COLOR_16_SWAP=1 ; bascule a false si Task 14 montre des couleurs fausses

// Rectangle source pour un fit "cover" (remplit dst, crop centre). Retourne {sx,sy,sw,sh} entiers.
export function coverRect(srcW, srcH, dstW, dstH) {
  const targetAspect = dstW / dstH;
  if (srcW / srcH > targetAspect) {           // source trop large -> crop horizontal
    const sw = Math.round(srcH * targetAspect);
    return { sx: Math.round((srcW - sw) / 2), sy: 0, sw, sh: srcH };
  }
  const sh = Math.round(srcW / targetAspect); // source trop haute -> crop vertical
  return { sx: 0, sy: Math.round((srcH - sh) / 2), sw: srcW, sh };
}

// RGBA8888 (Uint8ClampedArray, 4 octets/pixel) -> RGB565 (Uint8Array, 2 octets/pixel).
export function rgba8888ToRgb565(rgba, swap = SWAP) {
  const px = rgba.length >> 2;
  const out = new Uint8Array(px * 2);
  for (let i = 0, o = 0; i < px; i++) {
    const r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2];
    const v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    if (swap) { out[o++] = (v >> 8) & 0xFF; out[o++] = v & 0xFF; }
    else      { out[o++] = v & 0xFF;        out[o++] = (v >> 8) & 0xFF; }
  }
  return out;
}

// RGB565 (Uint8Array) -> RGBA8888 (Uint8ClampedArray), pour reconstruire un apercu.
export function rgb565ToRgba8888(bytes, swap = SWAP) {
  const px = bytes.length >> 1;
  const out = new Uint8ClampedArray(px * 4);
  for (let i = 0, o = 0; i < px; i++) {
    const b0 = bytes[i * 2], b1 = bytes[i * 2 + 1];
    const v = swap ? (b0 << 8) | b1 : (b1 << 8) | b0;
    const r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
    out[o++] = (r5 << 3) | (r5 >> 2);
    out[o++] = (g6 << 2) | (g6 >> 4);
    out[o++] = (b5 << 3) | (b5 >> 2);
    out[o++] = 255;
  }
  return out;
}

// FNV-1a 64 bits -> 16 hex minuscules. BigInt pour l'exactitude. Cle d'asset = hash du contenu RGB565.
export function fnv1a64Hex(u8) {
  const PRIME = 0x100000001b3n, MASK = 0xFFFFFFFFFFFFFFFFn;
  let h = 0xcbf29ce484222325n;
  for (let i = 0; i < u8.length; i++) {
    h ^= BigInt(u8[i]);
    h = (h * PRIME) & MASK;
  }
  return h.toString(16).padStart(16, '0');
}
```

- [ ] **Step 4 : Lancer les tests**

Run: `cd designer && node --test`
Expected: PASS (tout `bg-image.test.js` vert, le reste inchangé).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/bg-image.js designer/tests/bg-image.test.js
git commit -m "feat(Rich_Telemetry designer): helpers purs bg-image (coverRect, RGBA<->565, FNV-1a)"
```

---

## Task 8 : Designer — mutations `setPageBackgroundImage` / `effectivePageBgImage`

**Files:**
- Modify: `designer/js/mutations.js`
- Test: `designer/tests/mutations.test.js`

- [ ] **Step 1 : Écrire les tests qui échouent**

Dans `designer/tests/mutations.test.js`, ajouter à l'import (ligne ~7) `setPageBackgroundImage, effectivePageBgImage`, puis ajouter ces tests (après le bloc `effectivePageBg`) :

```js
test('setPageBackgroundImage : pose la clé', () => {
  const s = fresh();
  setPageBackgroundImage(s, 0, 'abc123');
  assert.equal(s.pages[0].background_image, 'abc123');
});

test('setPageBackgroundImage : vide/null supprime la clé', () => {
  const s = fresh(); s.pages[0].background_image = 'abc123';
  setPageBackgroundImage(s, 0, null);
  assert.equal('background_image' in s.pages[0], false);
});

test('setPageBackgroundImage : index invalide → no-op (pas de throw)', () => {
  const s = fresh();
  assert.doesNotThrow(() => setPageBackgroundImage(s, 9, 'abc123'));
});

test('effectivePageBgImage : clé de la page', () => {
  const s = { pages: [{ name: 'P1', place: [], background_image: 'abc123' }] };
  assert.equal(effectivePageBgImage(s, 0), 'abc123');
});

test('effectivePageBgImage : sans clé → null (pas de fond image global)', () => {
  const s = { pages: [{ name: 'P1', place: [] }] };
  assert.equal(effectivePageBgImage(s, 0), null);
});
```

(`fresh()` existe déjà en tête de `mutations.test.js` — réutiliser tel quel.)

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `cd designer && node --test`
Expected: FAIL (`setPageBackgroundImage`/`effectivePageBgImage` non exportés).

- [ ] **Step 3 : Implémenter dans `mutations.js`**

Juste après `setPageBackground` (ligne ~91) :

```js
// Clé d'image de fond effective d'une page (override par page uniquement ; pas de fond image global).
export function effectivePageBgImage(state, pageIndex) {
  return state.pages?.[pageIndex]?.background_image || null;
}

// Définit/supprime la clé d'image de fond d'une page. Vide/null → supprime (pas d'image).
export function setPageBackgroundImage(state, pageIndex, key) {
  const page = state.pages?.[pageIndex];
  if (!page) return;
  if (key) page.background_image = key;
  else delete page.background_image;
}
```

- [ ] **Step 4 : Lancer les tests**

Run: `cd designer && node --test`
Expected: PASS.

- [ ] **Step 5 : Commit**

```bash
git add designer/js/mutations.js designer/tests/mutations.test.js
git commit -m "feat(Rich_Telemetry designer): mutations setPageBackgroundImage / effectivePageBgImage"
```

---

## Task 9 : Designer — glue navigateur (conversion fichier + cache) dans `bg-image.js`

Partie navigateur du module (createImageBitmap, canvas, cache d'aperçu). Non testée sous node (APIs DOM) ; ne s'exécute qu'à l'appel, pas à l'import.

**Files:**
- Modify: `designer/js/bg-image.js`

- [ ] **Step 1 : Ajouter le cache + la conversion en fin de `bg-image.js`**

```js
// --- Cache d'apercu (navigateur). cle -> { bytes: Uint8Array RGB565, url: dataURL }. ---
// Non persiste : au rechargement de page, repeuple via fetchBgImage depuis le device (cf. app.js).
const _cache = new Map();

export function cacheBytes(key) { return _cache.get(key)?.bytes || null; }
export function previewUrl(key) { return _cache.get(key)?.url || null; }
export function referencedKeys(state) {
  return [...new Set((state.pages || []).map(p => p.background_image).filter(Boolean))];
}

// Construit un dataURL d'apercu depuis des octets RGB565 et range le couple dans le cache.
export function cachePut(key, bytes) {
  const cnv = document.createElement('canvas'); cnv.width = BG_W; cnv.height = BG_H;
  const ctx = cnv.getContext('2d');
  const img = ctx.createImageData(BG_W, BG_H);
  img.data.set(rgb565ToRgba8888(bytes, SWAP));
  ctx.putImageData(img, 0, 0);
  _cache.set(key, { bytes, url: cnv.toDataURL() });
}

// Fichier image -> { key, bytes }. Decode via le navigateur, recadre en cover 360x360,
// convertit en RGB565, hashe, met en cache. Tout decodage de format se fait ici, cote navigateur.
export async function imageFileToBg(file) {
  const bmp = await createImageBitmap(file);
  const { sx, sy, sw, sh } = coverRect(bmp.width, bmp.height, BG_W, BG_H);
  const cnv = document.createElement('canvas'); cnv.width = BG_W; cnv.height = BG_H;
  const ctx = cnv.getContext('2d');
  ctx.drawImage(bmp, sx, sy, sw, sh, 0, 0, BG_W, BG_H);
  bmp.close?.();
  const rgba = ctx.getImageData(0, 0, BG_W, BG_H).data;
  const bytes = rgba8888ToRgb565(rgba, SWAP);
  const key = fnv1a64Hex(bytes);
  cachePut(key, bytes);
  return { key, bytes };
}
```

- [ ] **Step 2 : Vérifier que les tests purs passent toujours (import non cassé)**

Run: `cd designer && node --test`
Expected: PASS (l'ajout n'exécute aucune API DOM à l'import).

- [ ] **Step 3 : Commit**

```bash
git add designer/js/bg-image.js
git commit -m "feat(Rich_Telemetry designer): glue navigateur bg-image (conversion fichier + cache apercu)"
```

---

## Task 10 : Designer — REST `uploadBgImage` / `fetchBgImage` (`device.js`)

**Files:**
- Modify: `designer/js/device.js`

- [ ] **Step 1 : Ajouter les deux fonctions en fin de `device.js`**

```js
// POST /bgimage?key=<hex> : upload d'un fond RGB565 (multipart, streame cote device en LittleFS).
export async function uploadBgImage(base, key, bytes) {
  const fd = new FormData();
  fd.append('img', new Blob([bytes], { type: 'application/octet-stream' }), key + '.565');
  const r = await fetch(clean(base) + '/bgimage?key=' + encodeURIComponent(key), { method: 'POST', body: fd });
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json().catch(() => ({}));
}

// GET /bgimage?key=<hex> : recupere les octets RGB565 (Uint8Array), ou null si 404.
export async function fetchBgImage(base, key) {
  const r = await fetch(clean(base) + '/bgimage?key=' + encodeURIComponent(key));
  if (r.status === 404) return null;
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return new Uint8Array(await r.arrayBuffer());
}
```

- [ ] **Step 2 : Vérifier (pas de régression)**

Run: `cd designer && node --test`
Expected: PASS (`device.js` n'est pas importé par les tests purs ; rien ne casse).

- [ ] **Step 3 : Commit**

```bash
git add designer/js/device.js
git commit -m "feat(Rich_Telemetry designer): REST uploadBgImage / fetchBgImage"
```

---

## Task 11 : Designer — contrôle « Image de fond » (`inspector.js`)

**Files:**
- Modify: `designer/js/inspector.js`

- [ ] **Step 1 : Étendre les imports (ligne 4)**

```js
import { setComponentProp, setPlacementProp, setThresholds, removePlacement, setPageBackground, setPageBackgroundImage } from './mutations.js';
import { imageFileToBg, previewUrl } from './bg-image.js';
```

- [ ] **Step 2 : Ajouter le bloc image dans `renderPagePanel`**

Dans `renderPagePanel`, juste après `body.appendChild(row);` qui clôt le bloc « Fond page » (ligne ~95, dans le `if (pg) { … }`), ajouter — toujours dans le `if (pg)` :

```js
      // Image de fond de la page : override optionnel, prime sur la couleur. Conversion + upload
      // au navigateur (cf. bg-image.js) ; la cle (hash) est posee dans le layout, les octets sont
      // pousses au device au « Pousser » (app.js).
      const imgRow = document.createElement('div'); imgRow.className = 'insp-row';
      const imgLabel = document.createElement('span'); imgLabel.className = 'insp-label';
      imgLabel.textContent = 'Image de fond';
      imgRow.appendChild(imgLabel);
      const file = document.createElement('input');
      file.type = 'file'; file.accept = 'image/*'; file.className = 'insp-bg-file';
      file.addEventListener('change', async () => {
        const f = file.files?.[0]; if (!f) return;
        try {
          const { key } = await imageFileToBg(f);
          model.commit(st => setPageBackgroundImage(st, pi, key));
        } catch (e) { console.error('bg image:', e); }
        file.value = '';
      });
      imgRow.appendChild(file);
      if (pg.background_image) {
        const thumb = document.createElement('img');
        thumb.className = 'insp-bg-thumb';
        const u = previewUrl(pg.background_image);
        if (u) thumb.src = u; else thumb.alt = '(recharger depuis le device)';
        imgRow.appendChild(thumb);
        const del = document.createElement('button');
        del.type = 'button'; del.className = 'insp-bg-reset'; del.textContent = '↺';
        del.title = "Retirer l'image";
        del.addEventListener('click', () => model.commit(st => setPageBackgroundImage(st, pi, null)));
        imgRow.appendChild(del);
      }
      body.appendChild(imgRow);
```

- [ ] **Step 3 : CSS minimal (miniature)**

Repérer le fichier CSS du designer (celui qui définit `.insp-bg-hint` / `.insp-bg-reset` — `grep -rl insp-bg-reset designer/`) et y ajouter :

```css
.insp-bg-thumb { width: 40px; height: 40px; object-fit: cover; border-radius: 4px; margin-left: 6px; vertical-align: middle; }
.insp-bg-file  { max-width: 150px; }
```

- [ ] **Step 4 : Vérifier (pas de régression node)**

Run: `cd designer && node --test`
Expected: PASS.

- [ ] **Step 5 : Commit**

```bash
git add designer/js/inspector.js designer/css/*.css
git commit -m "feat(Rich_Telemetry designer): controle Image de fond dans le panneau page"
```

---

## Task 12 : Designer — aperçu de l'image en fond du stage (`canvas.js`)

**Files:**
- Modify: `designer/js/canvas.js`

- [ ] **Step 1 : Étendre les imports (ligne 13)**

```js
import { effectivePageBg, effectivePageBgImage } from './mutations.js';
import { previewUrl } from './bg-image.js';
```

- [ ] **Step 2 : Poser l'image dans `render()`**

Remplacer la ligne 91 :

```js
    stage.style.background = effectivePageBg(model.state, activePage);   // fond de page (override) ou global
```

par :

```js
    stage.style.background = effectivePageBg(model.state, activePage);   // fond de page (override) ou global
    // Image de fond (prime sur la couleur). Apercu depuis le cache ; vide si la cle n'a pas d'octets
    // charges (ex. apres rechargement avant un « Charger » depuis le device) -> la couleur reste visible.
    const bgImgKey = effectivePageBgImage(model.state, activePage);
    const bgImgUrl = bgImgKey ? previewUrl(bgImgKey) : null;
    stage.style.backgroundImage = bgImgUrl ? `url(${bgImgUrl})` : '';
    stage.style.backgroundSize = 'cover';
    stage.style.backgroundPosition = 'center';
```

- [ ] **Step 3 : Vérifier (pas de régression node)**

Run: `cd designer && node --test`
Expected: PASS (`canvas.js` non importé par les tests).

- [ ] **Step 4 : Commit**

```bash
git add designer/js/canvas.js
git commit -m "feat(Rich_Telemetry designer): apercu image de fond sur le stage (prime sur couleur)"
```

---

## Task 13 : Designer — câblage « Pousser » / « Charger » (`app.js`)

Au « Pousser » : uploader les images référencées (présentes en cache) **avant** `pushLayout` (le sweep device s'exécute au `POST /layout`, donc les fichiers doivent exister d'abord). Au « Charger » : récupérer les octets des clés référencées pour repeupler l'aperçu.

**Files:**
- Modify: `designer/js/app.js`

- [ ] **Step 1 : Étendre les imports (ligne 4)**

```js
import { loadLayout, pushLayout, captureScreenshot, getStatus, setDevicePage, pushValues, uploadBgImage, fetchBgImage } from './device.js';
import { referencedKeys, cacheBytes, cachePut, previewUrl } from './bg-image.js';
```

- [ ] **Step 2 : Upload des fonds au « Pousser »**

Dans le handler du bouton « Pousser » (ligne ~144), remplacer :

```js
    try { await pushLayout($('base').value, model.toJSON()); setStatus('Poussé et persisté', 'ok'); }
```

par :

```js
    try {
      const base = $('base').value;
      for (const k of referencedKeys(model.state)) {
        const bytes = cacheBytes(k);
        if (bytes) await uploadBgImage(base, k, bytes);   // avant pushLayout (le sweep tourne au POST /layout)
      }
      await pushLayout(base, model.toJSON());
      setStatus('Poussé et persisté', 'ok');
    }
```

- [ ] **Step 3 : Fetch des fonds au « Charger »**

Dans le handler « Charger » (ligne ~136), remplacer :

```js
    try { model.loadJSON(JSON.stringify(await loadLayout($('base').value))); setStatus('Chargé', 'ok'); }
```

par :

```js
    try {
      const base = $('base').value;
      model.loadJSON(JSON.stringify(await loadLayout(base)));
      for (const k of referencedKeys(model.state)) {
        if (!previewUrl(k)) { const b = await fetchBgImage(base, k); if (b) cachePut(k, b); }
      }
      setStatus('Chargé', 'ok');
    }
```

- [ ] **Step 4 : Vérifier (pas de régression node)**

Run: `cd designer && node --test`
Expected: PASS.

- [ ] **Step 5 : Commit**

```bash
git add designer/js/app.js
git commit -m "feat(Rich_Telemetry designer): upload des fonds au Pousser, fetch au Charger"
```

---

## Task 14 : Validation on-device (bout en bout) + confirmation de l'endianness

Device Guition branché au Mac, IP `192.168.1.35` (vérifier d'abord). Workflow de validation : cf. mémoire `feedback-device-validation-workflow`. Sauvegarder le layout avant, restaurer après.

**Files:** aucun (validation), sauf bascule éventuelle de `SWAP` en Step 6.

- [ ] **Step 1 : Backup du layout device**

Run: `curl --max-time 5 http://192.168.1.35/layout -o /tmp/rt-layout-backup.json && echo OK`
Expected: `OK`, fichier non vide.

- [ ] **Step 2 : Flasher firmware + image LittleFS (designer à jour)**

Run (depuis `devices/guition_knob/projects/Rich_Telemetry/`) :
```bash
./build.sh guition_knob Rich_Telemetry --upload     # ou depuis le repo racine ; device-check inclus
bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs   # designer embarque a jour (champ Image de fond)
```
Expected: upload SUCCESS. ⚠ `uploadfs` écrase le layout persisté et les `/bg/*.565` → re-POST en Step 4.

- [ ] **Step 3 : Fabriquer un fond RGB565 de test (rouge plein) et l'uploader**

```bash
python3 - <<'PY'
# 360x360 rouge plein, RGB565 swap (octet fort en premier) = 0xF8 0x00 par pixel
open('/tmp/red.565','wb').write(b'\xF8\x00' * (360*360))
PY
# clé de test arbitraire (16 hex) — le firmware ne recalcule pas le hash, il stocke sous la clé fournie
curl --max-time 8 -F 'img=@/tmp/red.565;filename=deadbeefdeadbeef.565' \
  'http://192.168.1.35/bgimage?key=deadbeefdeadbeef'
```
Expected: `{"ok":true}`. (Un mauvais nombre d'octets doit renvoyer `400 bad size`.)

- [ ] **Step 4 : Pousser un layout 2 pages — page 0 avec l'image, page 1 sans**

```bash
curl --max-time 8 -X POST http://192.168.1.35/layout -H 'Content-Type: application/json' -d '{
  "background":"#0B0B0F",
  "components":{"t":{"type":"label","text":"BG"}},
  "pages":[
    {"name":"img","background_image":"deadbeefdeadbeef","place":[{"ref":"t","anchor":"CENTER"}]},
    {"name":"plain","place":[{"ref":"t","anchor":"CENTER"}]}
  ]}'
```
Expected: `{"ok":true}`.

- [ ] **Step 5 : Capturer la page 0 et vérifier la couleur**

```bash
curl --max-time 6 -X POST http://192.168.1.35/page -H 'Content-Type: application/json' -d '{"index":0}'
curl --max-time 8 http://192.168.1.35/screenshot -o /tmp/p0.bmp && sips -s format png /tmp/p0.bmp --out /tmp/p0.png
```
Envoyer `/tmp/p0.png` à l'utilisateur (SendUserFile). Attendu : **fond rouge** plein avec le label « BG ».

- [ ] **Step 6 : Verdict endianness**

- Si le fond est **rouge** → `SWAP=true` est correct, rien à faire.
- Si le fond est **bleu/cyan/autre** → l'octet-order est inversé : éditer `designer/js/bg-image.js`, passer `export const SWAP = false;`, **et** régénérer le `.565` de test avec `b'\x00\xF8'` pour re-valider. Puis `cd designer && node --test` (les vecteurs `swap=true`/`swap=false` restent valides), re-stager le designer (`stage_fs.sh` + `uploadfs`), commit :
  ```bash
  git add designer/js/bg-image.js
  git commit -m "fix(Rich_Telemetry designer): octet-order RGB565 confirme on-device"
  ```

- [ ] **Step 7 : Vérifier la page 1 (sans image → couleur héritée)**

```bash
curl --max-time 6 -X POST http://192.168.1.35/page -H 'Content-Type: application/json' -d '{"index":1}'
curl --max-time 8 http://192.168.1.35/screenshot -o /tmp/p1.bmp && sips -s format png /tmp/p1.bmp --out /tmp/p1.png
```
Attendu : fond `#0B0B0F` (sombre), pas d'image. Envoyer `/tmp/p1.png`.

- [ ] **Step 8 : Vérifier le sweep des orphelins**

Re-pousser le layout du Step 4 **sans** `background_image` sur aucune page, puis confirmer que l'image n'est plus servie :
```bash
curl --max-time 8 -X POST http://192.168.1.35/layout -H 'Content-Type: application/json' -d '{
  "background":"#0B0B0F","components":{"t":{"type":"label","text":"BG"}},
  "pages":[{"name":"a","place":[{"ref":"t","anchor":"CENTER"}]}]}'
curl --max-time 6 -s -o /dev/null -w '%{http_code}\n' 'http://192.168.1.35/bgimage?key=deadbeefdeadbeef'
```
Expected: `404` (le sweep a supprimé `/bg/deadbeefdeadbeef.565`).

- [ ] **Step 9 : Restaurer le layout d'origine**

```bash
curl --max-time 8 -X POST http://192.168.1.35/layout -H 'Content-Type: application/json' --data-binary @/tmp/rt-layout-backup.json
```
Expected: `{"ok":true}`.

- [ ] **Step 10 : Test manuel du designer embarqué (aller-retour image)**

Ouvrir `http://192.168.1.35/designer/`, saisir l'IP dans le champ device, sélectionner une page, **Image de fond → Choisir une image** (un PNG/JPEG quelconque) → l'aperçu apparaît sur le stage (cover) → **Pousser** → le device affiche l'image. **Charger** depuis un autre onglet/refresh → l'aperçu se repeuple via `GET /bgimage`. Confirmer visuellement (screenshot).

---

## Self-review (auteur)

**Couverture du spec :**
- Schéma `pages[].background_image` → Task 1. ✓
- Endpoints `POST/GET /bgimage` + validation 259200 + 400 → Task 6. ✓
- Sweep orphelins au `POST /layout` → Task 6 Step 3. ✓
- Rendu firmware (`heap_caps_malloc` PSRAM, `lv_img_dsc_t` TRUE_COLOR 360×360, `bg_img_src`, pas de scaling) + lifecycle (free au rebuild) → Task 5. ✓
- Conversion navigateur (createImageBitmap, cover, RGBA→565) → Tasks 7+9. ✓
- Clé = hash de contenu FNV-1a (non-crypto, contexte HTTP) → Task 7. ✓
- Image > couleur → Tasks 5 (firmware pose l'image par-dessus) + 12 (designer). ✓
- UI designer (choisir/miniature/retirer, aperçu stage) → Tasks 11+12. ✓
- Round-trip GET pour l'aperçu → Tasks 10+13. ✓
- Endianness `LV_COLOR_16_SWAP=1` → défaut `SWAP=true` + validation Task 14. ✓
- Décisions : fit cover (Task 7 coverRect), clé hash (Task 7), sweep (Task 6), image>couleur (Tasks 5/12). ✓
- Contrainte flash 3,46 MB : pas de code, mais Task 6 sweep + dédup par hash limitent l'accumulation. ✓

**Placeholders :** aucun « TBD/TODO » ; tout pas de code montre le code. Le seul renvoi de découverte (`grep -rl insp-bg-reset` pour le fichier CSS, Task 11 Step 3) est une localisation de fichier, pas un placeholder de logique.

**Cohérence des types/noms :** `bg_key_valid` (Tasks 2/3/6), `Page::background_image` (Tasks 3/5/6), `BG_IMG_BYTES`/`BG_DIR` (Tasks 4/5/6), `setPageBackgroundImage`/`effectivePageBgImage` (Tasks 8/11/12), `imageFileToBg`/`cacheBytes`/`cachePut`/`previewUrl`/`referencedKeys`/`SWAP` (Tasks 7/9/11/12/13), `uploadBgImage`/`fetchBgImage` (Tasks 10/13) — cohérents d'une task à l'autre.
