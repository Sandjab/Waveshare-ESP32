# HANDOFF — Rich_Telemetry (Guition K718)

**Date : 2026-06-19.** Document autoporteur pour reprendre après un `/clear`. **Remplace**
`docs/superpowers/2026-06-18-HANDOFF-rich-telemetry.md` (périmé). Détail long et durable :
mémoire [[project-rich-telemetry]] et [[project-rt-designer]].

## TL;DR — état

Tout est **mergé et poussé sur `origin/master`** (HEAD `a2995c7`, branche `master` **synchronisée**,
arbre propre hormis le `docs/superpowers/specs/2026-06-18-dialboard-launch-strategy.md` **volontairement
untracked**). Cette session a livré, dans l'ordre :

1. **Alignement du designer embarqué** (LittleFS) — `uploadfs` pour servir le designer à jour à `/designer/`.
2. **Fix « guide d'ancrage au drag »** du designer (commit `052ef18`).
3. **Feature « style du label de la Bar »** (alignement 8 positions + couleur + police) — 7 commits, mergés.

Tests au vert : designer `node --test` **157/157** ; firmware `pio test -e native` **77/77** ;
`pio run -e esp32s3` SUCCESS. Designer validé au navigateur, firmware **validé on-device**.

## Ce que la session a livré (détail)

### 1. Fix « guide d'ancrage au drag » (commit `052ef18`, poussé)
Dans le designer (`canvas.js` + `inspector.js` + `app.js`) :
- **Les 8 ancres + la ligne pointillée n'apparaissent QUE pendant le drag** et disparaissent au relâchement.
  Cause : le guide était caché via la **propriété** `svg.hidden`, inerte sur un `SVGElement` (non réfléchie
  en attribut), alors que le CSS cible `.ag[hidden]` (attribut). Corrigé en basculant l'**attribut** `hidden`.
- **Les champs Ancrage/dx/dy de l'inspecteur se mettent à jour en temps réel pendant le drag**, sans commit
  (callback `onLiveMove` → `setLivePlacement`) — le commit unique reste au drop (« commit-on-drop » préservé).

### 2. Feature « style du label de la Bar » (mergée, HEAD `a2995c7`)
Le label du composant **Bar** a désormais **alignement** (8 positions extérieures autour de la barre,
`LV_ALIGN_OUT_*`), **couleur** et **taille de police** configurables. Avant : codés en dur. **Défauts =
apparence d'avant** (gris `#9AA0AA`, police 14, au-dessus centré `TOP_MID`) → rétrocompat bit-à-bit.
- Champs plats `label_color` / `label_font` / `label_align` sur `comp_bar` ; nouveau `$defs/anchorOut`
  (8 positions, **sans CENTER**) = designer `ANCHORS_OUT` = firmware `ALIGN_OUT_MAP[]` (indexé par l'enum
  `Anchor`, repli `A_CENTER`→`OUT_TOP_MID`). Marge fixe `BAR_LABEL_GAP=6` (pas de dx/dy de label — YAGNI).
- Designer : rendu du label **hors flux** (`position:absolute`) autour du track → ne fausse pas le placement
  de la barre (8 classes `.w-bar-label--*`). Un bug attrapé en revue : `.w-bar{position:relative}` cassait
  le placement multi-barres (override de `.w{position:absolute}`) → **supprimé** (`.w` suffit comme conteneur).
- spec/plan : `docs/superpowers/{specs/2026-06-18-bar-label-style-design.md, plans/2026-06-18-bar-label-style.md}`.
- 5 commits feature : `cbb34fe` (schéma) → `b9aa719` (parsing) → `032cb0f` (rendu) → `0a77593` (designer
  registre/inspecteur) → `a2995c7` (designer rendu DOM).

## Contexte device — IMPORTANT (testable sans le user)

**« User à distance » ≠ « device injoignable ».** Le Guition reste branché au Mac. Workflow complet :
mémoire [[feedback-device-validation-workflow]].
- MAC `ac:a7:04:ef:74:28` → `knobGuitionNoir1` → `device_dir=guition_knob`. IP **`192.168.1.35`** (DHCP stable).
- **Flash app** : `./build.sh guition_knob Rich_Telemetry --upload` (device-check inclus). **Ne touche PAS
  le filesystem** (layout persisté + `/bg/*.565` survivent).
- **Re-stage du designer embarqué** (après changement de `designer/*`) :
  `bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs`. ⚠ `uploadfs` **écrase** le layout persisté
  **et** les `/bg/*.565` → **backup layout ET toutes les bg images référencées AVANT**, restaurer après
  (gotcha confirmé, cf. [[feedback-device-validation-workflow]]).
- **Snapshot LCD par moi** : `curl --max-time N http://192.168.1.35/screenshot -o x.bmp`
  → `sips -s format png x.bmp --out x.png` → `SendUserFile`. (Pas de `timeout` sur macOS ; `curl --max-time`.)
- **Toujours** backup `GET /layout` avant de pousser un layout de test, **restaurer** après.
- État laissé en fin de session : device flashé avec le firmware de `a2995c7` (nouveau rendu bar-label) +
  **layout d'origine de l'utilisateur restauré** (5 pages, 26 composants).

## Suivis ouverts / à savoir

- **Images de fond absentes du device** : le layout utilisateur référence `pages[].background_image`
  `6f7e4016e31048d2` (page 0) et `dc189d978cc23dc8` (page 3), mais **les deux sont 404** sur le device
  (perdues lors d'un `uploadfs` antérieur). Le firmware retombe sur le fond couleur (garde `bg_key_valid`).
  **Recours** : re-« Pousser » depuis le designer (cache navigateur) ré-uploade les bg référencés.
- **Suite possible (non tranchée)** : **page de config accessible par swipe vers le haut** — cohérent avec
  le choix délibéré « swipes verticaux réservés ».
- Choix délibérés à **ne pas « corriger »** : label de bar — défauts = apparence d'avant, marge fixe,
  `anchorOut` exclut CENTER, label hors flux ; guide d'ancrage visible au drag seulement ; (antérieurs)
  bg `SWAP=true`, image > couleur, physiques globaux édités dans la zone « Device », sound timeout-0,
  swipes verticaux réservés, toast=verdict / `#status`=progression, renommage de page bloque les doublons.

## Lancer / tester

- **Designer local** : `cd devices/guition_knob/projects/Rich_Telemetry && python3 -m http.server 8000`
  → `http://localhost:8000/designer/` (servir depuis `Rich_Telemetry/`, **pas** `designer/`, pour `../schema`).
- **Designer embarqué** : `http://192.168.1.35/designer/` (saisir l'IP dans le champ Device).
- **Tests designer** : `cd .../designer && node --test` (**157** ; PAS `node --test tests/` — casse).
- **Tests firmware** : `cd .../Rich_Telemetry && pio test -e native` (**77**). Compile : `pio run -e esp32s3`.

Voir aussi : [[project-rich-telemetry]], [[project-rt-designer]], [[feedback-device-validation-workflow]],
[[project-dev-lan-mdns-filtered]], et `designer/README.md`.
