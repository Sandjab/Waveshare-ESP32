# Rich_Telemetry Designer — Plan C2 : Pages & fichier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Passer le designer d'un éditeur **mono-page** à un éditeur **multi-pages** : onglets de pages (créer / renommer / déplacer / supprimer), canvas qui affiche la **page active** (plus seulement `pages[0]`), **bibliothèque** de composants réutilisables inter-pages, **export / import** fichier `layout.json`, et **messages d'erreur ajv humanisés**.

**Architecture:** On prolonge la séparation A/B/C1. Les nouvelles ops de pages vont dans `mutations.js` (PUR, `node --test`). La page active devient une donnée **hors layout** (le device n'a pas à la connaître) : elle vit dans `canvas.js` (source de vérité unique), lue via `getActivePage()` et pilotée via `setPage(i)`. `inspector.js` et `palette.js` ciblent désormais la page active au lieu de `pages[0]`. Deux nouvelles vues DOM : `pages.js` (onglets) et `file-io.js` (export/import). L'humanisation des erreurs va dans un module pur `humanize.js`, branché dans `validate.js`.

**Tech Stack:** JavaScript (modules ES), `node --test` (zéro dépendance), HTML5 drag-and-drop (palette + bibliothèque → canvas), `Blob`/`URL.createObjectURL` + `<input type=file>` pour le fichier. Servir via `python3 -m http.server` **depuis `Rich_Telemetry/`** (le schéma partagé est en `../schema/`).

---

## Périmètre : C2 sur 2 (clôture du « Plan C »)

Le « Plan C » du spec a été scindé en C1 (édition mono-page, **FAIT**) et C2 (ce plan). C2 livre les éléments du spec encore manquants :

- **Pages** : onglets CRUD + réordonner ; **canvas multi-pages** (afficher la page active).
- **Bibliothèque** de composants partagés inter-pages (glisser un composant existant sur le canvas = le **placer** sur la page active en partageant le même `id`/état, pas une copie).
- **Export / import fichier** `layout.json` (filet indépendant du device, sans CORS).
- **Humanisation des messages d'erreur ajv** (le panneau d'erreurs brut du Plan A devient lisible).
- **Refactors reportés de C1** (notes de revue holistique B + C1) intégrés ici :
  - (B-a) la sélection est un **index** dans `pages[<active>].place` — réglé en **désélectionnant au changement de page** (un index n'a pas de sens d'une page à l'autre ; plus simple que re-keyer sur une référence stable, et la sélection n'a pas à survivre à un changement de page).
  - (B-b) le canvas était **câblé en dur sur `pages[0]`** — l'index de page active est propagé dans `render`, les closures de commit (drag/resize), `placements()`, `inspector` et `palette`.
  - (C1-b) `tests/mocks.test.js` a une **dépendance d'ordre** (id `cpu` partagé) — corrigé (id distinct).
  - (C1-c) **flash cosmétique** de sélection au delete d'un placement non-dernier — corrigé (`sel = null` + `clearSelection()` **avant** le commit).

### Hors scope (reste reporté, documenté)

