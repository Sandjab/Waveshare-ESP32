# Rich_Telemetry Designer — Plan A : Fondation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refondre le squelette du designer en modules ES testables, avec la math de positionnement, le modèle d'état (undo/redo) et la validation couverts par `node --test`, et un éditeur JSON avancé câblé + device load/push préservé.

**Architecture:** Web app vanilla, modules ES chargés via `<script type="module">`, zéro bundler. La logique pure (`geometry.js`, `model.js`, `validate.js`) ne dépend pas du DOM → testable sous Node. Le câblage DOM (`app.js`, `json-view.js`, `device.js`) se vérifie dans le navigateur. ajv est vendorisé en un fichier ESM importable côté Node *et* navigateur.

**Tech Stack:** JavaScript (modules ES), `node --test` (runner intégré, zéro dépendance), ajv 8 vendorisé, `python3 -m http.server` pour servir.

---

## File Structure

```
designer/
├── package.json            # {"type":"module"} — fait que Node traite les .js comme ESM. AUCUNE dépendance.
├── index.html              # coquille 3 colonnes + barre device + JSON avancé + panneau erreurs
├── style.css
├── vendor/
│   └── ajv.min.js          # ajv 8 bundlé ESM (unique artefact vendorisé)
├── js/
│   ├── default-layout.js   # DEFAULT_LAYOUT (layout de départ valide, pur)
│   ├── geometry.js         # ancrage + offset, snap, inverse (PUR, testé)
│   ├── model.js            # état + mutations + undo/redo + events (PUR, testé)
│   ├── validate.js         # createValidator(schema) → validate() : forme ajv + refs (PUR, testé)
│   ├── device.js           # load/push REST (extrait de l'ancien app.js)
│   ├── json-view.js        # sync textarea ↔ model
│   └── app.js              # bootstrap : câble model ↔ vues ↔ device ↔ undo
└── tests/
    ├── geometry.test.js
    ├── model.test.js
    └── validate.test.js
```

> Note : l'ancien `app.js` monolithique est remplacé. `index.html`, `style.css` sont réécrits. Le `README.md` et le dossier `schema/` ne sont pas touchés par ce plan.

> `package.json` n'introduit **aucune dépendance** ni step de build : c'est uniquement le drapeau `"type":"module"` pour que `node --test` exécute les `.js` en ESM. Cohérent avec « zéro-build ».

---

## Task 1 : Scaffold (coquille + défaut + ajv vendorisé)

**Files:**
- Create: `designer/package.json`
- Create: `designer/js/default-layout.js`
- Create: `designer/vendor/ajv.min.js` (téléchargé)
- Modify (réécriture): `designer/index.html`
- Modify (réécriture): `designer/style.css`

- [ ] **Step 1 : `package.json`**

```json
{
  "name": "rich-telemetry-designer",
  "version": "0.0.0",
  "private": true,
  "type": "module"
}
```

- [ ] **Step 2 : layout de départ — `js/default-layout.js`**

```js
// Layout de départ de l'éditeur (vide-utile). Valide vis-à-vis de layout.schema.json.
// Indépendant du layout par défaut du firmware.
export const DEFAULT_LAYOUT = {
  title: "Dashboard",
  background: "#000000",
  components: {
    hello: { type: "label", text: "Hello", font: 20, color: "#FFFFFF" }
  },
  pages: [
    { name: "Page 1", place: [ { ref: "hello", anchor: "CENTER" } ] }
  ]
};
```

- [ ] **Step 3 : vendoriser ajv (un fichier ESM bundlé)**

Run (depuis `designer/`) :
```bash
mkdir -p vendor && curl -L "https://esm.sh/ajv@8?bundle&target=es2022" -o vendor/ajv.min.js
```
Expected : `vendor/ajv.min.js` créé (>100 KB). Vérifier que l'export par défaut est la classe `Ajv` :
```bash
node -e "import('./vendor/ajv.min.js').then(m => console.log(typeof m.default))"
```
Expected : `function`

- [ ] **Step 4 : `index.html` (coquille 3 colonnes)**

