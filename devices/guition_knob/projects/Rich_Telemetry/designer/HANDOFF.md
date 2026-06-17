# Rich_Telemetry Designer — HANDOFF (reprise après clear context)

> **MISE À JOUR 2026-06-17 (post-P3) — Améliorations designer : FAITES.** (1) **Filet & gardes** : autosave
> localStorage (anti-perte), gardes de limites firmware (pages 8 / composants 32 / placements 12) +
> avertissement `bind`↔sources dans `validate.js`. (2) **Boucle device** : `device.js` câble `/update`
> (« Valeurs test »), `/page` (nav ◀▶ dans l'overlay de capture), `/status` (barre de santé + état des
> sources), `/screenshot` (« Capture écran »). (3) **Designer embarqué** : servi par le device à
> `http://<ip>/designer/` via LittleFS (`tools/stage_fs.sh` → `pio … -t uploadfs`), même origin. (4)
> **Parité de rendu** alignée sur le device (`snapshots/parity/`). Les mentions « à venir » / « CORS à
> résoudre » / « pill approximatif » ci-dessous sont **périmées** → voir `README.md` à jour. `node --test` 93/93.

> **MISE À JOUR 2026-06-17 — Pull P3 + chart/meter (designer) : FAITS** (branche
> `feat/rt-designer-p3`, plan `docs/superpowers/plans/2026-06-17-pull-P3-designer.md`). Ajoutés :
> panneau d'édition des `sources` (pull réseau) dans le footer (`js/sources.js` + mutations
> `addSource`/… dans `mutations.js`) ; champ `bind` (variable du contexte) dans l'inspecteur pour
> label/readout/bar/ring/chart/meter ; types **chart** (sparkline) et **meter** (jauge arc 270° +
> zones + aiguille) de bout en bout (schema `comp_chart`/`comp_meter`, `registry.js`, aperçu SVG dans
> `render.js`, éditeur de zones du meter dans `inspector.js`). **Secrets délibérément HORS designer**
> (conforme spec ; `POST /secrets` manuel). Vérifié : `node --test` 88/88 + navigateur. Le reste
> ci-dessous (Plans A/B/C) reste l'historique de l'éditeur WYSIWYG.

**Dernière mise à jour : 2026-06-16.** Document autoporteur pour reprendre le travail sur l'éditeur WYSIWYG du designer. Lis aussi `specs/` et `plans/` (ils sont la source de vérité ; ce fichier est l'état + le plan de reprise).

## TL;DR

L'éditeur WYSIWYG du designer est découpé en plans séquentiels. **Plans A (fondation), B (canvas WYSIWYG) et C1 (palette + inspecteur = édition mono-page) : implémentés, testés, revus (spec + qualité + holistique = SHIP), vérifiés navigateur.** Le « Plan C » a été **scindé en C1 + C2** : **C1 est FAIT** (`plans/2026-06-16-wysiwyg-editor-plan-c1-panels.md`, 6 commits `e8030ea`..`1561478`) ; **C2 (pages CRUD + canvas multi-pages + file-io + humanisation ajv + bibliothèque inter-pages) : implémenté et testé** (`plans/2026-06-16-wysiwyg-editor-plan-c2-pages-fileio.md`). `feat/rt-designer` **intègre désormais `master` à jour** et est soumise en **PR #7**. Le **CORS est disponible** (mergé via **PR #5**, présent dans la base de la branche après intégration de `master`) → **Charger/Pousser device fonctionne dans le navigateur**. Restent seulement les reportés v2+ du spec (`/update`, `/page`, aperçu animé led_ring, presets).

**Reprise immédiate :** écrire C2 (`superpowers:writing-plans`, même format que A/B/C1), puis l'exécuter en subagent-driven — cf. « Process de reprise » en bas.

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
node --test                 # 43 tests (geometry/model/validate/render) au post-Plan B

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