- (C1-d) **GC du store de mocks** : C2 n'ajoute ni rename de composant ni suppression de composant de la bibliothèque (le partage inter-pages n'orpheline aucun mock), donc le GC reste inutile en C2. Laissé déféré avec une note dans `mocks.js`.
- `POST /update` / `POST /page` (valeurs/navigation live), aperçu animé `led_ring`, presets, multi-layouts, aperçu pixel-exact — reportés v2+ par le spec.
- Édition de `title` / `background` / `nav.wrap` du dashboard : restent éditables via le **JSON avancé** (pas de scope-creep d'UI dédiée en C2).

C2 produit un éditeur multi-pages complet, utilisable de bout en bout sans device.

---

## Rappel de l'état actuel (post Plan C1)

- `model.js` : `createModel()` → `{ state, subscribe, commit(mutator), undo, redo, canUndo, canRedo, toJSON, loadJSON }`. `commit(mutator)` mute `state` en place (snapshots undo clonés). La page active **n'est pas** dans `state` (hors layout).
- `mutations.js` (PUR) : `uniqueId`, `DEFAULTS`, `addComponent`, `addPlacement(state, pageIndex, placement)`, `removePlacement(state, pageIndex, placeIndex)`, `setComponentProp`, `setPlacementProp(state, pageIndex, placeIndex, key, value)`, `setThresholds`. **C2 ajoute** `addPage/removePage/renamePage/reorderPages`.
- `canvas.js` : `createCanvas({stage, badges}, model, {onSelect}) → { render, getSelected, selectPlacement }`. Sélection par **index** de placement, **câblé sur `pages[0]`** (4 occurrences : `placements()`, drag-commit, bar-resize-commit, ring-resize-commit). `onSelect` reçoit `{placeIndex, ref}`. **C2 ajoute** `activePage` + `setPage` + `getActivePage`.
- `inspector.js` : `createInspector(root, model, {rerenderCanvas, clearSelection}) → {select}`. `place()` lit `pages[0]` ; `setPlacementProp(s, 0, …)` et `removePlacement(s, 0, i)` figés sur 0. **C2** propage `getActivePage`.
- `palette.js` : `createPalette(root, model, {stage, onCreated})`. Drop crée sur `pages[0]`. **C2** ajoute la bibliothèque + cible la page active.
- `validate.js` : `createValidator(schema) → validate(layout) → {valid, errors}`. Erreurs ajv brutes (`${e.instancePath || '/'} ${e.message}`) + refs sémantiques. **C2** humanise.
- `json-view.js`, `device.js`, `app.js` (bootstrap), `render.js`, `geometry.js` (`ANCHORS`, `snapPlacement`, …), `mocks.js` (`getMock(id,type)/setMock(id,patch)`) : inchangés sauf le câblage d'`app.js`.
- Schéma (source de vérité) `../schema/layout.schema.json` : `page` = `{ name (string, requis), place[] }` ; `placement` = `{ref (requis), anchor, dx, dy, width, height, radius, thickness, gap_deg, start_angle}`. `additionalProperties:false` partout. `pages` est un `array` sans `minItems` (zéro page valide vis-à-vis de la forme, mais inutile — on garde ≥ 1 page côté UI).
- `node --test` baseline : **57 tests** (vérifié au départ de cette session).

---

## File Structure

```
designer/
├── index.html              # MODIF : <nav id="pages"> (Task 4) + boutons Exporter/Importer (Task 6)
├── style.css               # MODIF : styles onglets (Task 4) + bibliothèque (Task 5)
└── js/
    ├── mutations.js        # MODIF : addPage/removePage/renamePage/reorderPages (Task 1)
    ├── humanize.js         # NOUVEAU : humanisation des erreurs ajv (PUR, testé) (Task 2)
    ├── validate.js         # MODIF : branche humanize + filtre le bruit oneOf + dedupe (Task 2)
    ├── canvas.js           # MODIF : activePage + setPage + getActivePage ; pages[0] → pages[active] (Task 3)
    ├── inspector.js        # MODIF : getActivePage ; garde sélection obsolète ; fix flash delete (Task 3)
    ├── palette.js          # MODIF : bibliothèque (drag ref → place) + cible page active (Task 5)
    ├── pages.js            # NOUVEAU : onglets CRUD + réordonner (vérif navigateur) (Task 4)
    ├── file-io.js          # NOUVEAU : export/import layout.json (vérif navigateur) (Task 6)
    └── app.js              # MODIF : câble getActivePage, pages, file-io (Tasks 3,4,5,6)
└── tests/
    ├── mutations.test.js   # MODIF : tests des ops de pages (Task 1)
    ├── mocks.test.js       # MODIF : fix dépendance d'ordre, id distinct (Task 1)
    ├── humanize.test.js    # NOUVEAU (Task 2)
    └── validate.test.js    # MODIF : assertions adaptées aux messages humanisés (Task 2)
```

---

## Task 1 : `mutations.js` — ops de pages (TDD) + hygiène `mocks.test.js`

**Files:**
- Modify: `designer/js/mutations.js`
- Modify: `designer/tests/mutations.test.js`
- Modify: `designer/tests/mocks.test.js`

- [ ] **Step 1 : étendre les tests de mutations** — ajouter au début de `designer/tests/mutations.test.js` les 4 fonctions à l'import existant. Remplacer le bloc d'import :
```js
import {
  uniqueId, DEFAULTS, addComponent, addPlacement, removePlacement,
  setComponentProp, setPlacementProp, setThresholds
} from '../js/mutations.js';
```
par :
```js
import {
  uniqueId, DEFAULTS, addComponent, addPlacement, removePlacement,
  setComponentProp, setPlacementProp, setThresholds,
  addPage, removePage, renamePage, reorderPages
} from '../js/mutations.js';
```
Puis ajouter, **à la fin du fichier**, les tests des pages :
```js
test('addPage ajoute une page vide nommée en fin de liste', () => {
  const s = fresh();
  addPage(s, 'P2');
  assert.equal(s.pages.length, 2);
  assert.deepEqual(s.pages[1], { name: 'P2', place: [] });
});

test('removePage retire la page par index', () => {
  const s = fresh();
  addPage(s, 'P2');
  removePage(s, 0);
  assert.deepEqual(s.pages.map(p => p.name), ['P2']);
});

test('renamePage change le nom de la page', () => {
  const s = fresh();
  renamePage(s, 0, 'Accueil');
  assert.equal(s.pages[0].name, 'Accueil');
});

test('reorderPages déplace from → to', () => {
  const s = fresh();
  addPage(s, 'P2'); addPage(s, 'P3');          // [P1, P2, P3]
  reorderPages(s, 0, 2);                        // [P2, P3, P1]
  assert.deepEqual(s.pages.map(p => p.name), ['P2', 'P3', 'P1']);
});

test('reorderPages ignore les index hors bornes (no-op)', () => {
  const s = fresh();
  addPage(s, 'P2');                             // [P1, P2]
  reorderPages(s, 0, 5);
  assert.deepEqual(s.pages.map(p => p.name), ['P1', 'P2']);
});
```

- [ ] **Step 2 : lancer, vérifier l'échec** — depuis `designer/` : `node --test tests/mutations.test.js` → FAIL (`addPage` etc. `is not a function` / import non résolu).

- [ ] **Step 3 : implémenter les ops de pages dans `js/mutations.js`** — ajouter **à la fin** du fichier :
```js
// --- Pages (Plan C2) ---

// Ajoute une page vide en fin de liste. `name` est requis (le schéma exige page.name).
export function addPage(state, name) {
  (state.pages ||= []).push({ name, place: [] });
}

export function removePage(state, pageIndex) {
  if (!state.pages) return;
  state.pages.splice(pageIndex, 1);
}

export function renamePage(state, pageIndex, name) {
  const page = state.pages?.[pageIndex];
  if (page) page.name = name;
}

// Déplace la page d'index `from` vers `to`. No-op si index hors bornes ou identiques.
export function reorderPages(state, from, to) {
  const pages = state.pages;
  if (!pages || from === to) return;
  if (from < 0 || from >= pages.length || to < 0 || to >= pages.length) return;
  const [p] = pages.splice(from, 1);
  pages.splice(to, 0, p);
}
```

- [ ] **Step 4 : lancer, vérifier le succès** — `node --test tests/mutations.test.js` → PASS. Puis `node --test` → **62 tests**, tout vert.

- [ ] **Step 5 : corriger la dépendance d'ordre de `tests/mocks.test.js` (note C1-b)** — le test « setMock fusionne » mutait l'id `cpu` que le 1er test lit à 42 ; on isole sur un id dédié. Remplacer :
```js
test('setMock fusionne et persiste par id', () => {
  setMock('cpu', { value: 88 });
  assert.equal(getMock('cpu', 'readout').value, 88);
});
```
par :
```js
test('setMock fusionne et persiste par id', () => {
  setMock('cpuEdit', { value: 88 });            // id dédié : pas de couplage d'ordre avec les autres tests
  assert.equal(getMock('cpuEdit', 'readout').value, 88);
});
```

- [ ] **Step 6 : lancer, vérifier** — `node --test tests/mocks.test.js` → PASS (4). `node --test` → **62 tests**, tout vert.

- [ ] **Step 7 : Commit**
```bash
git add designer/js/mutations.js designer/tests/mutations.test.js designer/tests/mocks.test.js
git commit -m "Rich_Telemetry: designer mutations — page ops (add/remove/rename/reorder) + tests; fix mocks test order-dep"
```

---

## Task 2 : `humanize.js` — messages d'erreur ajv lisibles (TDD) + branchement

**Files:**
- Create: `designer/js/humanize.js`
- Test: `designer/tests/humanize.test.js`
- Modify: `designer/js/validate.js`
- Modify: `designer/tests/validate.test.js`

> Le panneau d'erreurs montrait des messages bruts (`/background must match pattern "^#[0-9A-Fa-f]{6}$"`). On les rend lisibles en français. La logique est **pure** (`node --test`) ; on la branche dans `validate.js` puis on confirme le rendu sur de **vraies** erreurs ajv.

- [ ] **Step 1 : écrire les tests qui échouent** — `designer/tests/humanize.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { humanizeAjvError, humanizePath } from '../js/humanize.js';

test('humanizePath rend le chemin lisible (1-based)', () => {
  assert.equal(humanizePath('/pages/0/place/2/dx'), 'page 1 › élément 3 › dx');
  assert.equal(humanizePath('/components/cpu'), 'composant › cpu');
  assert.equal(humanizePath(''), 'racine');
});

test('pattern couleur → message dédié', () => {
  const e = { keyword: 'pattern', instancePath: '/background', params: { pattern: '^#[0-9A-Fa-f]{6}$' }, message: 'must match pattern' };
  assert.match(humanizeAjvError(e), /couleur.*#RRGGBB/);
});

test('pattern ASCII → message dédié', () => {
  const e = { keyword: 'pattern', instancePath: '/components/titre/text', params: { pattern: '^[\\x00-\\x7F]*$' }, message: 'must match pattern' };
  assert.match(humanizeAjvError(e), /ASCII/);
});

test('additionalProperties nomme la propriété inconnue', () => {
  const e = { keyword: 'additionalProperties', instancePath: '/components/cpu', params: { additionalProperty: 'foo' }, message: 'must NOT have additional properties' };
  assert.match(humanizeAjvError(e), /propriété inconnue.*foo/);
});

test('enum liste les valeurs permises', () => {
  const e = { keyword: 'enum', instancePath: '/pages/0/place/0/anchor', params: { allowedValues: ['CENTER', 'TOP_MID'] }, message: 'must be equal to one of the allowed values' };
  const s = humanizeAjvError(e);
  assert.match(s, /non autorisée/);
  assert.match(s, /CENTER/);
});

test('required nomme la propriété manquante', () => {
  const e = { keyword: 'required', instancePath: '/components/x', params: { missingProperty: 'type' }, message: 'must have required property' };
  assert.match(humanizeAjvError(e), /obligatoire.*type/);
});

test('type traduit le type attendu', () => {
  const e = { keyword: 'type', instancePath: '/pages/0/place/0/dx', params: { type: 'integer' }, message: 'must be integer' };
  assert.match(humanizeAjvError(e), /entier/);
});

test('keyword inconnu retombe sur le message ajv brut', () => {
  const e = { keyword: 'weird', instancePath: '/x', params: {}, message: 'must be weird' };
  assert.match(humanizeAjvError(e), /must be weird/);
});
```

- [ ] **Step 2 : lancer, vérifier l'échec** — `node --test tests/humanize.test.js` → FAIL (`Cannot find module`).

- [ ] **Step 3 : implémenter `js/humanize.js`** :
```js
// Humanise une erreur ajv (draft-07) brute en français lisible pour le panneau d'erreurs.
// ajv fournit { instancePath, keyword, params, message }. On mappe les keywords fréquents du schéma
// layout vers une phrase claire et on rend le chemin lisible. Les patterns du schéma (couleur, ASCII)
// sont reconnus par leur source. Tout keyword non mappé retombe sur le message ajv brut (jamais muet).

const COLOR_PATTERN = '^#[0-9A-Fa-f]{6}$';
const ASCII_PATTERN = '^[\\x00-\\x7F]*$';

const FR_TYPE = { integer: 'entier', number: 'nombre', string: 'texte', boolean: 'booléen', object: 'objet', array: 'liste' };
const frType = t => FR_TYPE[t] || t;

// "/pages/0/place/2/dx" -> "page 1 › élément 3 › dx" ; "/components/cpu" -> "composant › cpu".
export function humanizePath(instancePath) {
  if (!instancePath) return 'racine';
  const parts = instancePath.split('/').filter(Boolean);
  const out = [];
  for (let i = 0; i < parts.length; i++) {
    const seg = parts[i];
    if (seg === 'pages' && /^\d+$/.test(parts[i + 1])) { out.push(`page ${Number(parts[i + 1]) + 1}`); i++; }
    else if (seg === 'place' && /^\d+$/.test(parts[i + 1])) { out.push(`élément ${Number(parts[i + 1]) + 1}`); i++; }
    else if (seg === 'components') out.push('composant');
    else out.push(seg);
  }
  return out.join(' › ');
}

export function humanizeAjvError(e) {
  const where = humanizePath(e.instancePath);
  switch (e.keyword) {
    case 'pattern':
      if (e.params?.pattern === COLOR_PATTERN) return `${where} : doit être une couleur au format #RRGGBB`;
      if (e.params?.pattern === ASCII_PATTERN) return `${where} : doit rester en ASCII (pas d'accents ni de symboles spéciaux)`;
      return `${where} : format invalide`;
    case 'enum': {
      const vals = (e.params?.allowedValues || []).join(', ');
      return `${where} : valeur non autorisée${vals ? ` (au choix : ${vals})` : ''}`;
    }
    case 'additionalProperties':
      return `${where} : propriété inconnue « ${e.params?.additionalProperty} »`;
    case 'required':
      return `${where} : propriété obligatoire « ${e.params?.missingProperty} » manquante`;
    case 'type':
      return `${where} : doit être de type ${frType(e.params?.type)}`;
    case 'minProperties':
      return `${where} : au moins une entrée requise`;
    case 'minimum':
      return `${where} : doit être ≥ ${e.params?.limit}`;
    case 'maximum':
      return `${where} : doit être ≤ ${e.params?.limit}`;
    case 'oneOf':
      return `${where} : type de composant non reconnu ou propriétés incohérentes`;
    default:
      return `${where} ${e.message || ''}`.trim();
  }
}
```

- [ ] **Step 4 : lancer, vérifier le succès** — `node --test tests/humanize.test.js` → PASS (8). Puis `node --test` → **70 tests**.

- [ ] **Step 5 : brancher `humanize` dans `js/validate.js`** — remplacer **tout** le contenu de `designer/js/validate.js` par :
```js
// Validation du layout : forme (ajv contre le schema) + invariants sémantiques (refs).
// Le schema définit le FORMAT ; la résolution des placement.ref est une contrainte
// sémantique non exprimable en JSON Schema, ajoutée ici (miroir du firmware).
// Les messages ajv sont humanisés (humanize.js) pour le panneau d'erreurs.
import Ajv from '../vendor/ajv.min.js';
import { humanizeAjvError } from './humanize.js';