```html
<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Rich_Telemetry — Designer</title>
  <link rel="stylesheet" href="style.css" />
</head>
<body>
  <header>
    <strong>Rich_Telemetry Designer</strong>
    <span class="grow"></span>
    <button id="undo" disabled>↶ Undo</button>
    <button id="redo" disabled>↷ Redo</button>
    <label>Device <input id="base" type="text" placeholder="http://192.168.1.35" size="20" /></label>
    <button id="load">Charger</button>
    <button id="push">Pousser</button>
    <span id="status" class="status"></span>
  </header>

  <main>
    <aside id="palette" class="col"><h2>Palette</h2><p class="todo">Plan B/C</p></aside>
    <section id="canvas-col" class="col"><h2>Canvas</h2><p class="todo">Plan B</p></section>
    <aside id="inspector" class="col"><h2>Inspecteur</h2><p class="todo">Plan C</p></aside>
  </main>

  <footer>
    <details open>
      <summary>JSON avancé</summary>
      <textarea id="json" spellcheck="false"></textarea>
      <div class="row">
        <button id="apply">Appliquer le JSON</button>
        <span id="valid" class="valid"></span>
      </div>
      <pre id="errors" class="errors"></pre>
    </details>
  </footer>

  <script type="module" src="js/app.js"></script>
</body>
</html>
```

- [ ] **Step 5 : `style.css`**

```css
* { box-sizing: border-box; }
body { margin: 0; font: 14px/1.5 system-ui, sans-serif; background: #0b1220; color: #e6edf3; }
header { display: flex; align-items: center; gap: 8px; padding: 8px 12px; border-bottom: 1px solid #1e293b; }
header .grow { flex: 1; }
button { background: #0f2233; border: 1px solid #38bdf8; color: #e6edf3; border-radius: 5px; padding: 4px 10px; cursor: pointer; }
button:disabled { border-color: #1e293b; color: #475569; cursor: default; }
input { background: #0a1424; border: 1px solid #1e293b; color: #e6edf3; border-radius: 5px; padding: 3px 6px; }
main { display: grid; grid-template-columns: 170px 1fr 200px; gap: 10px; padding: 10px; min-height: 50vh; }
.col { background: #0d1117; border: 1px solid #1e293b; border-radius: 8px; padding: 10px; }
.col h2 { font-size: 12px; text-transform: uppercase; color: #64748b; margin: 0 0 8px; }
.todo { color: #475569; font-style: italic; }
footer { padding: 10px 12px; border-top: 1px solid #1e293b; }
textarea { width: 100%; height: 220px; background: #070d17; color: #cbd5e1; border: 1px solid #1e293b; border-radius: 8px; font: 12.5px/1.5 monospace; padding: 10px; }
.row { display: flex; align-items: center; gap: 10px; margin-top: 6px; }
.status, .valid { font-size: 12.5px; }
.status.ok, .valid.ok { color: #22c55e; } .status.err, .valid.err { color: #f87171; }
.errors { color: #f87171; font: 12px/1.4 monospace; white-space: pre-wrap; margin: 6px 0 0; }
```

- [ ] **Step 6 : vérification manuelle**

Run (depuis `designer/`) :
```bash
python3 -m http.server 8000
```
Ouvrir `http://localhost:8000`. Expected : 3 colonnes (Palette / Canvas / Inspecteur avec « Plan B/C »), barre device, section « JSON avancé » avec textarea vide. Aucune erreur console (sauf `app.js` 404 — créé en Task 5 ; acceptable à ce stade).

- [ ] **Step 7 : Commit**

```bash
git add designer/package.json designer/index.html designer/style.css designer/js/default-layout.js designer/vendor/ajv.min.js
git commit -m "Rich_Telemetry: designer scaffold 3-col shell + default layout + vendored ajv"
```

---

## Task 2 : `geometry.js` — math de positionnement (TDD)

**Files:**
- Create: `designer/js/geometry.js`
- Test: `designer/tests/geometry.test.js`

- [ ] **Step 1 : écrire le test qui échoue**

`designer/tests/geometry.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { offsetFor, nearestAnchor, snapPlacement, placeAt } from '../js/geometry.js';

const W = 120, H = 34;

test('widget centré → CENTER offset (0,0)', () => {
  // centre du widget (x+w/2, y+h/2) au centre écran (180,180) : x=120, y=163
  assert.deepEqual(offsetFor('CENTER', 120, 163, W, H), [0, 0]);
});

test('widget collé haut-centre → TOP_MID offset (0,0)', () => {
  assert.deepEqual(offsetFor('TOP_MID', 120, 0, W, H), [0, 0]);
});

test('nearestAnchor près du haut → TOP_MID', () => {
  assert.equal(nearestAnchor(120, 5, W, H), 'TOP_MID');
});

test('snap quand proche d’un ancrage → dx=dy=0', () => {
  const r = snapPlacement(120, 3, W, H, 16);
  assert.equal(r.anchor, 'TOP_MID');
  assert.equal(r.dx, 0); assert.equal(r.dy, 0); assert.equal(r.snapped, true);
});

test('pas de snap quand loin → offset conservé', () => {
  const r = snapPlacement(120, 60, W, H, 16);
  assert.equal(r.snapped, false);
  assert.ok(r.dy > 0);
});

test('placeAt est l’inverse de offsetFor (round-trip)', () => {
  const [dx, dy] = offsetFor('TOP_MID', 100, 50, W, H);
  const { x, y } = placeAt('TOP_MID', dx, dy, W, H);
  assert.equal(Math.round(x), 100);
  assert.equal(Math.round(y), 50);
});
```

