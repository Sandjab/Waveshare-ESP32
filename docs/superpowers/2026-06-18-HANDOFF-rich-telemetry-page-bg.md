# HANDOFF — Rich_Telemetry : fond par page (étape 1 faite) + improvements designer

**Date : 2026-06-18.** Document autoporteur pour reprendre après un clear de contexte. Succède au
HANDOFF cockpit (`docs/superpowers/2026-06-17-HANDOFF-rich-telemetry-cockpit.md`), qu'il **remplace**
(le cockpit avait plusieurs commits de retard).

## TL;DR

Tout est **mergé et poussé sur `origin/master`** (HEAD `51fdc84`, branche synchronisée, arbre propre).
Firmware et designer alignés. Cette session a livré, dans l'ordre : une vague d'améliorations du designer
(raccourcis, guide d'ancrage, renommage d'onglet, anti-doublon, toasts) **puis l'étape 1 de la feature
« fond par page »** (couleur de fond par page). **Reprise prévue : étape 2 — image de fond.**

Tests : designer `node --test` **125/125** ; firmware `pio test -e native` **67/67** ; `pio run -e esp32s3`
SUCCESS. Validé **on-device** (Guition flashé, snapshots).

## Commits de la session (anciens → récents, tous sur `origin/master`)

```
39ce64c feat(designer): raccourcis clavier Suppr + annuler/rétablir
746fc54 feat(designer): guide d'ancrage visuel pendant le drag
f53a63b feat(designer): renommer un onglet par double-clic
4a559f5 feat(designer): noms de page auto sans collision (uniquePageName)
a778e2c feat(designer): renommage d'onglet sans doublon (validation inline + toast)
da571d1 feat(designer): verdicts d'action en toast (échec rouge / succès vert)
51fdc84 feat(Rich_Telemetry): couleur de fond par page (override optionnel)   ← étape 1
```
(Antérieurs mais non couverts par le cockpit : `925b474` chrome, `4937563` drag&drop pages,
`58fe896` icônes palette, `ea98402` zoom canvas, `0150652` swipe transition.)

## Contexte device — IMPORTANT (testable sans le user)

**« User à distance » ≠ « device injoignable ».** Le Guition reste branché au Mac où je tourne :
- MAC `ac:a7:04:ef:74:28` → `knobGuitionNoir1` → `device_dir=guition_knob` (`python3 tools/device_mac.py scan`).
- IP **`192.168.1.35`** (DHCP mais stable ; tester `curl --max-time 4 http://192.168.1.35/status` avant de lire le série).
- **Flash** : `./build.sh guition_knob Rich_Telemetry --upload` (device-check inclus).
- **Snapshot visuel par moi** : `curl --max-time N http://192.168.1.35/screenshot -o x.bmp` (BMP 24-bit 360×360)
  → `sips -s format png x.bmp --out x.png` → envoyer le PNG au user (SendUserFile).
- **Toujours** `GET /layout` (backup) avant de pousser un layout de test, et **restaurer** après.
Voir mémoire [[feedback-device-validation-workflow]] (mise à jour 2026-06-18).

## Étape 1 — couleur de fond PAR PAGE (FAIT, `51fdc84`)

**Modèle : override optionnel.** Couleur effective d'une page = `page.background ?? layout.background ?? #000000`.
Layouts existants inchangés (aucune page n'a le champ → tout hérite du global, comportement d'avant).

- **Schéma** (`schema/layout.schema.json`) : `pages[].background` optionnel (`$ref #/$defs/hexColor`).
- **Designer** :
  - `js/mutations.js` : `setPageBackground(state, i, color)` (vide/null → `delete` = hérite) et
    `effectivePageBg(state, i)` — purs, testés (`tests/mutations.test.js`).
  - `js/inspector.js` (`renderPagePanel`, branche « rien sélectionné ») : champ **« Fond page »** =
    color picker + bouton **↺** « hériter du global » ; indicateur **« (hérité) »** quand pas d'override.
  - `js/canvas.js` : `stage.style.background = effectivePageBg(model.state, activePage)`.
  - CSS `.insp-bg-hint` / `.insp-bg-reset`.