export function createValidator(schema) {
  const ajv = new Ajv({ allErrors: true, strict: false });
  const validateShape = ajv.compile(schema);
  return function validate(layout) {
    const errors = [];
    if (!validateShape(layout)) {
      for (const e of validateShape.errors) {
        // Bruit oneOf : chaque type de composant compare /type à sa constante ; on supprime ces
        // mismatchs de discriminant et on garde le message de synthèse oneOf (+ la vraie erreur
        // de propriété, additionalProperties/required, qui reste).
        if (e.keyword === 'const' && e.instancePath.endsWith('/type')) continue;
        errors.push(humanizeAjvError(e));
      }
    }
    const ids = new Set(Object.keys(layout?.components || {}));
    (layout?.pages || []).forEach((p, pi) => {
      (p?.place || []).forEach(pl => {
        if (pl && pl.ref !== undefined && !ids.has(pl.ref)) errors.push(`page ${pi + 1} : référence inconnue « ${pl.ref} »`);
      });
    });
    return { valid: errors.length === 0, errors: [...new Set(errors)] };  // dedupe les doublons humanisés
  };
}
```

- [ ] **Step 6 : adapter `tests/validate.test.js` aux messages humanisés** — les assertions sur les sous-chaînes brutes changent. Remplacer :
```js
test("ref de placement non résolue → invalide (sémantique, hors JSON Schema)", () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.pages[0].place[0].ref = 'ghost';
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes("ref inconnue 'ghost'")));
});
```
par :
```js
test("ref de placement non résolue → invalide (sémantique, hors JSON Schema)", () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.pages[0].place[0].ref = 'ghost';
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes('ghost')));        // message humanisé : « page 1 : référence inconnue « ghost » »
});
```
Puis remplacer :
```js
test('erreurs de forme ET sémantique coexistent (pas de court-circuit)', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.background = 'red';               // erreur de forme
  bad.pages[0].place[0].ref = 'ghost';  // erreur sémantique
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes('/background')));
  assert.ok(r.errors.some(e => e.includes("ref inconnue 'ghost'")));
});
```
par :
```js
test('erreurs de forme ET sémantique coexistent (pas de court-circuit)', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.background = 'red';               // erreur de forme
  bad.pages[0].place[0].ref = 'ghost';  // erreur sémantique
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes('background')));   // « background : doit être une couleur #RRGGBB »
  assert.ok(r.errors.some(e => e.includes('ghost')));
});
```
(Les autres tests — défaut valide, type inconnu invalide, couleur invalide, ref absente sans `undefined` — restent valides tels quels : ils n'assertent que `valid` ou l'absence de `'undefined'`.)

- [ ] **Step 7 : lancer, vérifier** — `node --test tests/validate.test.js` → PASS (6). `node --test` → **70 tests**, tout vert.

- [ ] **Step 8 : confirmer le rendu sur de VRAIES erreurs ajv** (le bundle vendorisé doit produire les `params` attendus). Depuis `designer/` :
```bash
node --input-type=module -e "import {createValidator} from './js/validate.js'; import {readFileSync} from 'node:fs'; const schema=JSON.parse(readFileSync(new URL('../schema/layout.schema.json',import.meta.url))); const v=createValidator(schema); const bad={title:'t', background:'red', components:{cpu:{type:'readout', text:'oops'}}, pages:[{name:'P', place:[{ref:'ghost', anchor:'NOPE'}]}]}; console.log(v(bad).errors.join('\n'));"
```
Expected (à quelques mots près) — **lisible, aucun `must match pattern` brut, aucun bruit `/type must be equal to constant`** :
```
background : doit être une couleur au format #RRGGBB
composant › cpu : type de composant non reconnu ou propriétés incohérentes
composant › cpu : propriété inconnue « text »
page 1 › élément 1 › anchor : valeur non autorisée (au choix : CENTER, TOP_MID, ...)
page 1 : référence inconnue « ghost »
```
Si un `params.*` diffère dans le bundle ajv (p.ex. `additionalProperty`), ajuster `humanize.js` puis ré-exécuter ce step + `node --test`.

- [ ] **Step 9 : Commit**
```bash
git add designer/js/humanize.js designer/tests/humanize.test.js designer/js/validate.js designer/tests/validate.test.js
git commit -m "Rich_Telemetry: designer humanize ajv errors (validate panel) + tests"
```

---

## Task 3 : `canvas.js` + `inspector.js` — propager la page active (refactor préparatoire)

**Files:**
- Modify: `designer/js/canvas.js`
- Modify: `designer/js/inspector.js`
- Modify: `designer/js/app.js`

> Vérification navigateur. Aucune nouvelle UI : on introduit la **page active** (défaut 0) dans le canvas et on propage l'index partout (canvas + inspector). Avec une seule page, le comportement est **identique** à C1 (régression à confirmer). On corrige aussi le flash de delete (C1-c) et on durcit l'inspecteur contre une sélection devenue obsolète.

- [ ] **Step 1 : `canvas.js` — introduire `activePage`** — remplacer :
```js
export function createCanvas({ stage, badges }, model, { onSelect } = {}) {
  let selected = null; // index du placement sélectionné sur la page active

  const placements = () => model.state.pages?.[0]?.place ?? [];
```
par :
```js
export function createCanvas({ stage, badges }, model, { onSelect } = {}) {
  let selected = null;    // index du placement sélectionné sur la page active
  let activePage = 0;     // page affichée par le canvas (source de vérité de l'éditeur, hors layout)

  const placements = () => model.state.pages?.[activePage]?.place ?? [];
```

- [ ] **Step 2 : `canvas.js` — page active dans les 3 closures de commit** — remplacer les trois occurrences `s.pages[0].place[i]`.

  (a) drag (label/readout/bar) :
```js
      if (live) model.commit(s => {                    // commit unique, pas par frame
        const q = s.pages[0].place[i];
        q.anchor = live.anchor; q.dx = live.dx; q.dy = live.dy;
      });
```
→
```js
      if (live) model.commit(s => {                    // commit unique, pas par frame
        const q = s.pages[activePage].place[i];
        q.anchor = live.anchor; q.dx = live.dx; q.dy = live.dy;
      });
```
  (b) resize bar (ligne unique) — remplacer :
```js
        if (dim) model.commit(s => { const q = s.pages[0].place[i]; q.width = dim.width; q.height = dim.height; });
```
→
```js
        if (dim) model.commit(s => { const q = s.pages[activePage].place[i]; q.width = dim.width; q.height = dim.height; });
```
  (c) resize ring :
```js
          if (moved) model.commit(s => {
            const q = s.pages[0].place[i];
            q.radius = g.r; q.thickness = g.th; q.gap_deg = g.gap;
          });
```
→
```js
          if (moved) model.commit(s => {
            const q = s.pages[activePage].place[i];
            q.radius = g.r; q.thickness = g.th; q.gap_deg = g.gap;
          });
```

- [ ] **Step 3 : `canvas.js` — exposer `setPage` + `getActivePage`** — remplacer la fin du module :
```js
  model.subscribe(render);
  render();
  // La webfont Montserrat (font-display:swap) charge en asynchrone : le 1er render mesure
  // avant le swap → centrage à ~8px près. Re-render une fois la police prête (fidélité).
  if (document.fonts?.ready) document.fonts.ready.then(render);
  return { render, getSelected: () => selected, selectPlacement: select };
}
```
par :
```js
  // Change la page affichée. On désélectionne (un index de placement n'a pas de sens d'une page à
  // l'autre — cf. Décisions C2, on désélectionne plutôt que de re-keyer) puis on re-rend.
  function setPage(i) {
    activePage = i;
    selected = null;
    render();
    onSelect && onSelect(null);
  }

  model.subscribe(render);
  render();
  // La webfont Montserrat (font-display:swap) charge en asynchrone : le 1er render mesure
  // avant le swap → centrage à ~8px près. Re-render une fois la police prête (fidélité).
  if (document.fonts?.ready) document.fonts.ready.then(render);
  return { render, getSelected: () => selected, selectPlacement: select, setPage, getActivePage: () => activePage };
}
```

- [ ] **Step 4 : `inspector.js` — recevoir `getActivePage`, lire la page active** — remplacer :
```js
export function createInspector(root, model, { rerenderCanvas, clearSelection } = {}) {
  let sel = null; // { placeIndex, ref } ou null

  const comp = () => sel && model.state.components[sel.ref];
  const place = () => sel && model.state.pages[0].place[sel.placeIndex];
```
par :
```js
export function createInspector(root, model, { rerenderCanvas, clearSelection, getActivePage = () => 0 } = {}) {
  let sel = null; // { placeIndex, ref } ou null

  const comp = () => sel && model.state.components[sel.ref];
  const place = () => sel && model.state.pages?.[getActivePage()]?.place?.[sel.placeIndex];
```

- [ ] **Step 5 : `inspector.js` — `setPlacementProp` sur la page active** — remplacer :
```js
        const input = makeInput(kind, p[key], v => model.commit(s => setPlacementProp(s, 0, sel.placeIndex, key, v)));
```
par :
```js
        const input = makeInput(kind, p[key], v => model.commit(s => setPlacementProp(s, getActivePage(), sel.placeIndex, key, v)));
```

- [ ] **Step 6 : `inspector.js` — garder une sélection obsolète + fix flash delete** — remplacer le début de `render()` :
```js
    root.querySelectorAll('.insp-body').forEach(n => n.remove());
    const c = comp();
    const body = document.createElement('div');
    body.className = 'insp-body';
    if (!c) {
      const p = document.createElement('p'); p.className = 'todo'; p.textContent = 'Sélectionne un widget sur le canvas.';
      body.appendChild(p); root.appendChild(body); return;
    }
```
par :
```js
    root.querySelectorAll('.insp-body').forEach(n => n.remove());
    const c = comp();
    const p = place();
    const body = document.createElement('div');
    body.className = 'insp-body';
    if (!c || !p) {                               // sélection absente ou devenue obsolète (page changée, undo…)
      const para = document.createElement('p'); para.className = 'todo'; para.textContent = 'Sélectionne un widget sur le canvas.';
      body.appendChild(para); root.appendChild(body); return;
    }
```
Puis remplacer le handler de suppression (désélectionner **avant** le commit — note C1-c — et viser la page active) :
```js
    del.addEventListener('click', () => {
      const i = sel.placeIndex;
      model.commit(s => removePlacement(s, 0, i));
      sel = null;
      clearSelection && clearSelection(); // désélectionne le canvas
    });
```
par :
```js
    del.addEventListener('click', () => {
      const i = sel.placeIndex;
      sel = null;
      clearSelection && clearSelection();                 // désélectionne AVANT le commit (évite le flash, note C1-c)
      model.commit(s => removePlacement(s, getActivePage(), i));
    });
```

- [ ] **Step 7 : `app.js` — passer `getActivePage` à l'inspecteur** — remplacer :
```js
  inspector = createInspector($('inspector'), model, {
    rerenderCanvas: canvas.render,
    clearSelection: () => canvas.selectPlacement(null)
  });
```
par :
```js
  inspector = createInspector($('inspector'), model, {
    rerenderCanvas: canvas.render,
    clearSelection: () => canvas.selectPlacement(null),
    getActivePage: canvas.getActivePage
  });
```

- [ ] **Step 8 : vérification navigateur (régression)** — servir depuis `Rich_Telemetry/` (`python3 -m http.server 8000`), ouvrir `http://localhost:8000/designer/`. Le comportement doit être **identique à C1** :
  1. rendu démo correct (anneau `72%`/`5h00`, `CPU 42 %`, barre RAM, badges), `✓ valide` ;
  2. sélection + drag d'un widget = **1 seul** undo ; resize bar/ring OK ;
  3. inspecteur : props + géométrie + seuils + mock OK ; édition = 1 undo ;
  4. **Supprimer de la page** sur un widget **non-dernier** (p.ex. `cpu`) → pas de flash de liseré sur un autre widget ; il disparaît, l'inspecteur revient à vide, Undo le restaure ;
  5. aucune erreur console (hors favicon 404).
  Expected : tous OK.

- [ ] **Step 9 : `node --test` (toujours 70) puis Commit**
```bash
node --test
git add designer/js/canvas.js designer/js/inspector.js designer/js/app.js
git commit -m "Rich_Telemetry: designer canvas+inspector — propagate active page; harden stale selection; fix delete flash"
```

---

## Task 4 : `pages.js` — onglets de pages (CRUD + réordonner)

**Files:**
- Create: `designer/js/pages.js`
- Modify: `designer/index.html` (barre de pages)
- Modify: `designer/style.css` (styles onglets)
- Modify: `designer/js/app.js` (instancier les onglets)

> Vérification navigateur. Un onglet par page (clic = page active). Une grappe de contrôles agit **sur la page active** : ajouter, renommer (édition inline, **pas de `prompt()`** — bloquerait le pilotage navigateur), déplacer ◀/▶, supprimer (désactivé s'il ne reste qu'une page). La page active vit dans le canvas (`getActivePage`/`setPage`).

- [ ] **Step 1 : ajouter la barre de pages dans `index.html`** — insérer, **entre `</header>` et `<main>`** :
```html
  <nav id="pages" class="pages-bar"></nav>
```
(Le `</header>` actuel est suivi directement de `<main>` ; la `<nav>` se glisse entre les deux.)

- [ ] **Step 2 : créer `js/pages.js`** :
```js
// Onglets de pages. Un onglet par page (clic = page active). Les contrôles agissent sur la page
// ACTIVE : + Page (ajoute en fin et l'active), Renommer (édition inline du nom, pas de prompt()),
// ◀/▶ (réordonne la page active), Supprimer (désactivé s'il ne reste qu'une page). La page active
// vit dans le canvas (source de vérité unique), lue via getActivePage et pilotée via setPage.
import { addPage, removePage, renamePage, reorderPages } from './mutations.js';

function mkBtn(text, onClick, cls) {
  const b = document.createElement('button');
  b.className = 'page-btn' + (cls ? ' ' + cls : '');
  b.textContent = text;
  b.addEventListener('click', onClick);
  return b;
}

export function createPages(root, model, { getActivePage, setPage } = {}) {
  let renaming = null; // index de la page en cours de renommage inline, ou null

  // Backstop : après removePage (ou undo/import), l'index actif peut dépasser la liste → on le ramène.
  function clampActive() {
    const n = model.state.pages?.length ?? 0;
    if (n && getActivePage() > n - 1) setPage(n - 1);
  }

  function render() {
    clampActive();
    root.replaceChildren();
    const pages = model.state.pages || [];
    const active = getActivePage();

    const tabs = document.createElement('div');
    tabs.className = 'page-tabs';
    pages.forEach((p, i) => {
      if (renaming === i) {
        const inp = document.createElement('input');
        inp.className = 'page-rename';
        inp.value = p.name || '';
        inp.addEventListener('change', () => {
          const name = inp.value.trim() || `Page ${i + 1}`;
          renaming = null;
          model.commit(s => renamePage(s, i, name));   // → subscribe → render()
        });
        inp.addEventListener('blur', () => { if (renaming === i) { renaming = null; render(); } });
        tabs.appendChild(inp);
        queueMicrotask(() => inp.focus());
      } else {
        const tab = document.createElement('button');
        tab.className = 'page-tab' + (i === active ? ' active' : '');
        tab.textContent = p.name || `Page ${i + 1}`;
        tab.addEventListener('click', () => { setPage(i); render(); });
        tabs.appendChild(tab);
      }
    });
    root.appendChild(tabs);

    const ctrls = document.createElement('div');
    ctrls.className = 'page-ctrls';

    ctrls.appendChild(mkBtn('+ Page', () => {
      model.commit(s => addPage(s, `Page ${s.pages.length + 1}`));
      setPage(model.state.pages.length - 1);
      render();
    }));

    ctrls.appendChild(mkBtn('Renommer', () => { renaming = active; render(); }));

    const left = mkBtn('◀', () => {
      if (active <= 0) return;
      model.commit(s => reorderPages(s, active, active - 1));
      setPage(active - 1);
      render();
    });
    left.disabled = active <= 0;
    ctrls.appendChild(left);

    const right = mkBtn('▶', () => {
      if (active >= pages.length - 1) return;
      model.commit(s => reorderPages(s, active, active + 1));
      setPage(active + 1);
      render();
    });
    right.disabled = active >= pages.length - 1;
    ctrls.appendChild(right);

    const del = mkBtn('Supprimer', () => {
      if (pages.length <= 1) return;                       // garder au moins une page
      model.commit(s => removePage(s, active));
      setPage(Math.min(active, model.state.pages.length - 1));
      render();
    }, 'page-del');
    del.disabled = pages.length <= 1;
    ctrls.appendChild(del);

    root.appendChild(ctrls);
  }

  model.subscribe(render);
  render();
  return { render };
}
```

- [ ] **Step 3 : styles des onglets — ajouter à la fin de `style.css`** :
```css
/* --- Plan C2 : onglets de pages --- */
.pages-bar { display: flex; align-items: center; gap: 12px; padding: 6px 12px; border-bottom: 1px solid var(--line); flex-wrap: wrap; }
.page-tabs { display: flex; gap: 4px; flex-wrap: wrap; }
.page-tab {
  background: #0f1a2b; border: 1px solid var(--line); color: var(--muted);
  border-radius: 6px 6px 0 0; padding: 4px 12px; cursor: pointer; font-size: 12.5px;
}
.page-tab.active { color: var(--ink); border-color: var(--accent); background: var(--panel); }
.page-rename { width: 120px; }
.page-ctrls { display: flex; gap: 4px; }
.page-btn { padding: 3px 9px; font-size: 12px; }
.page-btn:disabled { border-color: var(--line); color: #475569; cursor: default; }
.page-del { border-color: var(--err); color: var(--err); }
.page-del:disabled { border-color: var(--line); color: #475569; }
```

- [ ] **Step 4 : instancier les onglets dans `app.js`** — ajouter l'import sous les imports existants :
```js
import { createPages } from './pages.js';
```
Puis, **juste après** le bloc `createPalette(...)` (ou après l'inspecteur si la palette n'est pas encore modifiée — l'ordre des vues n'importe pas), ajouter :
```js
  // Onglets de pages : sélectionner la page active + CRUD + réordonner.
  const pages = createPages($('pages'), model, {
    getActivePage: canvas.getActivePage,
    setPage: i => canvas.setPage(i)
  });
```
(La référence `pages` sera réutilisée par l'export/import en Task 6 ; si Task 6 n'est pas encore faite, `pages` reste simplement instancié.)

- [ ] **Step 5 : vérification navigateur** — servir depuis `Rich_Telemetry/`, ouvrir le designer. Vérifier :
  1. La barre montre **un onglet « Page 1 »** actif (liseré bleu) + les contrôles `+ Page` / `Renommer` / `◀` / `▶` / `Supprimer` ; `◀`, `▶`, `Supprimer` **désactivés** (une seule page).
  2. **+ Page** → un onglet « Page 2 » apparaît, devient actif ; le **canvas se vide** (page 2 sans placement) ; le JSON avancé montre 2 entrées dans `pages`.
  3. Glisser un widget de la palette (Task C1) sur la page 2 → il s'ajoute à `pages[1].place` (pas page 1) ; revenir sur l'onglet **Page 1** → on retrouve les widgets d'origine, pas ceux de la page 2.
  4. **Renommer** (page active) → l'onglet devient un champ ; taper `Accueil` + Entrée → l'onglet affiche `Accueil` ; JSON reflète `name: "Accueil"` ; **1 seul** Undo.
  5. **◀ / ▶** réordonnent la page active (l'onglet actif se déplace, reste actif) ; JSON reflète le nouvel ordre.
  6. **Supprimer** la page active (avec ≥ 2 pages) → elle disparaît, l'actif retombe dans les bornes ; Undo la restaure.
  7. `✓ valide` à tout moment ; aucune erreur console.
  Expected : tous OK.

- [ ] **Step 6 : `node --test` (toujours 70) puis Commit**
```bash
node --test
git add designer/js/pages.js designer/index.html designer/style.css designer/js/app.js
git commit -m "Rich_Telemetry: designer pages tabs (CRUD + reorder, active-page canvas)"
```

---

## Task 5 : `palette.js` — bibliothèque de composants partagés inter-pages

**Files:**
- Modify: `designer/js/palette.js`
- Modify: `designer/style.css` (styles bibliothèque)
- Modify: `designer/js/app.js` (passer `getActivePage` à la palette)

> Vérification navigateur. La palette gagne une section **Bibliothèque** listant les composants déjà définis. Glisser un composant existant sur le canvas le **place** sur la page active en **partageant** le même `id` (pas une copie). Tous les drops (créateurs de type **et** bibliothèque) ciblent désormais la **page active**.

- [ ] **Step 1 : remplacer `js/palette.js`** par la version C2 (créateurs + bibliothèque + page active) :
```js
// Palette : (1) 6 créateurs de type — glisser sur le #stage crée un composant + un placement au point
// de dépôt ; (2) Bibliothèque des composants déjà définis — glisser un existant sur le #stage le PLACE
// (partage : même id/état, pas une copie) sur la page ACTIVE. Drop = UN commit, puis sélection du
// nouveau placement. Vérifié au navigateur. (Pages = pages.js ; valeurs d'aperçu = mocks.js.)
import { uniqueId, addComponent, addPlacement, DEFAULTS } from './mutations.js';
import { snapPlacement } from './geometry.js';

const TYPES = [
  ['label', 'Label'], ['readout', 'Lecture'], ['bar', 'Barre'],
  ['ring', 'Anneau'], ['led_ring', 'LED ring'], ['sound', 'Son']
];

// Placement initial selon le type. Ring centré ; led_ring/sound sans géométrie ; widgets écran :
// ancrage + offset déduits du point de dépôt (boîte ~0, affinable au drag).
function makePlacement(type, id, x, y) {
  if (type === 'ring') return { ref: id, radius: 80, thickness: 16, gap_deg: 70 };
  if (type === 'led_ring' || type === 'sound') return { ref: id };
  const { anchor, dx, dy } = snapPlacement(x, y, 0, 0, 16);
  return { ref: id, anchor, dx, dy };
}

export function createPalette(root, model, { stage, getActivePage, onCreated } = {}) {
  const page = () => (getActivePage ? getActivePage() : 0);

  // --- Section créateurs de type (statique) ---
  const list = document.createElement('div');
  list.className = 'palette-list';
  for (const [type, libelle] of TYPES) {
    const item = document.createElement('div');
    item.className = 'palette-item';
    item.draggable = true;
    item.dataset.type = type;
    item.textContent = libelle;
    item.addEventListener('dragstart', e => e.dataTransfer.setData('text/rt-type', type));
    list.appendChild(item);
  }
  root.appendChild(list);

  // --- Section bibliothèque (dynamique : reflète components) ---
  const libTitle = document.createElement('div');
  libTitle.className = 'lib-title';
  libTitle.textContent = 'Bibliothèque';
  root.appendChild(libTitle);
  const libList = document.createElement('div');
  libList.className = 'lib-list';
  root.appendChild(libList);

  function renderLibrary() {
    libList.replaceChildren();
    const comps = model.state.components || {};
    const ids = Object.keys(comps);
    if (!ids.length) {
      const empty = document.createElement('div');
      empty.className = 'lib-empty';
      empty.textContent = 'Aucun composant défini.';
      libList.appendChild(empty);
      return;
    }
    for (const id of ids) {
      const item = document.createElement('div');
      item.className = 'lib-item';
      item.draggable = true;
      const name = document.createElement('span'); name.textContent = id;
      const type = document.createElement('span'); type.className = 'lib-type'; type.textContent = comps[id].type;
      item.appendChild(name); item.appendChild(type);
      item.addEventListener('dragstart', e => e.dataTransfer.setData('text/rt-ref', id));
      libList.appendChild(item);
    }
  }
  model.subscribe(renderLibrary);
  renderLibrary();

  // --- Cible de drop : crée (type) ou place un existant (ref), sur la page active ---
  stage.addEventListener('dragover', e => {
    const t = e.dataTransfer.types;
    if (t.includes('text/rt-type') || t.includes('text/rt-ref')) e.preventDefault();
  });
  stage.addEventListener('drop', e => {
    const type = e.dataTransfer.getData('text/rt-type');
    const ref = e.dataTransfer.getData('text/rt-ref');
    if (!type && !ref) return;
    e.preventDefault();
    const r = stage.getBoundingClientRect();
    const x = e.clientX - r.left, y = e.clientY - r.top; // coords écran (1:1)
    const pi = page();
    let newIndex;
    model.commit(s => {
      if (type) {
        const id = uniqueId(s, type);
        addComponent(s, id, DEFAULTS[type]());
        addPlacement(s, pi, makePlacement(type, id, x, y));
      } else {
        const existing = s.components[ref];
        if (!existing) return;                            // ref disparue : rien à placer
        addPlacement(s, pi, makePlacement(existing.type, ref, x, y));
      }
      newIndex = s.pages[pi].place.length - 1;
    });
    if (newIndex != null) onCreated && onCreated(newIndex);
  });
}
```

- [ ] **Step 2 : styles bibliothèque — ajouter à la fin de `style.css`** :
```css
/* --- Plan C2 : bibliothèque de composants --- */
.lib-title { font-size: 11px; text-transform: uppercase; color: var(--muted); margin: 14px 0 6px; }
.lib-list { display: flex; flex-direction: column; gap: 5px; }
.lib-item {
  padding: 6px 9px; border: 1px solid var(--line); border-radius: 6px; background: #0a1424;
  cursor: grab; user-select: none; font-size: 12px; display: flex; justify-content: space-between; gap: 6px;
}
.lib-item:active { cursor: grabbing; }
.lib-item:hover { border-color: var(--accent); }
.lib-item .lib-type { color: var(--muted); font-size: 10.5px; }
.lib-empty { font-size: 11px; color: #475569; font-style: italic; }
```

- [ ] **Step 3 : passer `getActivePage` à la palette dans `app.js`** — remplacer :
```js
  // Palette : glisser un type sur le canvas crée le composant, puis on le sélectionne.
  createPalette($('palette'), model, {
    stage: $('stage'),
    onCreated: i => canvas.selectPlacement(i)
  });
```
par :
```js
  // Palette : glisser un type (création) ou un composant de la bibliothèque (partage) sur le canvas,
  // sur la page active, puis sélection du nouveau placement.
  createPalette($('palette'), model, {
    stage: $('stage'),
    getActivePage: canvas.getActivePage,
    onCreated: i => canvas.selectPlacement(i)
  });
```

- [ ] **Step 4 : vérification navigateur** — servir depuis `Rich_Telemetry/`, ouvrir le designer. Vérifier :
  1. Sous la palette des 6 types, une section **Bibliothèque** liste les composants du layout démo (`titre`, `cpu`, `ram`, `jauge`, `led`, `buzz`) avec leur type.
  2. **+ Page** (Task 4) → aller sur la page 2 (vide). Glisser **`cpu`** depuis la Bibliothèque sur le canvas → un readout `CPU 42 %` apparaît sur la page 2 ; le JSON montre un **placement** `{ref:"cpu", …}` ajouté à `pages[1].place`, **sans** nouveau composant (`components.cpu` inchangé, pas de `cpu1`).
  3. Éditer la couleur de `cpu` via l'inspecteur sur **une** page → le changement se voit sur **les deux** pages (état partagé) : revenir sur page 1, `cpu` a la nouvelle couleur.
  4. Créer un composant via un **créateur de type** (p.ex. Barre) → il apparaît **aussitôt** dans la Bibliothèque (liste dynamique).
  5. Glisser `led`/`buzz` (badges) depuis la Bibliothèque → un badge s'ajoute sous le canvas de la page active.
  6. `✓ valide` ; Undo annule un placement de bibliothèque (placement seul) ; aucune erreur console.
  Expected : tous OK.

- [ ] **Step 5 : Commit**
```bash
git add designer/js/palette.js designer/style.css designer/js/app.js
git commit -m "Rich_Telemetry: designer palette library (share existing components across pages)"
```

---

## Task 6 : `file-io.js` — export / import `layout.json`

**Files:**
- Create: `designer/js/file-io.js`
- Modify: `designer/index.html` (boutons + input fichier)
- Modify: `designer/js/app.js` (câbler l'export/import)

> Vérification navigateur. **Exporter** sérialise le modèle et déclenche un téléchargement `layout.json`. **Importer** lit un fichier et le charge dans le modèle (la validation live du panneau JSON signalera un layout non conforme). Filet **indépendant du device** (pas de CORS).

- [ ] **Step 1 : ajouter les contrôles dans `index.html`** — dans le `<header>`, remplacer :
```html
    <button id="load">Charger</button>
    <button id="push">Pousser</button>
    <span id="status" class="status"></span>
```
par :
```html
    <button id="load">Charger</button>
    <button id="push">Pousser</button>
    <button id="export">Exporter</button>
    <button id="import">Importer</button>
    <input id="import-file" type="file" accept="application/json,.json" hidden />
    <span id="status" class="status"></span>
```

- [ ] **Step 2 : créer `js/file-io.js`** :
```js
// Export / import du layout.json en fichier local — filet indépendant du device (pas de CORS).
// Export : sérialise le modèle → téléchargement. Import : lit un fichier → charge dans le modèle ;
// la validité de forme est signalée par le panneau JSON. onLoad permet de réinitialiser la vue
// (page active, sélection) après un import. Vérifié au navigateur.
export function bindFileIO(model, { exportBtn, importBtn, importInput, onLoad } = {}) {
  exportBtn.addEventListener('click', () => {
    const blob = new Blob([model.toJSON()], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'layout.json';
    a.click();
    URL.revokeObjectURL(url);
  });

  importBtn.addEventListener('click', () => importInput.click());

  importInput.addEventListener('change', async () => {
    const file = importInput.files?.[0];
    if (!file) return;
    try {
      const text = await file.text();
      model.loadJSON(text);            // throw si JSON illisible ; la forme est validée par le panneau
      onLoad && onLoad();
    } catch (e) {
      const status = document.getElementById('status');
      if (status) { status.textContent = 'Import échoué : ' + e.message; status.className = 'status err'; }
    } finally {
      importInput.value = '';          // réautorise la réimportation du même fichier
    }
  });
}
```

- [ ] **Step 3 : câbler dans `app.js`** — ajouter l'import sous les imports existants :
```js
import { bindFileIO } from './file-io.js';
```
Puis, **après** le bloc `createPages(...)` (la const `pages` de Task 4 est nécessaire), ajouter :
```js
  // Export / import fichier layout.json (filet indépendant du device). Après import, on revient à la
  // page 1 (l'ancienne page active peut ne plus exister) et on rafraîchit les onglets.
  bindFileIO(model, {
    exportBtn: $('export'), importBtn: $('import'), importInput: $('import-file'),
    onLoad: () => { canvas.setPage(0); pages.render(); }
  });
```

- [ ] **Step 4 : vérification navigateur** — servir depuis `Rich_Telemetry/`, ouvrir le designer. Vérifier :
  1. **Exporter** → le navigateur télécharge `layout.json` ; son contenu == le JSON avancé affiché (ouvrir le fichier pour comparer).
  2. Modifier le layout (ajouter une page + un widget), **Exporter** à nouveau → le fichier reflète les changements.
  3. Recharger la page (état revenu au défaut), **Importer** le fichier exporté à l'étape 2 → le layout modifié est restauré (pages + widgets) ; les onglets reflètent les pages importées ; on est sur la **page 1** ; `✓ valide`.
  4. Importer un fichier **JSON illisible** (p.ex. un `.json` tronqué) → le statut header affiche « Import échoué : … » ; l'éditeur n'est pas corrompu (le layout précédent reste).
  5. Importer un JSON **valide en forme mais invalide layout** (p.ex. une couleur `"red"`) → il se charge, le panneau JSON affiche `✗ invalide` + le message **humanisé** (Task 2).
  6. Aucune erreur console.
  Expected : tous OK.

- [ ] **Step 5 : Commit**
```bash
git add designer/js/file-io.js designer/index.html designer/js/app.js
git commit -m "Rich_Telemetry: designer file export/import (layout.json, device-independent)"
```

---

## Task 7 : Documentation — README designer + HANDOFF

**Files:**
- Modify: `designer/README.md`
- Modify: `designer/HANDOFF.md`

> Pas de code. On met l'état de la doc à jour : le designer n'est plus « à construire », il est multi-pages avec export/import. Le spec (lot 10) bundle explicitement « doc dans le README ».

- [ ] **Step 1 : actualiser l'état dans `README.md`** — remplacer le bloc `> État : …` (la citation sous le titre) :
```markdown
> État : **fondation (Plan A) en place** — édition via le panneau *JSON avancé* avec validation live contre le schéma (ajv), undo/redo, et load/push `/layout` vers le device. L'éditeur WYSIWYG visuel (palette, canvas drag-and-drop, rendu des widgets, inspecteur) reste à construire — Plans B/C (voir `specs/` et `plans/`).
```
par :
```markdown
> État : **éditeur WYSIWYG multi-pages complet** (Plans A → C2). Palette + bibliothèque de composants réutilisables, canvas drag-and-drop avec snap aux ancrages, inspecteur (props/géométrie/seuils/aperçu mock), onglets de pages (créer/renommer/réordonner/supprimer), export/import `layout.json`, validation live ajv avec messages humanisés, undo/redo. Le panneau *JSON avancé* reste disponible. Le load/push `/layout` vers le device nécessite le CORS firmware (voir plus bas). Détails : `specs/` et `plans/`.
```

- [ ] **Step 2 : noter export/import dans la section « Endpoints » du `README.md`** — juste **avant** le tableau « Endpoints utilisés », ajouter une ligne :
```markdown
> Sans device (ou en attendant le CORS firmware), utilise **Exporter / Importer** dans l'en-tête pour sauvegarder/recharger un `layout.json` en fichier local.
```

- [ ] **Step 3 : marquer C2 fait dans `HANDOFF.md`** — dans le TL;DR, remplacer :
```markdown
**C2 (pages CRUD + canvas multi-pages + file-io + humanisation ajv + bibliothèque inter-pages) reste à écrire et exécuter**. Tout vit sur la branche **`feat/rt-designer`** (non mergée).
```
par :
```markdown
**C2 (pages CRUD + canvas multi-pages + file-io + humanisation ajv + bibliothèque inter-pages) : implémenté et testé** (`plans/2026-06-16-wysiwyg-editor-plan-c2-pages-fileio.md`). Tout vit sur la branche **`feat/rt-designer`** (non mergée). Reste hors-designer : le **CORS firmware** (Charger/Pousser device) ; et les reportés v2+ du spec (`/update`, `/page`, aperçu animé led_ring, presets).
```

- [ ] **Step 4 : Commit**
```bash
git add designer/README.md designer/HANDOFF.md
git commit -m "Rich_Telemetry: designer docs — README + HANDOFF reflect C2 (multi-pages, file-io)"
```

---

## Self-Review (effectuée à la rédaction)

**1. Couverture (périmètre C2 / spec) :**
- Pages CRUD + réordonner → Task 1 (mutations PUR, TDD) + Task 4 (onglets) ✓.
- Canvas multi-pages (afficher la page active) → Task 3 (`activePage`/`setPage`/`getActivePage`, propagation) ✓.
- Bibliothèque de composants partagés inter-pages → Task 5 ✓.
- Export / import fichier → Task 6 ✓.
- Humanisation des erreurs ajv → Task 2 (PUR, TDD) ✓.
- Refactors reportés : sélection re-keyée *par désélection au changement de page* (B-a) → Task 3 ; canvas dé-câblé de `pages[0]` (B-b) → Task 3 ; mocks order-dep (C1-b) → Task 1 ; flash delete (C1-c) → Task 3 ✓.
- Reportés assumés : GC mocks (C1-d, non nécessaire en C2), `/update`/`/page`, aperçu animé, presets, édition `title`/`background`/`nav` (JSON avancé) — documentés.

**2. Placeholders :** aucun. Tout le code exécutable est fourni verbatim. Les vérifs navigateur listent des critères observables précis.

**3. Cohérence des types/symboles :**
- `mutations.js` exporte en plus `addPage(state,name)` / `removePage(state,i)` / `renamePage(state,i,name)` / `reorderPages(state,from,to)` — signatures identiques entre Task 1 (impl+tests), `pages.js` (Task 4).
- `canvas.js` expose `setPage(i)` + `getActivePage()` (Task 3) — consommés par `app.js`, `pages.js` (`setPage`/`getActivePage`), `palette.js` (`getActivePage`), `file-io.js` (`canvas.setPage(0)`).
- `createInspector(root, model, {rerenderCanvas, clearSelection, getActivePage})` (Task 3) — câblé tel quel dans `app.js` ; `place()`/`setPlacementProp`/`removePlacement` utilisent `getActivePage()`.
- `createPages(root, model, {getActivePage, setPage}) → {render}` (Task 4) — `pages.render` réutilisé par `bindFileIO.onLoad` (Task 6).
- `createPalette(root, model, {stage, getActivePage, onCreated})` (Task 5) — `getActivePage` ajouté côté `app.js`.
- `bindFileIO(model, {exportBtn, importBtn, importInput, onLoad})` (Task 6) — ids `export`/`import`/`import-file` présents dans `index.html`.
- `humanizeAjvError(e)` / `humanizePath(path)` (Task 2) — consommés par `validate.js`.
- `app.js` ordre d'instanciation : `canvas` → `inspector` (a besoin de `canvas.getActivePage`) → `palette` → `pages` (const réutilisée) → `bindFileIO`. Cohérent (canvas en premier, source de vérité de la page active).

**4. Conventions :** TDD `node --test` pour le pur (mutations pages, humanize) ; vérif navigateur pour le DOM (pages, palette, file-io) et pour le refactor multi-pages (Task 3, régression) — calque A/B/C1. Modules ES zéro-build. Commits fréquents. Édition committée sur `change` (pas par frappe) pour le rename (1 undo). Compteurs de tests : 57 (départ) → 62 (Task 1) → 70 (Task 2).

---

## Décisions prises (à connaître)

- **Page active hors layout, source de vérité dans `canvas.js`.** Le device n'a pas besoin de savoir quelle page le designer édite ; la mettre dans `model.state` la pousserait au device et la sérialiserait. Elle vit donc dans le canvas, lue/pilotée via `getActivePage`/`setPage`. Pas de nouvel « observable » : `pages.js` re-rend ses onglets localement après chaque action et au besoin via `pages.render()` (import). Choix de simplicité assumé (Rule 2) vs un store de session dédié.
- **Désélection au changement de page (au lieu de re-keyer la sélection sur une référence stable).** L'index de sélection n'a pas de sens d'une page à l'autre ; plutôt que de porter une référence stable (id/objet), `setPage` désélectionne. La sélection n'a pas à survivre à un changement de page. Règle la note B-a sans complexité.
- **Réordonnancement par ◀/▶ (pas de drag-and-drop d'onglets).** UX minimale et testable, sans piège de drag-ghost (cf. HANDOFF). « Réordonner » = déplacer la page active d'un cran ; l'actif suit la page.
- **Rename inline, pas de `prompt()`/`confirm()`.** Les dialogues modaux bloquent le pilotage navigateur (cf. consignes Chrome) et l'UX. Le rename édite un `<input>` inline, committé sur `change`.
- **Au moins une page conservée.** « Supprimer » est désactivé quand il ne reste qu'une page (évite un dashboard à zéro page et tout besoin de confirmation).
- **Partage, pas copie.** Glisser un composant de la bibliothèque ajoute un *placement* référant le même `id` (état partagé entre pages), conformément au schéma (`components` = map d'exemplaires uniques).
- **Bruit `oneOf` filtré.** À la validation, les mismatchs de discriminant `/<…>/type` (const) sont supprimés ; on garde le message de synthèse `oneOf` + la vraie erreur de propriété. Les messages identiques sont dédupliqués. Limite assumée : un composant à propriété invalide peut afficher 2 lignes (synthèse oneOf + propriété précise).
- **GC du store de mocks toujours déféré (C1-d).** C2 n'introduit ni rename ni suppression de composant ; le partage inter-pages n'orpheline aucun mock. Inutile d'ajouter un GC maintenant.

---

## Execution Handoff

Plan complet et sauvegardé dans `designer/plans/2026-06-16-wysiwyg-editor-plan-c2-pages-fileio.md`. Deux options :

1. **Subagent-Driven (recommandé)** — un subagent/tâche, revue spec puis qualité, vérif navigateur du DOM par le contrôleur (Playwright), en servant depuis `Rich_Telemetry/`. (Process suivi pour A, B et C1.)
2. **Inline** — exécution dans cette session, par lots avec checkpoints.

**Astuces process (rodées sur A/B/C1)** : hook pre-commit `SCHEMA DIVERGENT` jaune **non bloquant** (pré-existant, sans rapport — C2 ne touche pas le schéma) ; vérif navigateur via Playwright MCP en pilotant de vrais events (un `pointerdown` synthétique sans `pointerup` laisse un drag fantôme — faire des clics complets ; le DnD HTML5 se pilote via un `DataTransfer` synthétique) ; nettoyer les `*.png` de test laissés à la racine du repo.

Après C2 : l'éditeur WYSIWYG est complet côté designer. Reste **hors designer** le **CORS firmware** (branche embarqué, commit dédié) pour débloquer Charger/Pousser device ; et les reportés v2+ du spec.