- [ ] **Step 2 : lancer le test, vérifier l'échec**

Run (depuis `designer/`) : `node --test tests/geometry.test.js`
Expected : FAIL — `Cannot find module '../js/geometry.js'`.

- [ ] **Step 3 : implémenter `js/geometry.js`**

```js
// Modèle de positionnement : ancrage LVGL + offset (dx, dy). Pur, sans DOM.
// parent = carré 360×360 ; le widget s'aligne par le même "point d'ancrage" que le parent.
export const ANCHORS = ['CENTER','TOP_MID','BOTTOM_MID','LEFT_MID','RIGHT_MID','TOP_LEFT','TOP_RIGHT','BOTTOM_LEFT','BOTTOM_RIGHT'];
export const SCREEN = 360;

const P = {
  CENTER:[180,180], TOP_MID:[180,0], BOTTOM_MID:[180,360], LEFT_MID:[0,180], RIGHT_MID:[360,180],
  TOP_LEFT:[0,0], TOP_RIGHT:[360,0], BOTTOM_LEFT:[0,360], BOTTOM_RIGHT:[360,360]
};

export function parentPoint(anchor) { return P[anchor]; }

export function widgetPoint(anchor, x, y, w, h) {
  const px = anchor.includes('LEFT') ? x : anchor.includes('RIGHT') ? x + w : x + w / 2;
  const py = anchor.startsWith('TOP') ? y : anchor.startsWith('BOTTOM') ? y + h : y + h / 2;
  return [px, py];
}

export function offsetFor(anchor, x, y, w, h) {
  const [wx, wy] = widgetPoint(anchor, x, y, w, h);
  return [Math.round(wx - P[anchor][0]), Math.round(wy - P[anchor][1])];
}

export function nearestAnchor(x, y, w, h) {
  let best = null, bd = Infinity;
  for (const a of ANCHORS) {
    const [dx, dy] = offsetFor(a, x, y, w, h);
    const d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = a; }
  }
  return best;
}

export function snapPlacement(x, y, w, h, snap = 16) {
  const anchor = nearestAnchor(x, y, w, h);
  let [dx, dy] = offsetFor(anchor, x, y, w, h);
  const snapped = Math.hypot(dx, dy) < snap;
  if (snapped) { dx = 0; dy = 0; }
  return { anchor, dx, dy, snapped };
}

export function placeAt(anchor, dx, dy, w, h) {
  const px = P[anchor][0] + dx, py = P[anchor][1] + dy;
  const x = px - (anchor.includes('LEFT') ? 0 : anchor.includes('RIGHT') ? w : w / 2);
  const y = py - (anchor.startsWith('TOP') ? 0 : anchor.startsWith('BOTTOM') ? h : h / 2);
  return { x, y };
}
```

- [ ] **Step 4 : lancer le test, vérifier le succès**

Run : `node --test tests/geometry.test.js`
Expected : PASS (6 tests).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/geometry.js designer/tests/geometry.test.js
git commit -m "Rich_Telemetry: designer geometry module (anchor+offset, snap) + tests"
```

---

## Task 3 : `model.js` — état, mutations, undo/redo (TDD)

**Files:**
- Create: `designer/js/model.js`
- Test: `designer/tests/model.test.js`

- [ ] **Step 1 : écrire le test qui échoue**

`designer/tests/model.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createModel } from '../js/model.js';

test('commit applique une mutation et la rend visible', () => {
  const m = createModel();
  m.commit(s => { s.title = 'X'; });
  assert.equal(m.state.title, 'X');
});

test('undo restaure l’état précédent', () => {
  const m = createModel();
  const before = m.state.title;
  m.commit(s => { s.title = 'X'; });
  m.undo();
  assert.equal(m.state.title, before);
});

