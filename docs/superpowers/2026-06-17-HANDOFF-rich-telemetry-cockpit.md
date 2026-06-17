# HANDOFF — Rich_Telemetry : endpoint screenshot, parité, manuel HTML, designer-cockpit + embarqué

**Date : 2026-06-17.** Document autoporteur pour reprendre après un clear de contexte. Fait suite au
HANDOFF post-P3 (`docs/superpowers/2026-06-17-HANDOFF-rich-telemetry-post-P3.md`).

## TL;DR

Tout est **mergé et poussé sur `origin/master`** (HEAD `4f6aca3` + un commit doc/manuel/handoff par-dessus).
Firmware et designer alignés. Cette session a ajouté, dans l'ordre :

1. **Endpoint `GET /screenshot`** (capture BMP 24-bit de l'écran via `lv_snapshot`) + bouton « Capture écran » designer.
2. **Audit de parité** rendu device↔designer + **6 corrections** du rendu designer pour coller au device.
3. **Manuel HTML** de référence (`devices/guition_knob/projects/Rich_Telemetry/docs/index.html`, style du manuel Iris).
4. **Designer-cockpit + embarqué** (4 phases) : autosave & gardes, boucle device live, servi par le device, doc à jour.

État device : layout « P3 test » restauré ; firmware = dernière version (screenshot + serveStatic) ; LittleFS contient
l'image du designer embarqué. `node --test` **93/93**, `pio run -e esp32s3` SUCCESS, validé on-device (IP `192.168.1.35`).

## Commits de la session (anciens → récents, tous sur `origin/master`)

```
64d44fb feat: endpoint GET /screenshot + bouton designer
1016710 docs: snapshots device via endpoint /screenshot
8e97b3c docs: rapport de parité rendu device vs designer
f0d16c0 fix: aligne le rendu du designer sur le device
2942e7e docs: contrôle de parité après correction
1ceaec5 docs: manuel HTML (style manuel Iris)
5cc636b feat(designer): autosave + gardes de limites + avertissement bind<->sources   (Phase 1)
0e6f97d feat(designer): boucle device (live /update + nav /page + santé /status + capture par page)  (Phase 2)
fb6b026 feat: sert le designer embarqué depuis le device (LittleFS, même origin)        (Phase 3)
4f6aca3 docs(designer): README/HANDOFF à jour                                            (Phase 4)
```

## 1. Endpoint `GET /screenshot` (firmware + designer)

- `src/lv_conf.h` : `LV_USE_SNAPSHOT 1`. `src/api.cpp` : handler `h_screenshot` — `lv_snapshot_take_to_buf` (TRUE_COLOR)
  dans un buffer **PSRAM**, encodé **BMP 24-bit bottom-up BGR** streamé ligne par ligne (`setContentLength`+`sendContent`).
  `w`/`h` lus de `dsc.header` (pas en dur). Sûr (même thread que `lv_timer_handler`).
- Designer : bouton « Capture écran » → overlay (`device.js captureScreenshot`).
- Validé on-device : `curl /screenshot` → `PC bitmap 360×360×24`, rendu pixel-perfect, 10 captures sans fuite.

## 2. Parité device↔designer + corrections rendu

- `render.js` se déclare 2ᵉ implémentation best-effort de `src/view.cpp` — **le device arbitre**.
- Méthode : banc d'essai 1 composant/page poussé au device avec **les mêmes valeurs que les mocks** ; capture device
  (`/screenshot`) vs canvas designer (`#stage`), échelle 1:1. Artefacts : **`snapshots/parity/`** (`device-*.png`,
  `designer-*.png`, `designer-after-*.png`, `parity-report.html` avant, `parity-report-after-fix.html` après).
- **6 fixes** dans `render.js`/`canvas.js`/`style.css` : `pickFontPx` 36/48 (était plafonné à 28) ; `buildRing`
  `center_pct` (était absent) ; pill **centrée verticalement** sur la bande (`top=th/2` dans `render.js` ET `canvas.js`)
  + liseré ; bar label centré + pilule ; `buildChart` panneau+grille+points ; `buildMeter` fond+ticks+chiffres+moyeu.
- Restent des écarts esthétiques mineurs sur chart/meter (widgets natifs LVGL, best-effort assumé).

## 3. Manuel HTML de référence

- **`devices/guition_knob/projects/Rich_Telemetry/docs/index.html`** — autonome (images base64), style du manuel
  **Iris** (`/Users/jean-paulgavini/Documents/Dev/Iris/docs/manual/manuel.html` = gabarit) : thème clair/dark, serif,
  sidebar recherche + TOC scroll-spy, callouts, boutons copier. 10 sections (principes, démarrage, modèle JSON,
  8 composants, push/pull, API REST, designer, capture, limites, exemple).
- Source de génération conservée hors repo : `/tmp/rt-doc-iris.src.html` + script python d'embarquement base64
  (images : `snapshots/parity/device-*.png`, `snapshots/2026-06-17-*.png`, vues designer). Régénéré en fin de session
  pour refléter le designer embarqué + la boucle device.

## 4. Designer-cockpit + embarqué (4 phases)

- **Phase 1 — filet & gardes** (`validate.js`/`json-view.js`/`app.js`/`index.html`/`style.css`) : **autosave**
  localStorage (clé `rt-designer-layout`, restauré au boot via `createModel(saved)`) ; **gardes de limites firmware**
  (pages 8 / composants 32 / placements 12 — erreurs bloquantes) ; **avertissement `bind`↔sources** (non bloquant,
  zone `#warnings` ambre). `node --test` 93/93 (+5 cas).
- **Phase 2 — boucle device** (`device.js`/`app.js`/`index.html`/`style.css`) : `getStatus`/`setDevicePage`/`pushValues`.
  Boutons **« Valeurs test »** (POST /update depuis les mocks via `buildUpdatePayload`), **« Statut »** (GET /status →
  barre `#devbar` avec état des sources), et **nav ◀▶** dans l'overlay de capture (POST /page → recapture). Validé
  on-device (devbar « page 1/7 », « Valeurs poussées (6) », nav → page 2/7 « CPU 42 »).
- **Phase 3 — servi par le device** : `src/api.cpp` `serveStatic("/designer", LittleFS, "/designer")` +
  `"/schema"` (+ `#include <LittleFS.h>`). `platformio.ini` **`board_build.filesystem = littlefs`** (sinon image SPIFFS
  → mismatch). **`tools/stage_fs.sh`** stage `designer/` + `schema/` dans `data/` (index.html → **`index.htm`** car
  serveStatic cherche `index.htm` pour une URL de répertoire). `data/designer` + `data/schema` **gitignorés**
  (`.gitignore` projet). Validé : `http://192.168.1.35/designer/` charge tout (MIME `application/javascript` → ESM OK,
  fetch schema same-origin, UI complète).
- **Phase 4 — doc** : `designer/README.md` + `designer/HANDOFF.md` à jour (CORS résolu, `/update`+`/page` câblés, pill
  corrigée, designer embarqué).

## Rebuild / resume du designer embarqué

À chaque changement du designer, re-stager puis ré-uploader l'image LittleFS (écrase le layout persisté → restaurer
ensuite via `POST /layout`) :

```bash
cd devices/guition_knob/projects/Rich_Telemetry
bash tools/stage_fs.sh                       # data/designer (index.htm) + data/schema
pio run -e esp32s3 -t uploadfs               # build.sh n'implémente PAS --uploadfs : utiliser pio
# le firmware (serveStatic) n'a besoin d'un reflash que si src/ change
```

## Suivis ouverts / choix délibérés (ne pas « corriger »)

- **Pistes designer non retenues cette session** (du brainstorming d'amélioration) : thème clair + responsive du
  designer, raccourcis clavier (Suppr/Cmd+Z), duplication de composant, aperçu animé du led_ring. Toutes faisables,
  faible priorité.
- Choix délibérés firmware inchangés : `sound_comp` timeout-0 non bloquant (à dessein), swipes verticaux ignorés
  (réservés à une future page de config par swipe haut).
- `device.js` ne pousse pas de valeurs pour `label`/`led_ring`/`sound` dans « Valeurs test » (pas de mock scalaire pertinent).

Voir aussi : [[project-rich-telemetry]], [[project-rt-designer]], [[reference-iris-manual-doc-style]],
[[feedback-device-validation-workflow]] (mémoire), et `designer/README.md` (usage à jour).