### Plan C1 — Édition (palette + inspecteur) — ✅ FAIT (2026-06-16)

Exécuté en subagent-driven (un implémenteur/tâche + revue spec puis qualité par tâche + revue holistique finale = **SHIP**), vérifié navigateur (Playwright, pilotage DnD HTML5 via `DataTransfer` synthétique + édition par events `change`). **6 commits** sur `feat/rt-designer` : `e8030ea` mutations · `6b8a279` mocks · `43ea977` canvas (mocks/onSelect/selectPlacement) · `0a6c789` palette · `8379154` inspecteur (props/ASCII/delete) · `1561478` inspecteur (géométrie/seuils/mock). `node --test` → **57/57**. Vérifié : palette 6 types, drag→crée+sélectionne, undo atomique, inspecteur props par type, ASCII live, delete+undo, géométrie (ancrage→`.outside`, width/height, rayon), seuils +/édit/×, mock pilote l'arc **sans** entrer dans l'undo ni le `layout.json`, tout reste `✓ valide`.

**Notes Minor reportées à C2** (revue holistique, non bloquantes) : (a) `canvas.getSelected` est une surface d'API non encore consommée — utile pour re-keyer la sélection en C2 ; (b) `tests/mocks.test.js` a une dépendance d'ordre implicite (id `cpu` partagé entre 2 tests ; passe car `node:test` est à ordre stable — fix = id distinct) ; (c) flash cosmétique de sélection au delete d'un placement non-dernier (`render` synchrone avant `clearSelection` ; aucune corruption — fix C2 = `selected=null` avant `model.commit`) ; (d) le store de mocks garde les ids de composants supprimés (pas de GC ; à balayer en C2 si rename/réutilisation inter-pages).

Plan complet d'origine : **`plans/2026-06-16-wysiwyg-editor-plan-c1-panels.md`** (6 tâches, code verbatim). A donné un **éditeur mono-page complet** (créer/éditer/supprimer des widgets à la souris, sans toucher au JSON). Lots :
1. `js/mutations.js` — ops layout pures (uniqueId, addComponent, addPlacement, removePlacement, setComponentProp/PlacementProp, setThresholds) + DEFAULTS par type, **TDD `node --test`**.
2. `js/mocks.js` — store des valeurs d'aperçu par composant (hors layout), **testé**.
3. `js/canvas.js` (modif) — mocks par composant, `onSelect {placeIndex, ref}`, `selectPlacement(i)`.
4. `js/palette.js` — 6 types, **drag→canvas crée + place + sélectionne** (HTML5 DnD).
5. `js/inspector.js` — props par type (table de descripteurs) + **signalement ASCII** + supprimer.
6. inspecteur — géométrie de placement + éditeur de `thresholds` du ring + valeur mock.