test('redo réapplique', () => {
  const m = createModel();
  m.commit(s => { s.title = 'X'; });
  m.undo(); m.redo();
  assert.equal(m.state.title, 'X');
});

test('une nouvelle mutation vide la pile redo', () => {
  const m = createModel();
  m.commit(s => { s.title = 'A'; });
  m.undo();
  m.commit(s => { s.title = 'B'; });
  assert.equal(m.canRedo(), false);
});

test('subscribe est notifié à chaque changement', () => {
  const m = createModel();
  let n = 0; m.subscribe(() => n++);
  m.commit(s => { s.title = 'X'; });
  m.undo();
  assert.equal(n, 2);
});

test('toJSON / loadJSON round-trip', () => {
  const m = createModel();
  const json = m.toJSON();
  m.commit(s => { s.title = 'changed'; });
  m.loadJSON(json);
  assert.equal(m.state.title, 'Dashboard');
});

test('les snapshots sont clonés (pas de fuite par référence)', () => {
  const m = createModel();
  m.commit(s => { s.title = 'A'; });
  m.commit(s => { s.title = 'B'; });
  m.undo();
  assert.equal(m.state.title, 'A');
});
```

- [ ] **Step 2 : lancer le test, vérifier l'échec**

Run : `node --test tests/model.test.js`
Expected : FAIL — `Cannot find module '../js/model.js'`.

- [ ] **Step 3 : implémenter `js/model.js`**

```js
// Source de vérité du layout en mémoire. Pur (pas de DOM). Pile undo/redo + events.
import { DEFAULT_LAYOUT } from './default-layout.js';

export function createModel(initial) {
  let state = structuredClone(initial ?? DEFAULT_LAYOUT);
  const undoStack = [], redoStack = [], subs = new Set();
  const emit = () => subs.forEach(fn => fn(state));
  const snapshot = () => {
    undoStack.push(structuredClone(state));
    if (undoStack.length > 100) undoStack.shift();
    redoStack.length = 0;
  };
  return {
    get state() { return state; },
    subscribe(fn) { subs.add(fn); return () => subs.delete(fn); },
    commit(mutator) { snapshot(); mutator(state); emit(); },
    canUndo() { return undoStack.length > 0; },
    canRedo() { return redoStack.length > 0; },
    undo() { if (!undoStack.length) return; redoStack.push(structuredClone(state)); state = undoStack.pop(); emit(); },
    redo() { if (!redoStack.length) return; undoStack.push(structuredClone(state)); state = redoStack.pop(); emit(); },
    toJSON() { return JSON.stringify(state, null, 2); },
    loadJSON(text) { const next = JSON.parse(text); snapshot(); state = next; emit(); }
  };
}
```

- [ ] **Step 4 : lancer le test, vérifier le succès**

Run : `node --test tests/model.test.js`
Expected : PASS (7 tests).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/model.js designer/tests/model.test.js
git commit -m "Rich_Telemetry: designer model module (state, mutations, undo/redo) + tests"
```

---

## Task 4 : `validate.js` — forme ajv + refs sémantiques (TDD)

**Files:**
- Create: `designer/js/validate.js`
- Test: `designer/tests/validate.test.js`

- [ ] **Step 1 : écrire le test qui échoue**

`designer/tests/validate.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createValidator } from '../js/validate.js';
import { DEFAULT_LAYOUT } from '../js/default-layout.js';

const schema = JSON.parse(readFileSync(new URL('../../schema/layout.schema.json', import.meta.url)));
const validate = createValidator(schema);

test('layout par défaut est valide', () => {
  assert.equal(validate(DEFAULT_LAYOUT).valid, true);
});

test('type de composant inconnu → invalide', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.components.hello.type = 'wat';
  assert.equal(validate(bad).valid, false);
});

test('couleur hex invalide → invalide', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.background = 'red';
  assert.equal(validate(bad).valid, false);
});

test('ref de placement non résolue → invalide (sémantique, hors JSON Schema)', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.pages[0].place[0].ref = 'ghost';
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes("ref inconnue 'ghost'")));
});
```

- [ ] **Step 2 : lancer le test, vérifier l'échec**

Run : `node --test tests/validate.test.js`
Expected : FAIL — `Cannot find module '../js/validate.js'`.

- [ ] **Step 3 : implémenter `js/validate.js`**

