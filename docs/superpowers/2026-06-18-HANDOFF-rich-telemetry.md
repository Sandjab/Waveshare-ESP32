# HANDOFF — Rich_Telemetry (Guition K718)

**Date : 2026-06-18.** Document autoporteur pour reprendre après un `/clear`. **Remplace**
`docs/superpowers/2026-06-18-HANDOFF-rich-telemetry-page-bg.md` (périmé). Détail long et durable :
mémoire [[project-rich-telemetry]] et [[project-rt-designer]].

## TL;DR — état

Tout est **mergé et poussé sur `origin/master`** (HEAD `9a66fe7`, branche `master` **synchronisée**
avec origin, arbre propre). Cette session a livré, dans l'ordre, **deux features Rich_Telemetry** +
un nettoyage, toutes designer-driven, validées on-device :

1. **Étape 2 — image de fond par page** (la couleur = étape 1, déjà faite avant cette session).
2. **Zone « Device » — composants physiques (led_ring/sound) édités hors pages.**
3. **Nettoyage du code mort** des badges physiques.

Tests au vert : designer `node --test` **152/152** ; firmware `pio test -e native` **75/75** ;
`pio run -e esp32s3` SUCCESS. Plus aucun track de feature ouvert (voir « Suite possible » plus bas).

## Commits de la session (anciens → récents, tous sur `origin/master`)

