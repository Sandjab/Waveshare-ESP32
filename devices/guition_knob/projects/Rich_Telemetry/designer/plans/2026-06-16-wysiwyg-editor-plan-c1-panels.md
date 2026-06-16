# Rich_Telemetry Designer — Plan C1 : Édition (palette + inspecteur) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Faire du designer un **éditeur mono-page complet sans toucher au JSON** : créer un composant en glissant un type de la palette sur le canvas, et éditer toutes ses propriétés + sa géométrie + ses seuils + sa valeur d'aperçu dans un inspecteur, via des **mutations dédiées testées** de `model.js`.

**Architecture:** On prolonge la séparation Plan A/B : la logique de mutation pure va dans un nouveau `mutations.js` (DOM-free, testé `node --test`) ; un `mocks.js` porte les valeurs d'aperçu éditables (hors layout, non persistées) ; `palette.js` et `inspector.js` sont les vues DOM (vérifiées navigateur) ; `canvas.js` est étendu pour consommer les mocks par composant, exposer la sélection programmatique et un onSelect enrichi. `app.js` câble palette ↔ canvas ↔ inspecteur.

**Tech Stack:** JavaScript (modules ES), `node --test` (zéro dépendance), HTML5 drag-and-drop pour palette→canvas, pointer events (déjà en place) pour le canvas. Servir via `python3 -m http.server` **depuis `Rich_Telemetry/`**.

---

## Périmètre : C1 sur 2 (découpage validé)

Le « Plan C » du spec a été scindé. **Ce plan = C1 (édition mono-page).** Reporté à **C2 (Pages & fichier)**, hors scope ici :
- Onglets de **pages** (CRUD + réordonner) et **canvas multi-pages** (C1 reste câblé sur `pages[0]`).
- **Bibliothèque de composants partagés inter-pages** (drag d'un composant existant sur une autre page) — sa valeur est cross-page, donc C2. En C1 la palette ne propose que les **6 créateurs de type**.
- **Export / import fichier** `layout.json`.
- **Humanisation des messages d'erreur ajv** (le panneau d'erreurs brut du Plan A reste tel quel en C1).

C1 produit un logiciel utilisable de bout en bout sur une page : créer/éditer/supprimer des widgets à la souris, le JSON avancé et le device I/O (Plan A) restent fonctionnels.

---

## Rappel de l'état actuel (post Plan B)

