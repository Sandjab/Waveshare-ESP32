# Rich_Telemetry Designer — HANDOFF (reprise après clear context)

**Dernière mise à jour : 2026-06-16.** Document autoporteur pour reprendre le travail sur l'éditeur WYSIWYG du designer. Lis aussi `specs/` et `plans/` (ils sont la source de vérité ; ce fichier est l'état + le plan de reprise).

## TL;DR

L'éditeur WYSIWYG du designer est découpé en **3 plans séquentiels (A/B/C)**. **Les Plans A (fondation) et B (canvas WYSIWYG) sont implémentés, testés, revus et vérifiés en navigateur.** **Le Plan C reste à écrire et exécuter** (palette, inspecteur, pages CRUD, file-io, mutations dédiées `model.js`). Tout vit sur la branche **`feat/rt-designer`** (non mergée).

## Branche & base

- Travail sur **`feat/rt-designer`** (non mergée).
- ⚠️ `master` a avancé pendant la session (à `b32a6b9` « Merge PR #3 guition-onboard-note » au 2026-06-15) ; `feat/rt-designer` est basée sur un master antérieur. **Avant de merger B/C, intégrer le master récent** (merge/rebase) et re-tester.
- Le user a délibérément choisi de **garder la branche** plutôt que merger (Plan A = fondation, pas le designer fini).

## Ce qui est fait — Plan A (fondation)

Web app vanilla **zéro-build / zéro-dépendance** sous `devices/guition_knob/projects/Rich_Telemetry/designer/`. Modules ES :
- `js/geometry.js` — math ancrage+offset LVGL (`offsetFor/nearestAnchor/snapPlacement/placeAt`), **pure, testée**. Prête pour le canvas du Plan B (pas encore consommée).
- `js/model.js` — état layout + `commit(mutator)` + undo/redo + `subscribe` + `toJSON/loadJSON`. Source de vérité, **pure, testée**.
- `js/validate.js` — `createValidator(schema) → validate(layout) → {valid, errors}` : forme ajv + check sémantique des `ref`. **Testée.**
- `js/device.js` — `loadLayout/pushLayout` REST (`GET/POST /layout`).
- `js/json-view.js` — panneau JSON avancé ↔ modèle (validation live, guard focus).
- `js/app.js` — bootstrap (fetch schema, câblage model↔vues↔device↔undo, garde-fou Push anti perte-silencieuse).
- `js/default-layout.js`, `index.html` (coquille 3 colonnes), `style.css`, `vendor/ajv.min.js` (bundle ESM auto-contenu, browser+Node).

Commits Plan A sur `feat/rt-designer` (valides au 2026-06-15, peuvent bouger en cas de rebase) :
```
3942515 gitignore: ignore .playwright-mcp/
6738c0d designer wire JSON view + validation + undo + device I/O   (Task 5)
defc40c designer validate module (ajv shape + ref semantics)        (Task 4)
bc2a20d designer model module (state, mutations, undo/redo)          (Task 3)
07ac358 designer geometry module (anchor+offset, snap)               (Task 2)
148d782 designer scaffold 3-col shell + default layout + vendored ajv (Task 1)
e2f4b13 Plan A implementation plan
7c90569 design doc (brainstorm)
232071f scaffold WYSIWYG designer (skeleton initial)
```

**Vérifié :** `node --test` → 25/25 (geometry/model/validate). Smoke test navigateur réel (Playwright) : défaut chargé + `✓ valide`, JSON invalide → `✗ invalide` + erreur ajv, undo/redo OK, garde-fou Push OK. Revue finale holistique : **SHIP**.

## Comment lancer / tester

```bash
cd devices/guition_knob/projects/Rich_Telemetry/designer
node --test                 # 25 tests (logique pure)

# Servir : DEPUIS LE PARENT (pour que ../schema soit accessible en HTTP)
cd ..                       # → Rich_Telemetry/
python3 -m http.server 8000
# puis http://localhost:8000/designer/
```
Le câblage DOM n'est pas couvert par `node --test` → vérification navigateur.

## Décisions verrouillées (cf. `specs/2026-06-15-wysiwyg-editor-design.md`)

1. Usage : **pour n'importe qui** (UX guidée). 2. Positionnement : **hybride** (drag + snap aux ancrages, ancrage éditable). 3. Aperçu : **best-effort** HTML/canvas, **indicatif** (le device arbitre). 4. Disposition : **3 colonnes**. 5. Stack : **vanilla zéro-build**, ajv vendorisé. 6. **CORS** : header côté firmware.

## Ce qui est fait — Plan B (canvas WYSIWYG)

Implémenté en subagent-driven (plan : `plans/2026-06-15-wysiwyg-editor-plan-b-canvas.md`), revu (spec + qualité par tâche + revue holistique finale = SHIP) et vérifié en navigateur (Playwright).
- `js/render.js` : rendu best-effort `label/readout/bar/ring` + **badges** `led_ring`/`sound` ; math pure (barFill, pickThresholdColor, formatValue/Remaining, arcPath/ringPaths) **testée** ; miroir fidèle de `src/view.cpp`/`color.cpp`/`format.cpp` (dont `Math.trunc` du `%` pour coller à `(long)c.value`).
- `js/canvas.js` : drag + snap via `geometry.js`, sélection, **poignées de redim** (bar width/height ; ring radius/thickness/gap_deg), commit-on-drop, conscience du cercle (`.outside`), re-render sur `document.fonts.ready` (centrage fidèle au 1er paint).
- `js/geometry.js` += `resizeBox/ringRadiusAt/ringThicknessAt/gapDegAt/cornersOutsideCircle` (**testés**). `default-layout.js` = layout démo (4 widgets + 2 badges). `style.css`/`index.html` : stage rond 360 px + poignées + badges + Montserrat vendorisée.
- **Décision verrouillée (firmware)** : le ring est **centré et non déplaçable** (`lv_obj_center`, `view.cpp:51` → `anchor/dx/dy` ignorés pour un ring), seulement redimensionnable. Documenté dans `designer/README.md` (« Aperçu : indicatif »).

**Vérifié :** `node --test` → 43/43. Navigateur : rendu correct, drag = **1 seul** undo/déplacement, ring non déplaçable, resize bar+ring OK, undo/redo, `.outside`, sync JSON↔canvas bidirectionnelle.

## Ce qui reste

### Plan C — Panneaux & fichier (à écrire + exécuter)
- `js/palette.js` (6 types + bibliothèque de composants partagés inter-pages), `js/inspector.js` (props par type + éditeur de `thresholds` + valeur mock + **signalement ASCII** + **humaniser les messages d'erreur ajv** — actuellement bruts type `/background must match pattern`), `js/pages.js` (CRUD + réordonner), `js/file-io.js` (export/import `layout.json`).
- Ajouter des **mutations dédiées** à `model.js` (`addComponent`, `placeOnPage`, page CRUD…) en **TDD `node --test`** (l'API actuelle `commit(mutator)` est volontairement minimale, extension prévue).
- **À reprendre dans `canvas.js` quand Plan C arrive** (notes de la revue holistique B) : (a) la sélection est un **index** dans `pages[0].place` — OK tant que les placements sont stables, mais le CRUD de Plan C (suppression/réordre) doit **re-keyer la sélection sur une référence stable** (id/objet) ; (b) le canvas est **câblé en dur sur `pages[0]`** (`render`, les 3 closures de commit, `placements()`) — le multi-pages doit **propager l'index de page active** ; (c) `onSelect` est un no-op câblé dans `app.js`, prêt pour l'inspecteur.

### Prérequis firmware (hors designer)
- **CORS** sur le `WebServer` ESP32 : `Access-Control-Allow-Origin: *` + handler `OPTIONS` (preflight POST JSON). Sur la branche embarqué, **commit dédié**. Sans lui, Charger/Pousser device est câblé mais bloqué dans un navigateur (les autres lots n'en dépendent pas ; l'export fichier de Plan C permet de travailler sans device).
- À confirmer quand le firmware CORS arrive : la forme de réponse de `POST /layout` (`device.js` suppose `{ok, error}` ; un 200 sans corps JSON est traité comme succès).

## Process de reprise recommandé
1. Si Plan C a des décisions ouvertes → `superpowers:brainstorming` d'abord. Sinon, les décisions sont déjà dans le spec.
2. `superpowers:writing-plans` pour détailler le plan C (même format que A/B).
3. `superpowers:subagent-driven-development` pour exécuter (un subagent/tâche + revue spec puis qualité ; vérif navigateur du DOM par le contrôleur via Playwright). Plans A et B ont suivi exactement ce process.

## Pièges rencontrés (à re-signaler aux implémenteurs)
- **Apostrophes dans les labels de test** : un label contenant `'` doit être une string délimitée par `"` (sinon SyntaxError).
- **ajv** : `new Ajv({allErrors:true, strict:false})` compile le schéma draft-07 (oneOf×6, $defs, tuples) sans souci ; le bundle vendorisé est browser-safe.
- **Schema servi** : servir depuis le **parent** `Rich_Telemetry/` (le schéma partagé est hors `designer/`).
