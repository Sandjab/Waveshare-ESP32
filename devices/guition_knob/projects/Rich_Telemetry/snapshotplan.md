# Endpoint capture d'écran « pixel perfect » — Rich_Telemetry (Guition K718)

## Context

Le projet **Rich_Telemetry** (`devices/guition_knob/projects/Rich_Telemetry/`) tourne sur
le Guition JC3636K718 (écran rond 360×360, ST77916 QSPI, RGB565). Il expose déjà un serveur
HTTP REST (`api.cpp`, port 80, `guition.local`) et un outil web **designer** pour éditer le
dashboard à distance.

On veut que le device expose un **endpoint HTTP renvoyant une capture pixel-perfect de ce qui
est affiché à l'écran**, afin de vérifier le rendu réel (notamment depuis le designer) sans
avoir le device sous les yeux.

**Contrainte clé :** LVGL est configuré ici en **double buffer partiel** (bandes de 36 px,
`guition_lvgl.h`) ; il n'existe **aucun framebuffer plein écran en RAM** à relire, et la relecture
GRAM du ST77916 en QSPI n'est pas fiable. La solution retenue est donc de **re-rendre l'écran
actif via l'API LVGL `lv_snapshot_take_to_buf`** dans un buffer PSRAM, puis de l'encoder en
**BMP 24-bit** (universellement visualisable, zéro dépendance ajoutée) streamé ligne par ligne.

Pourquoi `lv_snapshot` plutôt qu'un shadow-framebuffer alimenté par le flush_cb :
- pas de coût mémoire/CPU permanent (capture à la demande) ;
- pas de modification de la lib partagée `guition_knob_hw` ;
- rendu identique à l'écran (même arbre de widgets, mêmes styles).

**Sûreté threading :** les handlers `WebServer` sont appelés depuis `server.handleClient()`
dans `loop()`, **le même thread que `lv_timer_handler()`**. Appeler `lv_snapshot_*` depuis un
handler est donc sûr (pas d'accès LVGL concurrent ; `net_pull` tourne sur une autre tâche mais
ne touche que le contexte data, pas les objets LVGL).

## Changes

### 1. Activer l'API snapshot LVGL
`src/lv_conf.h` — ajouter :
```c
#define LV_USE_SNAPSHOT 1
```
(snapshot ne dépend que de `LV_DRAW`/SW renderer, pas de `LV_USE_CANVAS` — vérifié dans
`lv_snapshot.c` : il alloue seulement un petit `draw_ctx`, le gros buffer est fourni par nous.)

### 2. Nouvel endpoint `GET /screenshot` (firmware)
`src/api.cpp` :
- `#include <lvgl.h>` et `#include "esp_heap_caps.h"` en tête.
- Nouveau handler `h_screenshot()` :
  1. `lv_obj_t* scr = lv_scr_act();`
  2. `uint32_t need = lv_snapshot_buf_size_needed(scr, LV_IMG_CF_TRUE_COLOR);` (≈ 360×360×2 = 259 200 o)
  3. Allouer le buffer en **PSRAM** : `heap_caps_malloc(need, MALLOC_CAP_SPIRAM)` (8 MB dispo) ;
     si NULL → `503`.
  4. `lv_snapshot_take_to_buf(scr, LV_IMG_CF_TRUE_COLOR, &dsc, buf, need)` ; si ≠ `LV_RES_OK` → `500` + free.
  5. Encoder **BMP 24-bit bottom-up, BGR**, stride aligné 4 (ici 360×3 = 1080, déjà multiple de 4) :
     - `S->setContentLength(54 + stride*h); S->send(200, "image/bmp", "");`
     - `S->sendContent(header, 54);` (en-tête BITMAPFILEHEADER+BITMAPINFOHEADER construit à la main)
     - boucle `y` de `h-1` à `0`, remplir une ligne de `stride` octets : pour chaque pixel
       `uint32_t c = lv_color_to32(((lv_color_t*)dsc.data)[y*w + x]);` puis B=`c&0xFF`,
       G=`(c>>8)&0xFF`, R=`(c>>16)&0xFF`. (`lv_color_to32` gère correctement `LV_COLOR_16_SWAP=1`.)
     - `S->sendContent(row, stride);`
  6. `heap_caps_free(buf);`
- Enregistrer la route dans `api_register()` : `server.on("/screenshot", HTTP_GET, h_screenshot);`
- Ajouter une mention `/screenshot` dans la page d'aide `h_root()`.

Pic mémoire ≈ 253 Ko PSRAM + 1 Ko ligne ; aucun second buffer plein écran (streaming).

### 3. Bouton + aperçu « Capture écran » dans le designer
- `designer/js/device.js` — nouvelle fonction :
  ```js
  export async function captureScreenshot(base) {
    const r = await fetch(clean(base) + '/screenshot');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return URL.createObjectURL(await r.blob());   // image/bmp
  }
  ```
- `designer/index.html` — ajouter un bouton `<button id="capture">Capture écran</button>`
  dans le `<header>` (à côté de « Charger »/« Pousser ») et une zone d'aperçu
  (`<img id="shot">` dans une `<dialog>`/overlay simple, ou sous le canvas).
- `designer/js/app.js` — importer `captureScreenshot`, câbler `$('capture').onclick`
  (réutilise `$('base').value` + `setStatus(...)` déjà présents ; afficher le blob URL dans
  `#shot`, révoquer l'ancienne URL via `URL.revokeObjectURL` pour éviter les fuites).
- `designer/style.css` — styles minimes pour l'aperçu (overlay + image 360px).

## Critical files
- `devices/guition_knob/projects/Rich_Telemetry/src/lv_conf.h` (flag snapshot)
- `devices/guition_knob/projects/Rich_Telemetry/src/api.cpp` (handler + route + aide)
- `devices/guition_knob/projects/Rich_Telemetry/designer/js/device.js`
- `devices/guition_knob/projects/Rich_Telemetry/designer/index.html`
- `devices/guition_knob/projects/Rich_Telemetry/designer/js/app.js`
- `devices/guition_knob/projects/Rich_Telemetry/designer/style.css`

Réutilisé tel quel : `lv_snapshot_take_to_buf` / `lv_snapshot_buf_size_needed` /
`lv_color_to32` (LVGL 8.4), `WebServer::setContentLength`/`sendContent` (déjà le pattern du
projet), `clean()`/`setStatus()` côté designer.

## Verification
1. **Compilation** : `./build.sh guition_knob Rich_Telemetry` (vérifie que `LV_USE_SNAPSHOT`
   et `lv_snapshot.c` se compilent sous pioarduino).
2. **Flash + run** : `./build.sh guition_knob Rich_Telemetry --upload` (device branché ;
   respecter la procédure d'enrôlement MAC du repo). Au boot, noter l'IP série `[wifi] IP=...`.
3. **Endpoint** :
   ```bash
   curl -s http://<ip>/screenshot -o shot.bmp && file shot.bmp   # "PC bitmap ... 360 x 360 x 24"
   ```
   Ouvrir `shot.bmp` : doit reproduire **exactement** l'écran (texte, jauges, couleurs).
   Changer la page sur le device (encoder) puis recapturer → l'image suit la page active.
4. **Designer** : servir `Rich_Telemetry/` (cf. `designer/README.md`), renseigner l'URL device,
   cliquer **Capture écran** → l'aperçu affiche le rendu courant.
5. **Sanity mémoire** : enchaîner ~10 captures, vérifier en série l'absence de fuite/OOM
   (la heap interne reste stable, le buffer PSRAM est libéré à chaque requête).