**Décisions C1 verrouillées** (cf. § « Décisions prises » du plan C1) : mutations dans un **module pur** appelé via `model.commit` (pas des méthodes du model) ; édition committée sur **`change`** (1 undo/édition), ASCII signalé en live ; **mocks hors modèle** (re-render canvas, **pas d'undo**, pas dans `layout.json`) ; **badges led_ring/sound non sélectionnables au canvas en C1** (édition via JSON avancé en attendant C2) ; **création palette = drag→canvas** (choix user). C1 reste **câblé sur `pages[0]`** (assumé).

### Plan C2 — Pages & fichier (à ÉCRIRE puis exécuter)
- `js/pages.js` : onglets de pages **CRUD + réordonner** ; **canvas multi-pages** (afficher la page active, pas seulement `pages[0]`).
- `js/file-io.js` : **export / import** `layout.json` (filet indépendant du device).
- **Humaniser les messages d'erreur ajv** (actuellement bruts type `/background must match pattern`) dans le panneau d'erreurs.
- **Bibliothèque de composants partagés inter-pages** (glisser un composant existant sur une autre page = le partager) — reportée de C1 car sa valeur est cross-page.
- **Refactors `canvas.js` à faire en C2** (notes de la revue holistique B, valables tant que C1 ne les traite pas) : (a) la sélection est un **index** dans `pages[0].place` — le CRUD/réordre de C2 doit **re-keyer la sélection sur une référence stable** (id/objet) ; (b) le canvas est **câblé en dur sur `pages[0]`** (`render`, les closures de commit, `placements()`, et les appels `mutations` avec `pageIndex=0`) — le multi-pages doit **propager l'index de page active** partout.
- Mutations supplémentaires à ajouter à `mutations.js` en C2 : `addPage`, `removePage`, `renamePage`, `reorderPages` (TDD `node --test`).

### Intégration firmware
- **CORS** : ✅ **DISPONIBLE** — mergé via **PR #5** (`Access-Control-Allow-Origin: *` + handler `OPTIONS` pour le preflight POST JSON), présent dans la base de `feat/rt-designer` après l'intégration de `master`. **Charger/Pousser device fonctionne désormais dans le navigateur** (c'était le seul prérequis firmware du designer ; l'export/import fichier de C2 permettait déjà de travailler sans device).
- **À confirmer en test réel maintenant que le CORS est là** : la forme de réponse de `POST /layout` (`device.js` suppose `{ok, error}` ; un 200 sans corps JSON est traité comme succès).

## Process de reprise recommandé
1. ~~Exécuter C1~~ — ✅ FAIT (cf. § « Plan C1 » ci-dessus).
2. **Écrire C2 maintenant** : `superpowers:writing-plans` (même format que A/B/C1), en s'appuyant sur le § « Plan C2 » ci-dessus + les notes Minor reportées de C1 + le spec. Décisions ouvertes éventuelles → `superpowers:brainstorming` d'abord.
3. Exécuter C2 en subagent-driven (même process que C1 : un implémenteur/tâche, revue spec puis qualité, vérif navigateur par le contrôleur via Playwright en servant depuis `Rich_Telemetry/`).

**Astuces process (rodées sur A/B/C1)** : le hook pre-commit affiche un warning jaune `SCHEMA DIVERGENT` (non bloquant, pré-existant — voir plus bas) ; la vérif navigateur se fait au mieux via Playwright MCP en pilotant les vrais pointer/DnD events (attention : un `pointerdown` synthétique sans `pointerup` laisse un drag fantôme — faire des clics complets) ; nettoyer les screenshots de test laissés à la racine du repo.

## Pièges rencontrés (à re-signaler aux implémenteurs)
- **Apostrophes dans les labels de test** : un label contenant `'` doit être une string délimitée par `"` (sinon SyntaxError).
- **ajv** : `new Ajv({allErrors:true, strict:false})` compile le schéma draft-07 (oneOf×6, $defs, tuples) sans souci ; le bundle vendorisé est browser-safe.
- **Schema servi** : servir depuis le **parent** `Rich_Telemetry/` (le schéma partagé est hors `designer/`).
- **Hook pre-commit `SCHEMA DIVERGENT` (non bloquant)** : à chaque commit, un hook signale en jaune que `schema/layout.schema.json` a divergé entre `feat/rt-embedded` et `feat/rt-designer` vs master. **Pré-existant, sans rapport avec le designer** (Plans B/C1 n'ont pas touché le schéma) ; le commit passe quand même. À resync via master un jour (le hook imprime la commande `git diff` utile). Ne pas s'en inquiéter pendant l'exécution de C1/C2.
- **Vérif navigateur (Playwright MCP)** : servir depuis `Rich_Telemetry/`, piloter de vrais events ; un `pointerdown` synthétique sans `pointerup` laisse un drag fantôme dont les `pointermove` ultérieurs bullent et déplacent le widget (utiliser des clics complets down+up). Nettoyer les `*.png` de test laissés à la racine du repo après coup.