- **Firmware** :
  - `src/dashboard.h` : `struct Page` gagne `uint32_t background`.
  - `src/dashboard.cpp` (boucle pages, ~l.93) : `p.background = parse_hex_color(pg["background"] | "", t.background)`
    — `""` → fallback `t.background` (hérite). `parse_hex_color("")` retourne bien le fallback (`color.cpp`).
  - `src/view.cpp` (création conteneur de page, ~l.374) : `bg_color = d->pages[p].background` + `bg_opa COVER`
    → **le fond fait partie de la page qui glisse au swipe**. (Le fond global reste sur l'écran, `view.cpp` ~l.360.)
  - Test : `test/test_core/test_main.cpp` → `test_page_background_override_and_inherit` (hérite vs override).
- **Validé on-device** : page héritante (sombre #0B0B0F) vs override (#1E9E5A vert) — fond bien attaché à la page.

## Étape 2 — IMAGE de fond (À FAIRE, prochaine session)

Le user veut **« les deux »** (couleur + image) ; on a décomposé et fait la **couleur d'abord** (étape 1).
L'image **se greffe sur la mécanique par-page** déjà en place. Modèle de référence : même esprit que la
couleur (override par page, sinon global/rien).

**Décidé :** override optionnel, par page ; l'image vient s'ajouter au schéma/designer/firmware existants.

**À brainstormer (rien encore tranché) :**
- **Format** : PNG / BMP / quel décodeur LVGL activer ? (`src/lv_conf.h` a déjà `LV_USE_SNAPSHOT 1` ;
  `LV_USE_PNG`/`LV_USE_BMP` à vérifier/activer). LVGL v8.3–8.4 (Context7 `/websites/lvgl_io_8_4`).
- **Acheminement de l'image** : upload depuis le designer → **endpoint device** → **LittleFS** (le device sert déjà
  des fichiers via `serveStatic`, cf. designer embarqué). Décider nom/chemin, et la réf dans le layout
  (`pages[].background_image` ?).
- **Taille / RAM** : écran 360×360 ; **PSRAM dispo** (8 MB), RAM actuelle 45 %, Flash 25 %. Décoder à la volée
  vs bitmap pré-converti (RGB565). Budget à cadrer.
- **Designer** : aperçu de l'image dans le canvas + UI d'upload/sélection.

**Points d'ancrage firmware pour l'image :**
- Peindre le conteneur de page avec une image : `lv_obj_set_style_bg_img_src(cont, src, 0)` au lieu (ou en plus)
  de la couleur, dans `src/view.cpp` (même endroit que le `bg_color` de l'étape 1, ~l.374).
- Endpoints REST : `src/api.cpp` (voir `GET /screenshot`, `serveStatic`, `POST /layout|/update|/page`).
- Stockage / staging LittleFS : `tools/stage_fs.sh` + `pio run -e esp32s3 -t uploadfs`
  (`board_build.filesystem = littlefs` dans `platformio.ini`).

## Suivis ouverts

- **Designer embarqué PAS re-stagé.** Le firmware `src/` a été flashé (fond par page actif sur le device), mais
  l'image LittleFS du designer (`data/designer`) n'a pas été régénérée → `http://192.168.1.35/designer/`
  sert encore l'**ancien** designer (sans le champ « Fond page »). Pour aligner :
  `bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs` **depuis** `devices/guition_knob/projects/Rich_Telemetry/`
  (⚠ `uploadfs` écrase le layout persisté → re-`POST /layout` ensuite). `data/designer` + `data/schema` sont gitignorés.
- Choix délibérés à ne pas « corriger » : couleur par page = **override optionnel** (hérite du global) ;
  **toast = verdict** d'action (échec rouge / succès vert), `#status` = progression, zones dédiées = état persistant
  (validité layout, santé device) ; **renommage manuel bloque les doublons** (le nom de page est la cible de
  `POST /page`, résolu à la 1re occurrence côté firmware).

## Lancer / tester

- **Designer local** : `cd devices/guition_knob/projects/Rich_Telemetry && python3 -m http.server 8000`
  → `http://localhost:8000/designer/` (servir depuis `Rich_Telemetry/`, **pas** `designer/`, pour `../schema`).
- **Tests designer** : `cd .../designer && node --test` (125).
- **Tests firmware** : `cd .../Rich_Telemetry && pio test -e native` (67). Compile cible : `pio run -e esp32s3`.
- **Validation visuelle device** : cf. section « Contexte device » ci-dessus.

Voir aussi : [[project-rich-telemetry]], [[project-rt-designer]], [[feedback-device-validation-workflow]],
[[project-dev-lan-mdns-filtered]], et `designer/README.md`.