- `model.js` : `createModel()` → `{ state, subscribe, commit(mutator), undo, redo, canUndo, canRedo, toJSON, loadJSON }`. **`commit(mutator)` mute `state` en place** (les snapshots undo sont clonés). API volontairement minimale — C1 l'étend via `mutations.js` (fonctions pures appelées dans `commit`).
- `render.js` : builders `buildLabel/buildReadout/buildBar/buildRing/buildBadge` ; `buildReadout(comp, mock)`, `buildBar(comp, placement, mock)`, `buildRing(comp, placement, mock)` acceptent déjà un mock (défaut `MOCKS[type]`). `MOCKS = { readout:{value:42}, bar:{value:60}, ring:{value:72, reset_in_s:18000} }`. Aussi `ringPaths`, `pickThresholdColor`.
- `canvas.js` : `createCanvas({stage, badges}, model, {onSelect})` → `{ render, getSelected }`. Sélection par **index** de placement sur `pages[0]`. `onSelect` reçoit le placement (sera enrichi). Ring centré non déplaçable. Drag/resize commit-on-drop. Re-render sur `document.fonts.ready`.
- `geometry.js` : `ANCHORS`, `SCREEN`, `snapPlacement(x,y,w,h,snap)→{anchor,dx,dy,snapped}`, `placeAt`, etc.
- `index.html` : colonnes `#palette` (« Plan B/C »), `#canvas-col` (#stage/#badges), `#inspector` (« Plan C »).
- Schéma (source de vérité) : `../schema/layout.schema.json` — composants `label/readout/bar/ring/led_ring/sound`, placements `{ref, anchor, dx, dy, width, height, radius, thickness, gap_deg, start_angle}`. `additionalProperties:false` partout (le designer doit produire des clés valides).

---

## File Structure

```
designer/
├── index.html              # MODIF : remplir #palette et #inspector (retirer les placeholders)
├── style.css               # MODIF : styles palette + inspecteur
└── js/
    ├── mutations.js        # NOUVEAU : mutations pures du layout (PUR, testé)
    ├── mocks.js            # NOUVEAU : store des valeurs d'aperçu par composant (PUR, testé)
    ├── palette.js          # NOUVEAU : 6 créateurs de type, drag→canvas crée+place (vérif navigateur)
    ├── inspector.js        # NOUVEAU : form props/géométrie/seuils/mock par sélection (vérif navigateur)
    ├── canvas.js           # MODIF : mocks par composant, onSelect enrichi {placeIndex,ref}, selectPlacement()
    └── app.js              # MODIF : câble palette ↔ canvas ↔ inspecteur
└── tests/
    ├── mutations.test.js   # NOUVEAU
    └── mocks.test.js       # NOUVEAU
```

---

## Task 1 : `mutations.js` — mutations pures du layout (TDD)

**Files:**
- Create: `designer/js/mutations.js`
- Test: `designer/tests/mutations.test.js`

- [ ] **Step 1 : écrire les tests qui échouent** — `designer/tests/mutations.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  uniqueId, DEFAULTS, addComponent, addPlacement, removePlacement,
  setComponentProp, setPlacementProp, setThresholds
} from '../js/mutations.js';

const fresh = () => ({ components: {}, pages: [{ name: 'P1', place: [] }] });

test('uniqueId incrémente par type', () => {
  const s = fresh();
  assert.equal(uniqueId(s, 'label'), 'label1');
  s.components.label1 = { type: 'label' };
  assert.equal(uniqueId(s, 'label'), 'label2');
});

test('DEFAULTS produit une définition valide par type', () => {
  for (const type of ['label','readout','bar','ring','led_ring','sound']) {
    assert.equal(DEFAULTS[type]().type, type);
  }
});

test('addComponent ajoute à la map components', () => {
  const s = fresh();
  addComponent(s, 'x', { type: 'label', text: 'Hi' });
  assert.deepEqual(s.components.x, { type: 'label', text: 'Hi' });
});

test('addPlacement pousse sur la page', () => {
  const s = fresh();
  addPlacement(s, 0, { ref: 'x', anchor: 'CENTER' });
  assert.equal(s.pages[0].place.length, 1);
  assert.equal(s.pages[0].place[0].ref, 'x');
});

test('removePlacement retire par index', () => {
  const s = fresh();
  s.pages[0].place = [{ ref: 'a' }, { ref: 'b' }];
  removePlacement(s, 0, 0);
  assert.deepEqual(s.pages[0].place.map(p => p.ref), ['b']);
});

test('setComponentProp pose une valeur, vide la supprime', () => {
  const s = fresh();
  s.components.x = { type: 'label' };
  setComponentProp(s, 'x', 'text', 'Hi');
  assert.equal(s.components.x.text, 'Hi');
  setComponentProp(s, 'x', 'text', '');
  assert.equal('text' in s.components.x, false);
});

test('setPlacementProp pose une valeur, vide la supprime', () => {
  const s = fresh();
  s.pages[0].place = [{ ref: 'x', dx: 5 }];
  setPlacementProp(s, 0, 0, 'dy', 12);
  assert.equal(s.pages[0].place[0].dy, 12);
  setPlacementProp(s, 0, 0, 'dx', '');
  assert.equal('dx' in s.pages[0].place[0], false);
});

test('setThresholds pose un tableau non vide, vide le supprime', () => {
  const s = fresh();
  s.components.x = { type: 'ring' };
  setThresholds(s, 'x', [[20, '#FF0000']]);
  assert.deepEqual(s.components.x.thresholds, [[20, '#FF0000']]);
  setThresholds(s, 'x', []);
  assert.equal('thresholds' in s.components.x, false);
});
```

- [ ] **Step 2 : lancer, vérifier l'échec** — `node --test tests/mutations.test.js` → FAIL (`Cannot find module`).

- [ ] **Step 3 : implémenter `js/mutations.js`** :
```js
// Mutations dédiées du layout. Fonctions PURES : elles mutent l'état passé en place et sont
// appelées via model.commit(s => mutate(s, ...)). Séparées de model.js (state/undo/events) pour
// rester testables sous node --test. Toute clé posée doit rester valide vis-à-vis du schéma.

// Définition par défaut minimale et VALIDE pour un nouveau composant de chaque type.
export const DEFAULTS = {
  label:    () => ({ type: 'label', text: 'Texte', font: 20, color: '#FFFFFF' }),
  readout:  () => ({ type: 'readout', label: 'Label', font: 20, color: '#FFFFFF' }),
  bar:      () => ({ type: 'bar', label: 'Bar', min: 0, max: 100, color: '#38BDF8' }),
  ring:     () => ({ type: 'ring', color: '#38BDF8', pill: true, min: 0, max: 100 }),
  led_ring: () => ({ type: 'led_ring', color: '#FFFFFF', brightness: 64 }),
  sound:    () => ({ type: 'sound' })
};

// id unique pour un nouveau composant : <type><n>, n = 1er entier libre.
export function uniqueId(state, type) {
  const comps = state.components || {};
  let n = 1;
  while (comps[`${type}${n}`]) n++;
  return `${type}${n}`;
}

export function addComponent(state, id, def) {
  (state.components ||= {})[id] = def;
}

export function addPlacement(state, pageIndex, placement) {
  const page = state.pages[pageIndex];
  (page.place ||= []).push(placement);
}

export function removePlacement(state, pageIndex, placeIndex) {
  state.pages[pageIndex].place.splice(placeIndex, 1);
}

// Édite une prop de composant. Valeur vide (''/null/undefined) => suppression de la clé
// (le firmware retombe alors sur son défaut ; évite de produire des clés invalides).
export function setComponentProp(state, id, key, value) {
  const c = state.components[id];
  if (!c) return;
  if (value === '' || value === null || value === undefined) delete c[key];
  else c[key] = value;
}

export function setPlacementProp(state, pageIndex, placeIndex, key, value) {
  const p = state.pages[pageIndex].place[placeIndex];
  if (!p) return;
  if (value === '' || value === null || value === undefined) delete p[key];
  else p[key] = value;
}

// thresholds : tableau de [limite, "#hex"]. Vide => suppression de la clé.
export function setThresholds(state, id, thresholds) {
  const c = state.components[id];
  if (!c) return;
  if (thresholds && thresholds.length) c.thresholds = thresholds;
  else delete c.thresholds;
}
```

- [ ] **Step 4 : lancer, vérifier le succès** — `node --test tests/mutations.test.js` → PASS (8). Puis `node --test` → tout vert.

- [ ] **Step 5 : vérifier que les DEFAULTS valident le schéma** (pas juste `.type`). Run depuis `designer/` :
```bash
node --input-type=module -e "import {DEFAULTS} from './js/mutations.js'; import {createValidator} from './js/validate.js'; import {readFileSync} from 'node:fs'; const schema=JSON.parse(readFileSync(new URL('../schema/layout.schema.json',import.meta.url))); const v=createValidator(schema); for (const t of Object.keys(DEFAULTS)){ const layout={components:{x:DEFAULTS[t]()},pages:[{name:'P',place:[{ref:'x'}]}]}; const r=v(layout); console.log(t, r.valid, r.errors.join('|')); }"
```
Expected : chaque ligne `... true` (les 6 types valides). Si un type est `false`, corriger sa DEFAULTS avant de continuer.

- [ ] **Step 6 : Commit**
```bash
git add designer/js/mutations.js designer/tests/mutations.test.js
git commit -m "Rich_Telemetry: designer mutations module (layout ops) + tests"
```

---

## Task 2 : `mocks.js` — store des valeurs d'aperçu (TDD)

**Files:**
- Create: `designer/js/mocks.js`
- Test: `designer/tests/mocks.test.js`

> Les valeurs d'aperçu (mock) ne font PAS partie du `layout` (non persistées, non poussées au device). Elles vivent dans un store en mémoire, clé = id de composant, défauts par type repris de `render.js` `MOCKS`.

- [ ] **Step 1 : écrire les tests qui échouent** — `designer/tests/mocks.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { getMock, setMock } from '../js/mocks.js';

test('getMock initialise depuis les défauts du type', () => {
  assert.equal(getMock('cpu', 'readout').value, 42);
  assert.equal(getMock('jauge', 'ring').value, 72);
  assert.equal(getMock('jauge', 'ring').reset_in_s, 18000);
});

test('getMock renvoie un objet propre au type sans défaut', () => {
  assert.deepEqual(getMock('titre', 'label'), {});
});

test('setMock fusionne et persiste par id', () => {
  setMock('cpu', { value: 88 });
  assert.equal(getMock('cpu', 'readout').value, 88);
});

test('les ids sont indépendants', () => {
  setMock('a', { value: 1 });
  setMock('b', { value: 2 });
  assert.equal(getMock('a', 'bar').value, 1);
  assert.equal(getMock('b', 'bar').value, 2);
});
```

- [ ] **Step 2 : lancer, vérifier l'échec** — `node --test tests/mocks.test.js` → FAIL.

- [ ] **Step 3 : implémenter `js/mocks.js`** :
```js
// Valeurs d'aperçu éditables (mock). HORS layout : non persistées, non poussées au device ;
// remplacées à l'exécution réelle par POST /update. Clé = id de composant.
import { MOCKS } from './render.js';

const store = new Map();

// Renvoie le mock (mutable) d'un composant ; l'initialise depuis le défaut de son type au 1er accès.
export function getMock(id, type) {
  if (!store.has(id)) store.set(id, structuredClone(MOCKS[type] ?? {}));
  return store.get(id);
}

// Fusionne un patch dans le mock d'un composant.
export function setMock(id, patch) {
  const m = store.get(id) ?? {};
  Object.assign(m, patch);
  store.set(id, m);
}
```

- [ ] **Step 4 : lancer, vérifier le succès** — `node --test tests/mocks.test.js` → PASS (4). Puis `node --test` → tout vert.

- [ ] **Step 5 : Commit**
```bash
git add designer/js/mocks.js designer/tests/mocks.test.js
git commit -m "Rich_Telemetry: designer mocks store (preview values) + tests"
```

---

## Task 3 : `canvas.js` — mocks par composant, sélection enrichie + programmatique

**Files:**
- Modify: `designer/js/canvas.js`
- Modify: `designer/js/app.js`

> Vérification navigateur. Objectif : (a) le canvas affiche les valeurs mock **par composant** (plus les défauts globaux) ; (b) `onSelect` fournit `{placeIndex, ref}` (ce dont l'inspecteur a besoin) ; (c) on expose `selectPlacement(i)` pour que la palette puisse sélectionner le widget fraîchement créé.

- [ ] **Step 1 : remplacer les imports en tête de `js/canvas.js`**

Remplacer :
```js
import {
  buildLabel, buildReadout, buildBar, buildRing, buildBadge,
  ringPaths, pickThresholdColor, MOCKS
} from './render.js';
```
par :
```js
import {
  buildLabel, buildReadout, buildBar, buildRing, buildBadge,
  ringPaths, pickThresholdColor
} from './render.js';
import { getMock } from './mocks.js';
```

- [ ] **Step 2 : passer le mock par composant dans `buildNode`**

Remplacer la fonction `buildNode` :
```js
  function buildNode(pl, comp) {
    if (comp.type === 'bar')     return buildBar(comp, pl);
    if (comp.type === 'ring')    return buildRing(comp, pl);
    if (comp.type === 'readout') return buildReadout(comp);
    return buildLabel(comp); // label
  }
```
par :
```js
  function buildNode(pl, comp) {
    if (comp.type === 'bar')     return buildBar(comp, pl, getMock(pl.ref, 'bar'));
    if (comp.type === 'ring')    return buildRing(comp, pl, getMock(pl.ref, 'ring'));
    if (comp.type === 'readout') return buildReadout(comp, getMock(pl.ref, 'readout'));
    return buildLabel(comp); // label : pas de mock (affiche son texte)
  }
```

- [ ] **Step 3 : utiliser le mock du composant dans le live-resize du ring (`paintRing`)**

Dans `paintRing`, remplacer les deux usages de `MOCKS.ring.value`. La fonction actuelle commence par `function paintRing(node, comp, g) {` et contient :
```js
    const p = ringPaths(g.r, g.th, g.gap, MOCKS.ring.value, comp.min ?? 0, comp.max ?? 100);
    const col = pickThresholdColor(comp.thresholds, MOCKS.ring.value, comp.color || '#38BDF8');
```
Changer la signature en `function paintRing(node, comp, g, mockVal) {` et ces deux lignes en :
```js
    const p = ringPaths(g.r, g.th, g.gap, mockVal, comp.min ?? 0, comp.max ?? 100);
    const col = pickThresholdColor(comp.thresholds, mockVal, comp.color || '#38BDF8');
```
Puis, dans `addRingHandles`, l'appel `paintRing(node, comp, g);` (dans le handler `move`) devient :
```js
          paintRing(node, comp, g, getMock(pl.ref, 'ring').value);
```
(`pl` est le paramètre de `addRingHandles(node, i, comp, pl)` — déjà disponible.)

- [ ] **Step 4 : enrichir `onSelect` et exposer `selectPlacement`**

Remplacer la fonction `select` :
```js
  function select(i) {
    selected = i;
    applySelection();
    onSelect && onSelect(i == null ? null : placements()[i]);
  }
```
par :
```js
  function select(i) {
    selected = i;
    applySelection();
    onSelect && onSelect(i == null ? null : { placeIndex: i, ref: placements()[i].ref });
  }
```
Et remplacer le `return` final :
```js
  return { render, getSelected: () => selected };
```
par :
```js
  return { render, getSelected: () => selected, selectPlacement: select };
```

- [ ] **Step 5 : adapter `app.js` (l'onSelect change de forme ; reste no-op pour l'instant)**

Dans `js/app.js`, le bloc actuel :
```js
  // Canvas WYSIWYG (page 0). La sélection sera consommée par l'inspecteur en Plan C.
  createCanvas({ stage: $('stage'), badges: $('badges') }, model, {
    onSelect: () => {}
  });
```
devient (on capture la référence, l'inspecteur arrive en Task 5) :
```js
  // Canvas WYSIWYG (page 0). onSelect reçoit { placeIndex, ref } (consommé par l'inspecteur, Task 5).
  const canvas = createCanvas({ stage: $('stage'), badges: $('badges') }, model, {
    onSelect: () => {}
  });
```

- [ ] **Step 6 : vérification navigateur**

Servir depuis `Rich_Telemetry/` (`python3 -m http.server 8000`), ouvrir `http://localhost:8000/designer/`. Vérifier que le rendu démo est **inchangé** (anneau `72%`/`5h00`, `CPU 42 %`, barre RAM ~60 %, badges) et `✓ valide`, aucune erreur console (hors favicon 404). Le comportement est identique à Plan B (les mocks par défaut == les anciens MOCKS) — cette tâche est un refactor préparatoire.

- [ ] **Step 7 : `node --test` (toujours vert) puis Commit**
```bash
node --test
git add designer/js/canvas.js designer/js/app.js
git commit -m "Rich_Telemetry: designer canvas — per-component mocks + richer onSelect + selectPlacement"
```

---

## Task 4 : `palette.js` — créer un composant en glissant un type sur le canvas

**Files:**
- Create: `designer/js/palette.js`
- Modify: `designer/index.html` (colonne Palette)
- Modify: `designer/style.css` (styles palette)
- Modify: `designer/js/app.js` (instancier la palette)

> Vérification navigateur. Drag HTML5 : un item de palette est `draggable` ; le `#stage` accepte le drop et crée composant + placement au point de dépôt, puis sélectionne le nouveau widget.

- [ ] **Step 1 : remplir la colonne Palette dans `index.html`**

Remplacer :
```html
    <aside id="palette" class="col"><h2>Palette</h2><p class="todo">Plan B/C</p></aside>
```
par :
```html
    <aside id="palette" class="col"><h2>Palette</h2></aside>
```

- [ ] **Step 2 : créer `js/palette.js`** :
```js
// Palette : 6 créateurs de type. Glisser un item sur le #stage crée le composant + un placement
// au point de dépôt, en UN SEUL commit, puis sélectionne le nouveau widget. (Bibliothèque de
// composants partagés inter-pages = Plan C2.)
import { uniqueId, addComponent, addPlacement, DEFAULTS } from './mutations.js';
import { snapPlacement } from './geometry.js';

const TYPES = [
  ['label', 'Label'], ['readout', 'Lecture'], ['bar', 'Barre'],
  ['ring', 'Anneau'], ['led_ring', 'LED ring'], ['sound', 'Son']
];

// Placement initial selon le type. Ring centré (radius par défaut) ; led_ring/sound sans géométrie ;
// widgets écran : ancrage + offset déduits du point de dépôt (boîte de taille ~0, affinable au drag).
function makePlacement(type, id, x, y) {
  if (type === 'ring') return { ref: id, radius: 80, thickness: 16, gap_deg: 70 };
  if (type === 'led_ring' || type === 'sound') return { ref: id };
  const { anchor, dx, dy } = snapPlacement(x, y, 0, 0, 16);
  return { ref: id, anchor, dx, dy };
}

export function createPalette(root, model, { stage, onCreated } = {}) {
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

  stage.addEventListener('dragover', e => {
    if (e.dataTransfer.types.includes('text/rt-type')) e.preventDefault(); // autorise le drop
  });
  stage.addEventListener('drop', e => {
    const type = e.dataTransfer.getData('text/rt-type');
    if (!type) return;
    e.preventDefault();
    const r = stage.getBoundingClientRect();
    const x = e.clientX - r.left, y = e.clientY - r.top; // coords écran (1:1)
    let newIndex;
    model.commit(s => {
      const id = uniqueId(s, type);
      addComponent(s, id, DEFAULTS[type]());
      addPlacement(s, 0, makePlacement(type, id, x, y));
      newIndex = s.pages[0].place.length - 1;
    });
    onCreated && onCreated(newIndex);
  });
}
```

- [ ] **Step 3 : styles palette — ajouter à la fin de `style.css`** :
```css
/* --- Plan C1 : palette --- */
.palette-list { display: flex; flex-direction: column; gap: 6px; }
.palette-item {
  padding: 8px 10px; border: 1px solid var(--line); border-radius: 6px;
  background: #0f1a2b; cursor: grab; user-select: none; font-size: 13px;
}
.palette-item:active { cursor: grabbing; }
.palette-item:hover { border-color: var(--accent); }
```

- [ ] **Step 4 : instancier la palette dans `app.js`**

Ajouter l'import sous les imports existants :
```js
import { createPalette } from './palette.js';
```
Puis, juste après la création du `canvas` (le bloc `const canvas = createCanvas(...)` de Task 3) :
```js
  // Palette : glisser un type sur le canvas crée le composant, puis on le sélectionne.
  createPalette($('palette'), model, {
    stage: $('stage'),
    onCreated: i => canvas.selectPlacement(i)
  });
```

- [ ] **Step 5 : vérification navigateur**

Servir depuis `Rich_Telemetry/`, ouvrir le designer. Vérifier :
1. La colonne Palette liste les 6 types (Label, Lecture, Barre, Anneau, LED ring, Son).
2. Glisser **Barre** sur le canvas → une nouvelle barre apparaît près du point de dépôt ; le JSON avancé montre un nouveau composant `bar1` + un placement sur la page ; `✓ valide`.
3. Glisser **Anneau** → un nouvel anneau centré apparaît (`ring1`, `radius:80`).
4. Glisser **LED ring** / **Son** → un nouveau badge apparaît sous le canvas (pas de widget écran).
5. Le widget créé est **sélectionné** (liseré bleu / poignées si bar/ring) immédiatement après le drop.
6. Undo annule la création (composant + placement) en une fois.

Expected : tous les points OK, aucune erreur console.

- [ ] **Step 6 : Commit**
```bash
git add designer/js/palette.js designer/index.html designer/style.css designer/js/app.js
git commit -m "Rich_Telemetry: designer palette (drag type to canvas creates component)"
```

---

## Task 5 : `inspector.js` — propriétés de composant + suppression (cœur du panneau)

**Files:**
- Create: `designer/js/inspector.js`
- Modify: `designer/index.html` (colonne Inspecteur)
- Modify: `designer/style.css` (styles inspecteur)
- Modify: `designer/js/app.js` (câbler inspecteur ↔ canvas)

> Vérification navigateur. Cette tâche pose le squelette de l'inspecteur, les **helpers de champs réutilisables**, les **propriétés de composant par type** (pilotées par une table de descripteurs), le **signalement ASCII**, le bouton **supprimer de la page**, et l'abonnement au modèle. La **géométrie / seuils / mock** arrivent en Task 6.

- [ ] **Step 1 : vider la colonne Inspecteur dans `index.html`**

Remplacer :
```html
    <aside id="inspector" class="col"><h2>Inspecteur</h2><p class="todo">Plan C</p></aside>
```
par :
```html
    <aside id="inspector" class="col"><h2>Inspecteur</h2></aside>
```

- [ ] **Step 2 : créer `js/inspector.js`** (helpers + props de composant ; la géométrie/seuils/mock sont ajoutés en Task 6) :
```js
// Inspecteur : édite le composant + le placement sélectionnés. Pilote les champs par des tables de
// descripteurs (DRY). Chaque édition committée = UN commit (sur 'change', pas par frappe → pas de
// flood undo). Le signalement ASCII est live (sur 'input'). S'abonne au modèle pour se rafraîchir.
import { setComponentProp, removePlacement } from './mutations.js';
import { ANCHORS } from './geometry.js';

const FONTS = [14, 20, 28];
const nonAscii = v => /[^\x00-\x7F]/.test(v ?? '');

// Champs de composant par type : [clé, libellé, kind]. kind: asciitext|text|num|color|bool|font.
const COMP_FIELDS = {
  label:    [['text', 'Texte', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
  readout:  [['label', 'Label', 'asciitext'], ['unit', 'Unité', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
  bar:      [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color']],
  ring:     [['color', 'Couleur', 'color'], ['pill', 'Pastille %', 'bool'], ['countdown', 'Countdown', 'bool'], ['min', 'Min', 'num'], ['max', 'Max', 'num']],
  led_ring: [['color', 'Couleur', 'color'], ['brightness', 'Luminosité (0-255)', 'num']],
  sound:    []
};

// Construit un <input>/<select> selon kind. onChange reçoit la valeur typée. Les éditeurs textuels
// committent sur 'change' (pas 'input') pour ne pas inonder l'undo.
function makeInput(kind, value, onChange) {
  let el;
  if (kind === 'bool') {
    el = document.createElement('input'); el.type = 'checkbox'; el.checked = !!value;
    el.addEventListener('change', () => onChange(el.checked));
  } else if (kind === 'color') {
    el = document.createElement('input'); el.type = 'color'; el.value = value || '#FFFFFF';
    el.addEventListener('change', () => onChange(el.value.toUpperCase()));
  } else if (kind === 'font') {
    el = document.createElement('select');
    for (const f of FONTS) { const o = document.createElement('option'); o.value = String(f); o.textContent = f + ' px'; if (f === (value ?? 20)) o.selected = true; el.appendChild(o); }
    el.addEventListener('change', () => onChange(Number(el.value)));
  } else if (kind === 'anchor') {
    el = document.createElement('select');
    for (const a of ANCHORS) { const o = document.createElement('option'); o.value = a; o.textContent = a; if (a === (value || 'CENTER')) o.selected = true; el.appendChild(o); }
    el.addEventListener('change', () => onChange(el.value));
  } else if (kind === 'num') {
    el = document.createElement('input'); el.type = 'number'; el.value = value ?? '';
    el.addEventListener('change', () => onChange(el.value === '' ? '' : Number(el.value)));
  } else { // text / asciitext
    el = document.createElement('input'); el.type = 'text'; el.value = value ?? '';
    el.addEventListener('change', () => onChange(el.value));
  }
  return el;
}

// Ligne libellé + champ (+ avertissement ASCII live pour les champs asciitext).
function fieldRow(label, input, { ascii } = {}) {
  const row = document.createElement('label');
  row.className = 'insp-row';
  const span = document.createElement('span'); span.className = 'insp-label'; span.textContent = label;
  row.appendChild(span); row.appendChild(input);
  if (ascii) {
    const warn = document.createElement('span'); warn.className = 'insp-warn'; warn.textContent = '⚠ ASCII';
    warn.style.display = nonAscii(input.value) ? '' : 'none';
    input.addEventListener('input', () => { warn.style.display = nonAscii(input.value) ? '' : 'none'; });
    row.appendChild(warn);
  }
  return row;
}

export function createInspector(root, model, { rerenderCanvas, clearSelection } = {}) {
  let sel = null; // { placeIndex, ref } ou null

  const comp = () => sel && model.state.components[sel.ref];
  const place = () => sel && model.state.pages[0].place[sel.placeIndex];

  function select(s) { sel = s; render(); }

  // hook d'extension rempli en Task 6 (géométrie + seuils + mock). No-op ici.
  function renderExtras(body, c) {}

  function render() {
    // garde focus : ne pas reconstruire pendant qu'un champ de l'inspecteur est en cours d'édition.
    if (root.contains(document.activeElement) && document.activeElement !== document.body) return;
    root.querySelectorAll('.insp-body').forEach(n => n.remove());
    const c = comp();
    const body = document.createElement('div');
    body.className = 'insp-body';
    if (!c) {
      const p = document.createElement('p'); p.className = 'todo'; p.textContent = 'Sélectionne un widget sur le canvas.';
      body.appendChild(p); root.appendChild(body); return;
    }
    const head = document.createElement('div'); head.className = 'insp-head';
    head.textContent = `${c.type} · ${sel.ref}`;
    body.appendChild(head);

    for (const [key, label, kind] of COMP_FIELDS[c.type] || []) {
      const input = makeInput(kind, c[key], v => model.commit(s => setComponentProp(s, sel.ref, key, v)));
      body.appendChild(fieldRow(label, input, { ascii: kind === 'asciitext' }));
    }

    renderExtras(body, c); // Task 6

    const del = document.createElement('button'); del.className = 'insp-del'; del.textContent = 'Supprimer de la page';
    del.addEventListener('click', () => {
      const i = sel.placeIndex;
      model.commit(s => removePlacement(s, 0, i));
      sel = null;
      clearSelection && clearSelection(); // désélectionne le canvas
    });
    body.appendChild(del);
    root.appendChild(body);
  }

  model.subscribe(render);
  render();
  return { select };
}
```

- [ ] **Step 3 : styles inspecteur — ajouter à la fin de `style.css`** :
```css
/* --- Plan C1 : inspecteur --- */
.insp-head { font-size: 12px; color: var(--muted); text-transform: uppercase; margin-bottom: 8px; }
.insp-row { display: flex; align-items: center; gap: 6px; margin-bottom: 6px; font-size: 12.5px; }
.insp-label { flex: 0 0 78px; color: #9aa0aa; }
.insp-row input[type="text"], .insp-row input[type="number"], .insp-row select { flex: 1; min-width: 0; background: #0a1424; border: 1px solid var(--line); color: var(--ink); border-radius: 4px; padding: 2px 5px; }
.insp-row input[type="color"] { width: 36px; height: 22px; padding: 0; border: 1px solid var(--line); background: none; }
.insp-warn { color: var(--err); font-size: 11px; }
.insp-del { margin-top: 10px; width: 100%; border-color: var(--err); color: var(--err); }
.insp-sub { font-size: 11px; color: var(--muted); margin: 10px 0 4px; text-transform: uppercase; }
.insp-note { font-size: 11px; color: var(--muted); font-style: italic; margin: 2px 0 6px; }
```

- [ ] **Step 4 : câbler l'inspecteur dans `app.js`**

Ajouter l'import :
```js
import { createInspector } from './inspector.js';
```
Le canvas (Task 3) est créé avec `onSelect: () => {}`. Il faut maintenant que `onSelect` informe l'inspecteur, et que l'inspecteur puisse re-rendre/désélectionner le canvas. Comme les deux se référencent, déclarer `inspector` avant le canvas et utiliser une référence différée. Remplacer le bloc de création du canvas par :
```js
  let inspector;
  // Canvas WYSIWYG (page 0). onSelect → inspecteur.
  const canvas = createCanvas({ stage: $('stage'), badges: $('badges') }, model, {
    onSelect: s => inspector.select(s)
  });
  inspector = createInspector($('inspector'), model, {
    rerenderCanvas: canvas.render,
    clearSelection: () => canvas.selectPlacement(null)
  });
```
(La palette, instanciée juste après, reste inchangée — elle utilise `canvas.selectPlacement`.)

- [ ] **Step 5 : vérification navigateur**

Servir depuis `Rich_Telemetry/`, ouvrir le designer. Vérifier :
1. Au chargement, l'inspecteur affiche « Sélectionne un widget sur le canvas. »
2. Cliquer le titre (label) → l'inspecteur montre `label · titre` + champs Texte / Police / Couleur, pré-remplis.
3. Changer **Texte** en `Hello` (puis Tab/Entrée) → le canvas se met à jour ; le JSON avancé reflète `text: "Hello"` ; **une seule** entrée Undo.
4. Changer **Couleur** → le label change de couleur.
5. Cliquer la barre → champs Label/Min/Max/Couleur ; cliquer l'anneau → Couleur/Pastille (case)/Countdown/Min/Max.
6. Taper un accent (`é`) dans Texte → le marqueur `⚠ ASCII` apparaît (live), disparaît si on l'enlève.
7. **Supprimer de la page** sur un widget → il disparaît du canvas + du JSON ; l'inspecteur revient à l'état vide ; Undo le restaure.
8. Sélectionner un `led_ring`/`sound` (badge non cliquable sur le canvas) — note : les badges ne sont pas sélectionnables en C1 (pas de poignée/clic canvas). C'est attendu : leur édition (couleur/brightness) arrive avec une liste sélectionnable en C2. Ne pas le considérer comme un bug.

Expected : points 1-7 OK, aucune erreur console.

- [ ] **Step 6 : `node --test` (toujours vert) puis Commit**
```bash
node --test
git add designer/js/inspector.js designer/index.html designer/style.css designer/js/app.js
git commit -m "Rich_Telemetry: designer inspector — component props + ASCII + delete"
```

---

## Task 6 : Inspecteur — géométrie de placement, seuils du ring, valeur mock

**Files:**
- Modify: `designer/js/inspector.js`

> Vérification navigateur. On remplit le hook `renderExtras` (vide en Task 5) : géométrie du placement (par type), éditeur de `thresholds` du ring, éditeur de valeur mock (qui re-rend le canvas sans toucher au modèle/undo).

- [ ] **Step 1 : étendre les imports de `js/inspector.js`**

Remplacer :
```js
import { setComponentProp, removePlacement } from './mutations.js';
import { ANCHORS } from './geometry.js';
```
par :
```js
import { setComponentProp, setPlacementProp, setThresholds, removePlacement } from './mutations.js';
import { ANCHORS } from './geometry.js';
import { getMock, setMock } from './mocks.js';
```

- [ ] **Step 2 : ajouter les tables de descripteurs géométrie + mock**

Juste après la constante `COMP_FIELDS` (avant `makeInput`), ajouter :
```js
// Champs de géométrie de placement par type : [clé, libellé, kind].
const PLACE_FIELDS = {
  label:    [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
  readout:  [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
  bar:      [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num'], ['width', 'Largeur', 'num'], ['height', 'Hauteur', 'num']],
  ring:     [['radius', 'Rayon', 'num'], ['thickness', 'Épaisseur', 'num'], ['gap_deg', 'Ouverture°', 'num']],
  led_ring: [],
  sound:    []
};

// Champs de valeur mock (aperçu) par type : [clé, libellé].
const MOCK_FIELDS = {
  readout: [['value', 'Valeur (aperçu)']],
  bar:     [['value', 'Valeur (aperçu)']],
  ring:    [['value', 'Valeur % (aperçu)'], ['reset_in_s', 'Countdown (s)']],
  label:   [], led_ring: [], sound: []
};
```

- [ ] **Step 3 : remplacer le `renderExtras` no-op par l'implémentation complète**

Remplacer :
```js
  // hook d'extension rempli en Task 6 (géométrie + seuils + mock). No-op ici.
  function renderExtras(body, c) {}
```
par :
```js
  // Sous-titre de section.
  function sub(body, text) { const h = document.createElement('div'); h.className = 'insp-sub'; h.textContent = text; body.appendChild(h); }
  function note(body, text) { const n = document.createElement('div'); n.className = 'insp-note'; n.textContent = text; body.appendChild(n); }

  function renderExtras(body, c) {
    const p = place();
    // --- Géométrie du placement ---
    const gf = PLACE_FIELDS[c.type] || [];
    if (gf.length) {
      sub(body, 'Placement');
      if (c.type === 'ring') note(body, 'Anneau centré : ancrage/dx/dy ignorés par le firmware.');
      for (const [key, label, kind] of gf) {
        const input = makeInput(kind, p[key], v => model.commit(s => setPlacementProp(s, 0, sel.placeIndex, key, v)));
        body.appendChild(fieldRow(label, input));
      }
    }

    // --- Seuils du ring (liste éditable de [limite, #couleur]) ---
    if (c.type === 'ring') {
      sub(body, 'Seuils (couleur si valeur < limite)');
      const ths = (c.thresholds || []).map(t => [t[0], t[1]]); // copie locale éditable
      const commitThs = () => model.commit(s => setThresholds(s, sel.ref, ths.filter(t => t[1])));
      ths.forEach((t, idx) => {
        const row = document.createElement('div'); row.className = 'insp-row';
        const lim = makeInput('num', t[0], v => { ths[idx][0] = v === '' ? 0 : v; commitThs(); });
        const col = makeInput('color', t[1], v => { ths[idx][1] = v; commitThs(); });
        const rm = document.createElement('button'); rm.className = 'insp-th-rm'; rm.textContent = '×';
        rm.addEventListener('click', () => { ths.splice(idx, 1); commitThs(); });
        row.appendChild(lim); row.appendChild(col); row.appendChild(rm);
        body.appendChild(row);
      });
      const add = document.createElement('button'); add.className = 'insp-th-add'; add.textContent = '+ seuil';
      add.addEventListener('click', () => { ths.push([0, '#FF0000']); commitThs(); });
      body.appendChild(add);
    }

    // --- Valeur d'aperçu (mock) : hors layout, re-rend le canvas sans toucher au modèle/undo ---
    const mf = MOCK_FIELDS[c.type] || [];
    if (mf.length) {
      sub(body, 'Aperçu (mock, non poussé au device)');
      const m = getMock(sel.ref, c.type);
      for (const [key, label] of mf) {
        const input = makeInput('num', m[key], v => {
          setMock(sel.ref, { [key]: v === '' ? 0 : v });
          rerenderCanvas && rerenderCanvas();
        });
        body.appendChild(fieldRow(label, input));
      }
    }
  }
```

- [ ] **Step 4 : styles des boutons de seuils — ajouter à la fin de `style.css`**
```css
.insp-th-rm { flex: 0 0 auto; padding: 0 8px; border-color: var(--line); color: var(--muted); }
.insp-th-add { width: 100%; margin-top: 2px; }
```

- [ ] **Step 5 : vérification navigateur**

Servir depuis `Rich_Telemetry/`, ouvrir le designer. Vérifier :
1. Sélectionner le titre → section **Placement** avec Ancrage (select 9 valeurs) / dx / dy. Changer Ancrage en `TOP_LEFT` → le label saute au coin (liseré `.outside` rouge) ; JSON reflète l'ancrage.
2. Sélectionner la barre → Placement avec Largeur/Hauteur ; les modifier → la barre se redimensionne ; JSON reflète width/height.
3. Sélectionner l'anneau → note « centré… » ; Rayon/Épaisseur/Ouverture° éditables → l'anneau change ; section **Seuils** : `+ seuil` ajoute une ligne (limite + couleur + ×) ; ajouter `[80, #F87171]`, mettre la valeur mock à `50` → l'arc passe au rouge (50 < 80) ; `×` retire le seuil.
4. **Aperçu (mock)** : pour la barre, changer Valeur → le remplissage bouge **sans** créer d'entrée Undo (mock hors modèle) ; pour l'anneau, Valeur % et Countdown(s) modifient pastille/arc/légende ; Undo n'est pas affecté par les changements de mock.
5. Tout édité via l'inspecteur reste **`✓ valide`**.

Expected : tous OK, aucune erreur console.

- [ ] **Step 6 : suite complète + Commit**
```bash
node --test
git add designer/js/inspector.js designer/style.css
git commit -m "Rich_Telemetry: designer inspector — placement geometry + ring thresholds + mock value"
```

---

## Self-Review (effectuée à la rédaction)

**1. Couverture (périmètre C1) :**
- Mutations dédiées `model.js` en TDD → Task 1 (`mutations.js`) ✓ (le spec dit « ajouter à model.js » ; on les met dans un module pur séparé appelé via `commit` — même intention, testable).
- Palette : 6 types, **drag→canvas crée un composant** → Task 4 ✓ (bibliothèque inter-pages = C2, noté).
- Inspecteur : props par type → Task 5 ; géométrie + éditeur `thresholds` + valeur mock + **signalement ASCII** → Task 5/6 ✓.
- Valeurs mock **éditables** → Task 2 (`mocks.js`) + Task 6 ✓.
- Reporté C2 (explicitement) : pages CRUD, canvas multi-pages, file-io, humanisation ajv, bibliothèque partagée. Champs réservés (`center_pct`, `start_angle`) : non exposés par l'inspecteur (conforme au spec).

**2. Placeholders :** aucun. `renderExtras` est un no-op **intentionnel et nommé** en Task 5, remplacé par du code complet en Task 6 (incrément TDD-friendly, pas un placeholder de plan). Tout le code exécutable est fourni.

**3. Cohérence des types/symboles :**
- `mutations.js` exporte `uniqueId/DEFAULTS/addComponent/addPlacement/removePlacement/setComponentProp/setPlacementProp/setThresholds` — signatures identiques entre Task 1 (impl+tests), `palette.js` (Task 4) et `inspector.js` (Task 5/6).
- `mocks.js` : `getMock(id,type)/setMock(id,patch)` — mêmes signatures entre Task 2, `canvas.js` (Task 3) et `inspector.js` (Task 6).
- `canvas.js` : `onSelect({placeIndex, ref})` et `selectPlacement(i)` (Task 3) consommés par `inspector.select` et `palette.onCreated` / `clearSelection` (Task 4/5).
- `createInspector(root, model, {rerenderCanvas, clearSelection}) → {select}` (Task 5) câblé tel quel dans `app.js` ; `rerenderCanvas = canvas.render` utilisé en Task 6.
- `makeInput`/`fieldRow` définis en Task 5, réutilisés en Task 6 (mêmes signatures).

**4. Conventions :** TDD `node --test` pour le pur (mutations, mocks) ; vérif navigateur pour le DOM (palette, inspecteur) — calque A/B. Modules ES zéro-build. Commits fréquents. `commit` sur `change` (pas par frappe) pour ne pas inonder l'undo (même esprit que le commit-on-drop du Plan B).

---

## Décisions prises (à connaître)

- **Mutations dans un module pur `mutations.js`** (pas des méthodes sur l'objet model) : maximise la testabilité `node --test` et garde `model.js` (state/undo/events) inchangé. Appelées via `model.commit(s => mutate(s, …))`.
- **Édition committée sur `change`** (blur/Entrée), pas sur chaque frappe : un seul commit par édition (sinon flood undo). Le signalement ASCII, lui, est live (`input`).
- **Mocks hors modèle** : éditer une valeur d'aperçu re-rend le canvas mais **ne crée pas d'entrée Undo** et n'altère pas le `layout.json` (cohérent : c'est de l'aperçu, le device reçoit ses valeurs via `/update`).
- **Badges led_ring/sound non sélectionnables en C1** (pas de cible de clic canvas) : leur édition viendra avec une liste sélectionnable en C2. Leurs propriétés restent éditables via le JSON avancé entre-temps.
- **C1 câblé sur `pages[0]`** : assumé (mono-page). C2 propagera l'index de page active (déjà noté dans le HANDOFF).

---

## Execution Handoff

Plan complet et sauvegardé dans `designer/plans/2026-06-16-wysiwyg-editor-plan-c1-panels.md`. Deux options :

1. **Subagent-Driven (recommandé)** — un subagent/tâche, revue spec puis qualité, vérif navigateur du DOM par le contrôleur (Playwright). (Process suivi pour A et B.)
2. **Inline** — exécution dans cette session, par lots avec checkpoints.

Après C1 : **Plan C2 — Pages & fichier** (onglets pages CRUD/réordonner + canvas multi-pages + file-io export/import + humanisation erreurs ajv + bibliothèque de composants partagés inter-pages).