```
# --- Étape 2 : image de fond par page ---
229b8b5 helpers purs bg-image (coverRect, RGBA<->565, FNV-1a)
3fdf9bc mutations setPageBackgroundImage / effectivePageBgImage
0ae1376 glue navigateur bg-image (conversion fichier + cache apercu)
77ef5b5 REST uploadBgImage / fetchBgImage
e23ffef controle Image de fond dans le panneau page
426cdbc apercu image de fond sur le stage (prime sur couleur)
dcd8315 upload des fonds au Pousser, fetch au Charger
# --- Zone « Device » : composants physiques ---
2c030d0 docs: spec zone « Device »
a33618f docs: plan zone « Device »
47e2a29 flag singleton sur led_ring
d039775 helpers purs composants physiques (physical.js)
bd3c5cc panneau Device pour composants physiques
3dafc80 retirer les composants physiques de la palette/canvas
ec4d212 migration douce des placements physiques au chargement
885169e fix: layout par défaut sans placements physiques
# --- Nettoyage ---
9a66fe7 chore: supprimer le code mort des badges physiques
```
(Les commits de docs `f2b329d`/`a5b9255`/`7272621` des specs/plans de l'étape 2 sont également sur master.)

## Contexte device — IMPORTANT (testable sans le user)

**« User à distance » ≠ « device injoignable ».** Le Guition reste branché au Mac. Workflow complet :
mémoire [[feedback-device-validation-workflow]].
- MAC `ac:a7:04:ef:74:28` → `knobGuitionNoir1` → `device_dir=guition_knob`.
- IP **`192.168.1.35`** (DHCP stable ; tester `curl --max-time 4 http://192.168.1.35/status` d'abord).
- **Flash app** : `./build.sh guition_knob Rich_Telemetry --upload` (device-check inclus).
- **Re-stage du designer embarqué** (après tout changement de `designer/*`) :
  `bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs` **depuis** le projet.
  ⚠ `uploadfs` **écrase** le layout persisté **et** les `/bg/*.565` → re-`POST /layout` ensuite.
- **Snapshot LCD par moi** : `curl --max-time N http://192.168.1.35/screenshot -o x.bmp`
  → `sips -s format png x.bmp --out x.png` → `SendUserFile`. (Pas de `timeout` sur macOS ; `curl --max-time`.)
- **Toujours** `GET /layout` (backup) avant de pousser un layout de test, et **restaurer** après.
- État laissé : device sur le **layout d'origine de l'utilisateur** (restauré en fin de session).

## Feature 1 — Image de fond par page (FAIT)

Override optionnel par page, **comme la couleur** (étape 1). spec/plan
`docs/superpowers/{specs,plans}/2026-06-18-rich-telemetry-bg-image*`.
- **Designer convertit** n'importe quelle image → **RGB565 360×360** (fit `cover`) ; **aucun décodage
  sur le MCU**. Clé d'asset = **hash FNV-1a 64 du contenu** (non-crypto : designer servi en HTTP simple →
  `crypto.subtle` indispo). `designer/js/bg-image.js`.
- **Endianness : octets stockés octet-fort-en-premier (`SWAP=true`) car `LV_COLOR_16_SWAP=1`** — confirmé
  ROUGE on-device. NE PAS « corriger ».
- Firmware : `POST /bgimage?key=` (multipart streamé LittleFS, garde taille 259200→`400`, temp+rename),
  `GET /bgimage?key=` (round-trip), `pages[].background_image` (image **prime sur couleur**), chargé en
  **PSRAM** → `lv_img_dsc_t` → `bg_img` sur le conteneur de page (`view.cpp`, free au `view_rebuild`),
  **sweep des orphelins** au `POST /layout`, garde `bg_key_valid` (`dashboard.cpp`).

## Feature 2 — Zone « Device » pour composants physiques (FAIT)

Les sorties device-globales (`led_ring`, `sound`) ne sont **plus posées sur une page** : éditées dans un
panneau **« Device »** (pied du designer). spec/plan
`docs/superpowers/{specs,plans}/2026-06-18-rt-physical-device-zone*`.
- **Pourquoi** : au runtime elles SONT déjà globales — `led_ring_tick`/`sound_tick` (`main.cpp:105-106`)
  balaient `components[]` global ; la vtable vue les ignore (`view.cpp` `{nullptr,nullptr}`) ;
  `dash_set_layout` garde un composant **sans placement**. Le placement était inerte.
- **Changement designer-seul** (firmware & schéma INCHANGÉS). Généralisé par le flag **`physical`** du
  registre. Nouveaux `designer/js/physical.js` (helpers purs testés) + `designer/js/device-panel.js`
  (calqué sur `sources.js`).
- **Cardinalité** : **led_ring = singleton** (flag `singleton:true`), **sound = 0..N**.
- **Migration douce** des placements physiques au chargement (autosave démarrage / Charger / import ;
  **PAS** le panneau JSON-avancé) + `default-layout.js` nettoyé.
- Validé on-device : `POST /layout` migré accepté (physiques dans `components`, **non placés**),
  `/update led` → `{ok}` (piloté sans placement), device vivant.

## Suivis ouverts / à savoir

- **Designer embarqué (LittleFS) potentiellement en retard d'un commit.** Le dernier `uploadfs` a eu lieu
  pendant la validation de la Feature 2 ; le commit de **nettoyage `9a66fe7` (code mort) n'y est pas
  encore** — purement cosmétique (suppression de code mort, comportement identique). Pour aligner :
  `bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs` (puis re-`POST /layout`). Non urgent.
- **Suite possible (non tranchée, idée restée en l'air)** : **page de config accessible par swipe vers le
  haut** — cohérent avec le choix délibéré « swipes verticaux réservés » (cf. [[project-rich-telemetry]]).
- Choix délibérés à **ne pas « corriger »** : bg `SWAP=true`, fit `cover`, image > couleur ; physiques =
  globaux édités dans la zone « Device » (jamais de placement) ; migration exclut JSON-avancé ; led_ring
  singleton / sound 0..N ; (côté firmware antérieur) sound timeout-0, swipes verticaux réservés,
  toast=verdict / `#status`=progression, renommage de page bloque les doublons.

## Lancer / tester

- **Designer local** : `cd devices/guition_knob/projects/Rich_Telemetry && python3 -m http.server 8000`
  → `http://localhost:8000/designer/` (servir depuis `Rich_Telemetry/`, **pas** `designer/`, pour `../schema`).
- **Designer embarqué** : `http://192.168.1.35/designer/` (saisir l'IP dans le champ Device — sinon
  « URL device ? » ; le texte gris est un *placeholder*, pas la valeur).
- **Tests designer** : `cd .../designer && node --test` (**152**).
- **Tests firmware** : `cd .../Rich_Telemetry && pio test -e native` (**75**). Compile : `pio run -e esp32s3`.

Voir aussi : [[project-rich-telemetry]], [[project-rt-designer]], [[feedback-device-validation-workflow]],
[[project-dev-lan-mdns-filtered]], et `designer/README.md`.