```js
// Validation du layout : forme (ajv contre le schema) + invariants sémantiques (refs).
// Le schema définit le FORMAT ; la résolution des placement.ref est une contrainte
// sémantique non exprimable en JSON Schema, ajoutée ici (miroir du firmware).
import Ajv from '../vendor/ajv.min.js';

export function createValidator(schema) {
  const ajv = new Ajv({ allErrors: true, strict: false });
  const validateShape = ajv.compile(schema);
  return function validate(layout) {
    const errors = [];
    if (!validateShape(layout)) {
      for (const e of validateShape.errors) {
        errors.push(`${e.instancePath || '/'} ${e.message}`);
      }
    }
    const ids = new Set(Object.keys(layout?.components || {}));
    (layout?.pages || []).forEach((p, pi) => {
      (p?.place || []).forEach(pl => {
        if (pl && !ids.has(pl.ref)) errors.push(`pages/${pi}: ref inconnue '${pl.ref}'`);
      });
    });
    return { valid: errors.length === 0, errors };
  };
}
```

- [ ] **Step 4 : lancer le test, vérifier le succès**

Run : `node --test tests/validate.test.js`
Expected : PASS (4 tests). Si ajv émet une erreur de meta-schema draft-07, vérifier que `strict: false` est bien passé.

- [ ] **Step 5 : lancer toute la suite**

Run (depuis `designer/`) : `node --test`
Expected : PASS (17 tests au total : 6 + 7 + 4).

- [ ] **Step 6 : Commit**

```bash
git add designer/js/validate.js designer/tests/validate.test.js
git commit -m "Rich_Telemetry: designer validate module (ajv shape + ref semantics) + tests"
```

---

## Task 5 : Câblage navigateur — device, JSON view, bootstrap

**Files:**
- Create: `designer/js/device.js`
- Create: `designer/js/json-view.js`
- Create: `designer/js/app.js`

> Vérification manuelle (le DOM/réseau n'est pas couvert par `node --test`).

- [ ] **Step 1 : `js/device.js` (load/push REST, extrait de l'ancien squelette)**

```js
// Pont REST avec le device. CORS résolu côté firmware (header + OPTIONS).
function clean(base) { return base.replace(/\/+$/, ''); }

export async function loadLayout(base) {
  const r = await fetch(clean(base) + '/layout');
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
}

export async function pushLayout(base, layoutText) {
  const r = await fetch(clean(base) + '/layout', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: layoutText
  });
  const body = await r.json().catch(() => ({}));
  if (!r.ok || body.ok === false) throw new Error(body.error || 'HTTP ' + r.status);
  return body;
}
```

- [ ] **Step 2 : `js/json-view.js` (sync textarea ↔ model)**

```js
// Vue JSON avancée : reflète le modèle dans le textarea, et applique le texte au modèle.
export function bindJsonView(model, { textarea, applyBtn, validEl, errorsEl }, validate) {
  // modèle → textarea
  const refresh = () => {
    if (document.activeElement !== textarea) textarea.value = model.toJSON();
    runValidation();
  };
  // validation de l'état courant du modèle
  const runValidation = () => {
    const { valid, errors } = validate(model.state);
    validEl.textContent = valid ? '✓ valide' : '✗ invalide';
    validEl.className = 'valid ' + (valid ? 'ok' : 'err');
    errorsEl.textContent = errors.join('\n');
  };
  // textarea → modèle (au clic Appliquer)
  applyBtn.onclick = () => {
    try {
      model.loadJSON(textarea.value);
      validEl.textContent = 'JSON appliqué';
      validEl.className = 'valid ok';
    } catch (e) {
      validEl.textContent = 'JSON illisible : ' + e.message;
      validEl.className = 'valid err';
    }
  };
  model.subscribe(refresh);
  refresh();
}
```

- [ ] **Step 3 : `js/app.js` (bootstrap + câblage)**

```js
import { createModel } from './model.js';
import { createValidator } from './validate.js';
import { bindJsonView } from './json-view.js';
import { loadLayout, pushLayout } from './device.js';

const $ = id => document.getElementById(id);

async function main() {
  const schema = await (await fetch('../schema/layout.schema.json')).json();
  const validate = createValidator(schema);
  const model = createModel();

  bindJsonView(model, {
    textarea: $('json'), applyBtn: $('apply'), validEl: $('valid'), errorsEl: $('errors')
  }, validate);

  // undo / redo
  const syncUndo = () => { $('undo').disabled = !model.canUndo(); $('redo').disabled = !model.canRedo(); };
  model.subscribe(syncUndo); syncUndo();
  $('undo').onclick = () => model.undo();
  $('redo').onclick = () => model.redo();

  // device
  const setStatus = (msg, kind) => { $('status').textContent = msg; $('status').className = 'status ' + (kind || ''); };
  $('load').onclick = async () => {
    if (!$('base').value) return setStatus('URL device ?', 'err');
    setStatus('Chargement…');
    try { model.loadJSON(JSON.stringify(await loadLayout($('base').value))); setStatus('Chargé', 'ok'); }
    catch (e) { setStatus('Échec : ' + e.message + ' (CORS ? cf. README)', 'err'); }
  };
  $('push').onclick = async () => {
    if (!$('base').value) return setStatus('URL device ?', 'err');
    if (!validate(model.state).valid) return setStatus('Layout invalide', 'err');
    setStatus('Envoi…');
    try { await pushLayout($('base').value, model.toJSON()); setStatus('Poussé et persisté', 'ok'); }
    catch (e) { setStatus('Échec : ' + e.message, 'err'); }
  };
}

main();
```

- [ ] **Step 4 : vérification manuelle (navigateur)**

Run (depuis `designer/`) : `python3 -m http.server 8000`, ouvrir `http://localhost:8000`. Vérifier :
1. Le textarea affiche le `DEFAULT_LAYOUT` au chargement, et `✓ valide`.
2. Modifier le JSON (ex. casser une couleur en `"red"`) → cliquer **Appliquer** → `✗ invalide` + message d'erreur listé.
3. Réparer + Appliquer → `✓ valide`. Les boutons **Undo/Redo** s'activent ; Undo revient au layout précédent, Redo le réapplique.
4. (Si un device est joignable + CORS en place) renseigner l'URL → **Charger** remplit le textarea ; **Pousser** renvoie « Poussé et persisté ». Sans device, ignorer ce point.

Expected : les 3 premiers points OK, aucune erreur console.

- [ ] **Step 5 : Commit**

```bash
git add designer/js/device.js designer/js/json-view.js designer/js/app.js
git commit -m "Rich_Telemetry: designer wire JSON view + validation + undo + device I/O"
```

---

## Self-Review (effectuée à la rédaction)

**1. Couverture du spec (Plan A uniquement) :**
- Stack vanilla zéro-build + ajv vendorisé → Task 1 ✓
- Modèle de données + undo/redo → Task 3 ✓
- Math de positionnement hybride (geometry) → Task 2 ✓ (le *drag/snap visuel* qui la consomme est Plan B)
- Validation ajv + refs + (signalement ASCII : ajv le couvre via `$defs/ascii` ; l'affichage UI dédié arrive avec l'inspecteur en Plan C) → Task 4 ✓
- JSON avancé bidirectionnel → Task 5 ✓
- Device load/push (CORS prérequis) → Task 5 ✓
- Hors Plan A (renvoyés à B/C) : rendu best-effort, canvas, palette, bibliothèque, inspecteur, pages, export/import fichier.

**2. Placeholders :** les « Plan B/C » dans `index.html` sont des libellés d'UI temporaires intentionnels (zones pas encore peuplées), pas des placeholders de plan ; tout le code exécutable est fourni.

**3. Cohérence des types :** API du modèle (`commit/undo/redo/canUndo/canRedo/subscribe/toJSON/loadJSON/state`) identique entre Task 3, ses tests, `json-view.js` et `app.js`. `createValidator(schema) → validate(layout) → {valid, errors}` identique entre Task 4 et `app.js`/`json-view.js`. Fonctions `geometry` exportées (offsetFor/nearestAnchor/snapPlacement/placeAt) prêtes pour Plan B.

---

## Plans suivants (à rédiger après A)

- **Plan B — Canvas WYSIWYG** : `render.js` (best-effort : label/readout/bar/ring + badges led_ring/sound) ; `canvas.js` (drag + snap via `geometry.js`, sélection, poignées de redim) ; valeurs d'aperçu mock. Le rendu sera vérifié visuellement ; la consommation de `geometry` est déjà testée.
- **Plan C — Panneaux & fichier** : `palette.js` (6 types + bibliothèque de composants partagés), `inspector.js` (props par type + thresholds + valeur mock + signalement ASCII), `pages.js` (CRUD + réordonner), `file-io.js` (export/import `layout.json`). Mutations dédiées ajoutées à `model.js` (TDD node --test).
