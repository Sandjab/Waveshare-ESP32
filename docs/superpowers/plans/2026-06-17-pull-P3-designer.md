# Pull P3 + chart/meter — Designer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Aligner le designer WYSIWYG (web app) sur le firmware déjà déployé : éditer les `sources` (pull réseau), exposer le champ `bind` par composant, et ajouter les types `chart`/`meter` (schema + registre + aperçu), de sorte que le designer produise tout ce que le firmware parse déjà.

**Architecture :** Le `schema/layout.schema.json` est le contrat partagé firmware↔designer (source de vérité ; un test de conformité JS et un test C le vérifient). On le fait évoluer en premier (sources + bind), puis le designer suit : mutations pures testées sous `node --test`, modules DOM vérifiés au navigateur. Les types `chart`/`meter` sont ajoutés de bout en bout (schema + registre + aperçu render dans le **même lot**) pour ne jamais casser la conformité. **Décisions de cadrage (validées 2026-06-17) : (1) les secrets restent HORS du designer — conforme au spec pull (« absent du layout, du designer et de l'export ») ; le schema n'ajoute donc PAS `secrets` top-level, et poser un secret reste un `POST /secrets` manuel ; (2) l'éditeur de `sources` est un panneau dépliable dans le footer, à côté de « JSON avancé ».**

**Tech Stack :** JavaScript vanilla zéro-build (modules ES), ajv vendorisé (browser+Node), `node --test` pour la logique pure, vérification navigateur (Playwright MCP) pour le câblage DOM. Schema JSON Schema draft-07. Firmware C++ natif (`pio test -e native`) comme garde-fou du contrat.

---

## Contexte firmware (source de vérité — déjà sur `origin/master`)

Le firmware parse **déjà** tout ce que ce plan ajoute au designer. Références vérifiées :

- **`bind`** (`src/dashboard.cpp:75`, `src/dashboard.h:30`) : `strlcpy(c.bind, o["bind"] | "", …)`. Appliqué par `context_apply()` (`dashboard.cpp:249-289`) aux types **bar / ring / readout / label / meter / chart** (PAS led_ring/sound). Vide ⇒ push par id (comportement actuel).
- **`points`** (`dashboard.cpp:76-78`) : `c.chart_points = o["points"] | 30`, borné `[1, CHART_MAX_POINTS]` (`CHART_MAX_POINTS=60`, `config.h:6`). Spécifique au chart.
- **`sources`** top-level (`dashboard.cpp:111-133`) : tableau d'objets `{ name, url (requis — rejet si vide), interval_s (défaut 60, plancher `CTX_MIN_INTERVAL_S=5`), headers {nom:valeur}, vars {nom:JSON Pointer} }`. Bornes : `MAX_SOURCES=6`, `MAX_HEADERS_PER_SOURCE=4`, `MAX_VARS_PER_SOURCE=6` (`config.h:8-9`).
- **`secrets`** : **jamais parsé dans `dash_set_layout`**. Store write-only via `POST /secrets` (`api.cpp:38-42`, `secret_store.h`), jamais relu, absent du layout/export. ⇒ le schema ne l'autorise pas (et `additionalProperties:false` au top-level rejettera tout layout en contenant — voulu).
- **chart** (`view.cpp:181-203`) : `lv_chart` LINE, taille `width|200 × height|100`, range Y `min..max`, série couleur `color` ; historique tenu dans le modèle (`hist[]`), mirroir dans `sync`.
- **meter** (`view.cpp:208-234`) : `lv_meter`, taille `width|160 × height|160`, `lv_meter_set_scale_range(min, max, 270, 135)` (arc **270°**, rotation **135°** ⇒ ouvert en bas), zones d'arc depuis `thresholds` (bande `i` = `(prev, limit[i]]`, `prev` démarre à `min`), aiguille couleur `color`.

Convention d'angle déjà en place dans `render.js` (`pointOnArc`, `arcPath`) : **0° = droite (3h), sens horaire, y vers le bas** — identique à LVGL. Réutilisable tel quel pour le meter.

## File Structure

| Fichier | Rôle | Action |
|---|---|---|
| `schema/layout.schema.json` | Contrat partagé | Modifier : `$defs/source` + `sources` top-level + `bind` (comp_label/readout/bar/ring) ; puis (lot chart/meter) `comp_chart`/`comp_meter` + `points` + oneOf |
| `designer/tests/schema.test.js` | Test ajv du schema (nouveau) | Créer : valide un layout sources+bind ; rejette une source sans url ; (lot chart/meter) valide chart+meter |
| `designer/js/mutations.js` | Ops layout pures | Modifier : `uniqueSourceName`/`addSource`/`removeSource`/`setSourceProp`/`setSourceHeaders`/`setSourceVars` |
| `designer/tests/mutations.test.js` | Tests des mutations | Modifier : cas sources |
| `designer/js/sources.js` | Panneau d'édition des sources (nouveau) | Créer |
| `designer/js/registry.js` | Registre des types | Modifier : `bind` sur types existants ; entrées `chart`/`meter` |
| `designer/js/render.js` | Aperçu best-effort | Modifier : `sparklinePoints`/`meterAngle` (pures) + `buildChart`/`buildMeter` + `MOCKS.chart`/`MOCKS.meter` |
| `designer/tests/render.test.js` | Tests de la math d'aperçu | Modifier : `sparklinePoints`/`meterAngle` |
| `designer/js/inspector.js` | Inspecteur | Modifier : éditeur de seuils étendu au `meter` (libellé dynamique) |
| `designer/index.html` | Coquille | Modifier : `<details>` Sources dans le footer |
| `designer/js/app.js` | Bootstrap | Modifier : instancier `createSources` |
| `designer/style.css` | Styles | Modifier : classes du panneau sources + aperçu chart/meter |

**Ordre des tâches** (chaque commit reste vert ; aucune divergence schema↔registre transitoire) :
1. Schema `sources` + `bind` (n'introduit aucun type ⇒ conformité intacte).
2. Mutations sources (pures, TDD).
3. Panneau `sources.js` + câblage (navigateur).
4. Champ `bind` dans l'inspecteur (registre, types existants).
5. Aperçu `buildChart`/`buildMeter` + math (TDD).
6. chart/meter de bout en bout : schema + registre + inspecteur (schema et registre bougent **ensemble** ⇒ conformité intacte).

**Branche :** créer une branche de feature à l'exécution (ex. `feat/rt-designer-p3`) — ne pas committer directement sur `master`. (Isolation worktree au choix de l'exécutant via `superpowers:using-git-worktrees`.)

**Commandes de référence :**
- Tests designer : `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
- Garde-fou contrat firmware (sans carte) : `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native`
- Servir le designer (navigateur) : depuis `devices/guition_knob/projects/Rich_Telemetry/`, `python3 -m http.server <port-neuf>` puis `http://localhost:<port>/designer/` (servir depuis le **parent** pour que `../schema` soit accessible ; **port neuf** à chaque fois — piège du cache de modules ES).
- Le hook pre-commit imprime un warning jaune `SCHEMA DIVERGENT` : **non bloquant**, pré-existant, sans rapport ; le commit passe.

---

## Task 1 : Schema — `sources` top-level + `bind` (types existants)

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json`
- Create: `devices/guition_knob/projects/Rich_Telemetry/designer/tests/schema.test.js`

- [ ] **Step 1 : Écrire le test ajv (échoue : schema pas encore étendu)**

Créer `designer/tests/schema.test.js` :

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createValidator } from '../js/validate.js';

const schema = JSON.parse(
  readFileSync(new URL('../../schema/layout.schema.json', import.meta.url))
);
const validate = createValidator(schema);

// Layout minimal valide réutilisé par les cas (un composant + une page).
function base() {
  return {
    components: { t: { type: 'readout', unit: 'C' } },
    pages: [{ name: 'P1', place: [{ ref: 't', anchor: 'CENTER' }] }]
  };
}

test('schema : sources top-level valides (url/interval/headers/vars)', () => {
  const l = base();
  l.sources = [{
    name: 'weather',
    url: 'https://api.example/w?city=Paris',
    interval_s: 600,
    headers: { 'X-API-Key': '$weather_key' },
    vars: { temp: '/main/temp' }
  }];
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : une source sans url est rejetée', () => {
  const l = base();
  l.sources = [{ name: 'bad', interval_s: 600 }];
  assert.equal(validate(l).valid, false);
});

test('schema : interval_s sous le plancher 5 est rejeté', () => {
  const l = base();
  l.sources = [{ url: 'http://x', interval_s: 2 }];
  assert.equal(validate(l).valid, false);
});

test('schema : champ bind accepté sur un composant data', () => {
  const l = base();
  l.components.t.bind = 'temp';
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : secrets top-level reste interdit (write-only, hors layout)', () => {
  const l = base();
  l.secrets = { weather_key: 'xxx' };
  assert.equal(validate(l).valid, false);
});
```

- [ ] **Step 2 : Lancer le test pour le voir échouer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/schema.test.js`
Expected: FAIL — `sources`/`bind` sont des propriétés inconnues (le top-level et les `comp_*` sont en `additionalProperties:false`).

- [ ] **Step 3 : Ajouter `sources` au top-level du schema**

Dans `schema/layout.schema.json`, dans `properties` (après le bloc `pages`, vers la ligne 29), ajouter la propriété `sources`. Remplacer :

```json
    "pages": {
      "type": "array",
      "description": "Liste ordonnee de pages.",
      "items": { "$ref": "#/$defs/page" }
    }
  },
```

par :

```json
    "pages": {
      "type": "array",
      "description": "Liste ordonnee de pages.",
      "items": { "$ref": "#/$defs/page" }
    },
    "sources": {
      "type": "array",
      "description": "Pull reseau (P2/P3). Chaque source fetch son url a son interval et ecrit ses vars dans le contexte ; un composant bind:\"var\" lit la valeur. SECRETS NON inclus ici : poses via POST /secrets (write-only), references par $nom dans headers.",
      "maxItems": 6,
      "items": { "$ref": "#/$defs/source" }
    }
  },
```

- [ ] **Step 4 : Ajouter `$defs/source`**

Dans `$defs`, après le bloc `placement` (avant le `}` fermant de `$defs`, vers la ligne 180), ajouter :

```json
    ,
    "source": {
      "type": "object",
      "required": ["url"],
      "additionalProperties": false,
      "description": "Une source de pull (1 source = 1 url). url requis (le firmware rejette une source sans url).",
      "properties": {
        "name": { "type": "string", "description": "Nom de la source (libelle ; apparait dans GET /status)." },
        "url": { "type": "string", "description": "URL HTTP(S) a interroger." },
        "interval_s": { "type": "integer", "minimum": 5, "description": "Periode de fetch en secondes. Defaut 60. Plancher impose 5 (CTX_MIN_INTERVAL_S)." },
        "headers": {
          "type": "object",
          "description": "En-tetes HTTP. Une valeur \"$nom\" reference un secret du store write-only (resolu au fetch).",
          "additionalProperties": { "type": "string" }
        },
        "vars": {
          "type": "object",
          "description": "Map nom_de_variable -> JSON Pointer (RFC 6901, ex. \"/main/temp\") extrait de la reponse.",
          "additionalProperties": { "type": "string" }
        }
      }
    }
```

- [ ] **Step 5 : Ajouter `bind` aux composants data existants**

Ajouter la propriété `bind` dans `properties` de **`comp_label`**, **`comp_readout`**, **`comp_bar`** et **`comp_ring`**. Le fragment à insérer (à placer après la propriété `type` de chaque bloc) est identique :

```json
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id. Absent = push par id (defaut)." },
```

Concrètement, dans `comp_label` remplacer :

```json
        "type": { "const": "label" },
        "text": { "$ref": "#/$defs/ascii", "description": "Texte statique (peut etre remplace via /update avec une string)." },
```

par :

```json
        "type": { "const": "label" },
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id. Absent = push par id (defaut)." },
        "text": { "$ref": "#/$defs/ascii", "description": "Texte statique (peut etre remplace via /update avec une string)." },
```

dans `comp_readout` remplacer :

```json
        "type": { "const": "readout" },
        "label": { "$ref": "#/$defs/ascii", "description": "Libelle affiche devant la valeur (ex. \"CPU\")." },
```

par :

```json
        "type": { "const": "readout" },
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id. Absent = push par id (defaut)." },
        "label": { "$ref": "#/$defs/ascii", "description": "Libelle affiche devant la valeur (ex. \"CPU\")." },
```

dans `comp_bar` remplacer :

```json
        "type": { "const": "bar" },
        "label": { "$ref": "#/$defs/ascii", "description": "Libelle affiche au-dessus de la barre." },
```

par :

```json
        "type": { "const": "bar" },
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id. Absent = push par id (defaut)." },
        "label": { "$ref": "#/$defs/ascii", "description": "Libelle affiche au-dessus de la barre." },
```

dans `comp_ring` remplacer :

```json
        "type": { "const": "ring" },
        "color": { "$ref": "#/$defs/hexColor" },
```

par :

```json
        "type": { "const": "ring" },
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id. Absent = push par id (defaut)." },
        "color": { "$ref": "#/$defs/hexColor" },
```

- [ ] **Step 6 : Lancer le test schema → vert**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/schema.test.js`
Expected: PASS (5 tests).

- [ ] **Step 7 : Non-régression — toute la suite designer + conformité registre**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (tous, dont `registry.test.js` toujours vert : aucun nouveau type introduit).

- [ ] **Step 8 : Garde-fou contrat firmware (sans carte)**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native`
Expected: PASS — le test C de conformité lit le schema ; `sources`/`bind` ne sont pas des types, donc inchangé côté firmware.

- [ ] **Step 9 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json \
        devices/guition_knob/projects/Rich_Telemetry/designer/tests/schema.test.js
git commit -m "Rich_Telemetry: schema — sources top-level + champ bind (contrat P3)"
```

---

## Task 2 : Mutations de sources (pures, TDD)

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/js/mutations.js`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/tests/mutations.test.js`

- [ ] **Step 1 : Écrire les tests (échouent : fonctions absentes)**

Ajouter à la fin de `designer/tests/mutations.test.js` :

```js
import {
  uniqueSourceName, addSource, removeSource,
  setSourceProp, setSourceHeaders, setSourceVars
} from '../js/mutations.js';

test('addSource ajoute une source nommee avec interval par defaut', () => {
  const s = { components: {}, pages: [] };
  addSource(s, 'weather');
  assert.deepEqual(s.sources, [{ name: 'weather', interval_s: 60 }]);
});

test('uniqueSourceName evite les collisions', () => {
  const s = { sources: [{ name: 'source1' }, { name: 'source2' }] };
  assert.equal(uniqueSourceName(s), 'source3');
  assert.equal(uniqueSourceName({}), 'source1');
});

test('setSourceProp pose une valeur, vide => supprime la cle', () => {
  const s = { sources: [{ name: 'a', interval_s: 60 }] };
  setSourceProp(s, 0, 'url', 'http://x');
  assert.equal(s.sources[0].url, 'http://x');
  setSourceProp(s, 0, 'url', '');
  assert.equal('url' in s.sources[0], false);
});

test('setSourceHeaders/setSourceVars remplacent ou suppriment', () => {
  const s = { sources: [{ name: 'a' }] };
  setSourceHeaders(s, 0, { 'X-Key': '$k' });
  assert.deepEqual(s.sources[0].headers, { 'X-Key': '$k' });
  setSourceHeaders(s, 0, {});
  assert.equal('headers' in s.sources[0], false);
  setSourceVars(s, 0, { temp: '/t' });
  assert.deepEqual(s.sources[0].vars, { temp: '/t' });
  setSourceVars(s, 0, {});
  assert.equal('vars' in s.sources[0], false);
});

test('removeSource retire par index', () => {
  const s = { sources: [{ name: 'a' }, { name: 'b' }] };
  removeSource(s, 0);
  assert.deepEqual(s.sources, [{ name: 'b' }]);
});

test('setSourceProp / setSourceHeaders no-op sur index invalide', () => {
  const s = { sources: [] };
  setSourceProp(s, 3, 'url', 'http://x');   // ne doit pas throw
  setSourceHeaders(s, 3, { a: 'b' });
  assert.deepEqual(s.sources, []);
});
```

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/mutations.test.js`
Expected: FAIL (imports non résolus / fonctions absentes).

- [ ] **Step 3 : Implémenter les mutations**

Ajouter à la fin de `designer/js/mutations.js` :

```js
// --- Sources (pull reseau, P3). Top-level state.sources (array d'objets plats). ---

// Nom libre <source><n> : 1er entier sans collision avec les noms existants.
export function uniqueSourceName(state) {
  const used = new Set((state.sources || []).map(s => s.name));
  let n = 1;
  while (used.has(`source${n}`)) n++;
  return `source${n}`;
}

// Ajoute une source en fin de liste. url absente volontairement (l'utilisateur la saisit ;
// url requise par le schema => signalee invalide tant qu'elle est vide).
export function addSource(state, name) {
  (state.sources ||= []).push({ name, interval_s: 60 });
}

export function removeSource(state, index) {
  if (!state.sources) return;
  state.sources.splice(index, 1);
}

// Edite name/url/interval_s. Valeur vide => suppression de la cle (parite avec setComponentProp).
export function setSourceProp(state, index, key, value) {
  const s = state.sources?.[index];
  if (!s) return;
  if (value === '' || value === null || value === undefined) delete s[key];
  else s[key] = value;
}

// Remplace l'objet headers (reconstruit cote UI depuis une liste de paires). Vide => supprime la cle.
export function setSourceHeaders(state, index, headers) {
  const s = state.sources?.[index];
  if (!s) return;
  if (headers && Object.keys(headers).length) s.headers = headers;
  else delete s.headers;
}

// Remplace l'objet vars (nom -> JSON Pointer). Vide => supprime la cle.
export function setSourceVars(state, index, vars) {
  const s = state.sources?.[index];
  if (!s) return;
  if (vars && Object.keys(vars).length) s.vars = vars;
  else delete s.vars;
}
```

- [ ] **Step 4 : Lancer pour voir passer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/mutations.test.js`
Expected: PASS.

- [ ] **Step 5 : Suite complète**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (tous).

- [ ] **Step 6 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/mutations.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/tests/mutations.test.js
git commit -m "Rich_Telemetry: designer — mutations de sources (pures, testees)"
```

---

## Task 3 : Panneau d'édition des sources + câblage

**Files:**
- Create: `devices/guition_knob/projects/Rich_Telemetry/designer/js/sources.js`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/index.html`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/js/app.js`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/style.css`

Pas de test `node` (câblage DOM) — vérifié au navigateur (Step 6).

- [ ] **Step 1 : Créer le module `sources.js`**

Créer `designer/js/sources.js` :

```js
// Panneau d'edition des sources de pull (config reseau top-level, hors canvas). Commit sur 'change'
// (1 undo/edition, pas de flood). headers/vars edites comme listes de paires, reconstruits en objets
// au commit (meme pattern que l'editeur de seuils du ring). S'abonne au modele ; garde-focus pour ne
// pas reconstruire pendant la frappe. Les SECRETS ne sont PAS geres ici (POST /secrets manuel).
import {
  uniqueSourceName, addSource, removeSource,
  setSourceProp, setSourceHeaders, setSourceVars
} from './mutations.js';

const MAX_SOURCES = 6, MAX_PAIRS = 6;  // miroir config.h (MAX_SOURCES, MAX_HEADERS/VARS_PER_SOURCE=4/6)

// Convertit un objet {k:v} en liste de paires editables [[k,v],...].
const toPairs = obj => Object.entries(obj || {}).map(([k, v]) => [k, v]);
// Reconstruit un objet depuis des paires, en ignorant celles a cle vide.
const fromPairs = pairs => Object.fromEntries(pairs.filter(([k]) => k !== ''));

function textInput(value, onChange, placeholder) {
  const el = document.createElement('input');
  el.type = 'text'; el.value = value ?? ''; if (placeholder) el.placeholder = placeholder;
  el.addEventListener('change', () => onChange(el.value));
  return el;
}

function numInput(value, onChange) {
  const el = document.createElement('input');
  el.type = 'number'; el.value = value ?? '';
  el.addEventListener('change', () => onChange(el.value === '' ? '' : Number(el.value)));
  return el;
}

function row(...kids) {
  const r = document.createElement('div'); r.className = 'src-row';
  for (const k of kids) r.appendChild(k);
  return r;
}

function labelled(text, input) {
  const l = document.createElement('label'); l.className = 'src-field';
  const s = document.createElement('span'); s.textContent = text;
  l.appendChild(s); l.appendChild(input);
  return l;
}

export function createSources(root, model) {
  function render() {
    // Garde-focus : ne pas reconstruire si un champ du panneau est en cours d'edition.
    if (root.contains(document.activeElement) && document.activeElement !== document.body) return;
    root.replaceChildren();
    const sources = model.state.sources || [];

    sources.forEach((src, i) => {
      const card = document.createElement('div'); card.className = 'src-card';

      const head = document.createElement('div'); head.className = 'src-head';
      const title = document.createElement('span'); title.className = 'src-title';
      title.textContent = src.name || `source ${i + 1}`;
      const del = document.createElement('button'); del.className = 'src-del'; del.textContent = 'Supprimer';
      del.addEventListener('click', () => model.commit(s => removeSource(s, i)));
      head.appendChild(title); head.appendChild(del);
      card.appendChild(head);

      card.appendChild(labelled('Nom', textInput(src.name, v => model.commit(s => setSourceProp(s, i, 'name', v)))));
      card.appendChild(labelled('URL', textInput(src.url, v => model.commit(s => setSourceProp(s, i, 'url', v)), 'https://…')));
      card.appendChild(labelled('Intervalle (s)', numInput(src.interval_s, v => model.commit(s => setSourceProp(s, i, 'interval_s', v)))));

      // --- Headers (paires nom -> valeur ; "$nom" = reference a un secret) ---
      card.appendChild(pairEditor(
        'En-tetes (valeur "$nom" = secret)', toPairs(src.headers), 'Nom', 'Valeur',
        pairs => model.commit(s => setSourceHeaders(s, i, fromPairs(pairs)))
      ));

      // --- Vars (paires nom -> JSON Pointer) ---
      card.appendChild(pairEditor(
        'Variables (nom -> JSON Pointer)', toPairs(src.vars), 'Variable', '/chemin/json',
        pairs => model.commit(s => setSourceVars(s, i, fromPairs(pairs)))
      ));

      root.appendChild(card);
    });

    const add = document.createElement('button'); add.className = 'src-add';
    add.textContent = '+ source';
    add.disabled = sources.length >= MAX_SOURCES;
    add.addEventListener('click', () => model.commit(s => addSource(s, uniqueSourceName(s))));
    root.appendChild(add);
  }

  // Editeur generique de map (headers/vars) : liste de paires + ligne d'ajout. onCommit recoit
  // la liste de paires courante (cle vide ignoree cote mutation).
  function pairEditor(title, pairs, kPlaceholder, vPlaceholder, onCommit) {
    const box = document.createElement('div'); box.className = 'src-pairs';
    const sub = document.createElement('div'); sub.className = 'src-sub'; sub.textContent = title;
    box.appendChild(sub);
    pairs.forEach((p, idx) => {
      const k = textInput(p[0], v => { pairs[idx][0] = v; onCommit(pairs); }, kPlaceholder);
      const v = textInput(p[1], v => { pairs[idx][1] = v; onCommit(pairs); }, vPlaceholder);
      const rm = document.createElement('button'); rm.className = 'src-pair-rm'; rm.textContent = '×';
      rm.addEventListener('click', () => { pairs.splice(idx, 1); onCommit(pairs); });
      box.appendChild(row(k, v, rm));
    });
    const add = document.createElement('button'); add.className = 'src-pair-add'; add.textContent = '+';
    add.disabled = pairs.length >= MAX_PAIRS;
    add.addEventListener('click', () => { pairs.push(['', '']); onCommit(pairs); });
    box.appendChild(add);
    return box;
  }

  model.subscribe(render);
  render();
  return { render };
}
```

- [ ] **Step 2 : Ajouter le panneau au footer (`index.html`)**

Dans `designer/index.html`, dans le `<footer>`, remplacer :

```html
  <footer>
    <details open>
      <summary>JSON avancé</summary>
```

par :

```html
  <footer>
    <details>
      <summary>Sources (pull réseau)</summary>
      <div id="sources" class="sources-panel"></div>
    </details>
    <details open>
      <summary>JSON avancé</summary>
```

- [ ] **Step 3 : Instancier dans `app.js`**

Dans `designer/js/app.js`, ajouter l'import après la ligne `import { bindFileIO } from './file-io.js';` :

```js
import { createSources } from './sources.js';
```

Puis, après le bloc `bindFileIO(...)` (avant `bindJsonView(...)`), ajouter :

```js
  // Panneau Sources (pull réseau) : édition des sources top-level. Indépendant du canvas/pages.
  createSources($('sources'), model);
```

- [ ] **Step 4 : Styles du panneau (`style.css`)**

Ajouter à la fin de `designer/style.css` :

```css
/* --- Panneau Sources (footer) --- */
.sources-panel { display: flex; flex-direction: column; gap: 10px; padding: 6px 0; }
.src-card { border: 1px solid #1F2937; border-radius: 6px; padding: 8px; display: flex; flex-direction: column; gap: 6px; }
.src-head { display: flex; align-items: center; justify-content: space-between; }
.src-title { font-weight: 600; color: #93C5FD; }
.src-del { font-size: 12px; }
.src-field { display: grid; grid-template-columns: 120px 1fr; align-items: center; gap: 8px; }
.src-field > span { color: #9CA3AF; font-size: 13px; }
.src-field input { width: 100%; }
.src-pairs { display: flex; flex-direction: column; gap: 4px; margin-left: 8px; }
.src-sub { color: #9CA3AF; font-size: 12px; margin-top: 4px; }
.src-row { display: flex; gap: 6px; align-items: center; }
.src-row input { flex: 1; min-width: 0; }
.src-pair-rm, .src-pair-add { flex: 0 0 auto; width: 28px; }
.src-add { align-self: flex-start; }
```

- [ ] **Step 5 : Suite `node` (non-régression)**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (inchangé ; sources.js n'est pas testé sous node).

- [ ] **Step 6 : Vérification navigateur (contrôleur)**

Servir depuis `Rich_Telemetry/` sur un port neuf, ouvrir `/designer/`, déplier « Sources ». Vérifier (Playwright MCP) :
1. « + source » crée une carte ; le JSON avancé montre `"sources":[{"name":"source1","interval_s":60}]`.
2. Saisir une URL → le JSON reflète `url`. Tant que l'URL est vide, le panneau d'erreurs signale `source … : propriété obligatoire « url » manquante`.
3. Ajouter un en-tête `X-API-Key` = `$weather_key` et une variable `temp` = `/main/temp` → reflétés dans `headers`/`vars` du JSON ; `✓ valide`.
4. `×` sur une paire la retire ; « Supprimer » retire la source.
5. Undo/redo traverse ces éditions une par une (1 commit/édition).
6. Au-delà de 6 sources, « + source » est désactivé.
Nettoyer les `*.png` de test laissés à la racine du repo.

- [ ] **Step 7 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/sources.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/index.html \
        devices/guition_knob/projects/Rich_Telemetry/designer/js/app.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/style.css
git commit -m "Rich_Telemetry: designer — panneau d'edition des sources (pull P3)"
```

---

## Task 4 : Champ `bind` dans l'inspecteur (types existants)

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/js/registry.js`

Le champ `bind` est une `compField` ordinaire (kind `asciitext`). Aucun test `node` (le câblage de l'inspecteur n'est pas couvert sous node ; la conformité `registry.test.js` ne regarde pas le détail des `compFields`). Vérifié au navigateur.

- [ ] **Step 1 : Ajouter `bind` aux `compFields` de label/readout/bar/ring**

Dans `designer/js/registry.js` :

`label` — remplacer :

```js
    compFields: [['text', 'Texte', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
```

par :

```js
    compFields: [['text', 'Texte', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color'], ['bind', 'Variable (pull)', 'asciitext']],
```

`readout` — remplacer :

```js
    compFields: [['label', 'Label', 'asciitext'], ['unit', 'Unité', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
```

par :

```js
    compFields: [['label', 'Label', 'asciitext'], ['unit', 'Unité', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color'], ['bind', 'Variable (pull)', 'asciitext']],
```

`bar` — remplacer :

```js
    compFields: [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color']],
```

par :

```js
    compFields: [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color'], ['bind', 'Variable (pull)', 'asciitext']],
```

`ring` — remplacer :

```js
    compFields: [['color', 'Couleur', 'color'],
                 ['pill', 'Pastille %', 'bool', c => !c.center_pct],         // ignoré quand center_pct (prioritaire)
                 ['center_pct', 'Centre %', 'bool'],
                 ['font', 'Police centre', 'font', c => !!c.center_pct],     // dimensionne le chiffre central
                 ['center_color', 'Couleur centre', 'color', c => !!c.center_pct],
                 ['countdown', 'Countdown', 'bool'], ['min', 'Min', 'num'], ['max', 'Max', 'num']],
```

par :

```js
    compFields: [['color', 'Couleur', 'color'],
                 ['pill', 'Pastille %', 'bool', c => !c.center_pct],         // ignoré quand center_pct (prioritaire)
                 ['center_pct', 'Centre %', 'bool'],
                 ['font', 'Police centre', 'font', c => !!c.center_pct],     // dimensionne le chiffre central
                 ['center_color', 'Couleur centre', 'color', c => !!c.center_pct],
                 ['countdown', 'Countdown', 'bool'], ['min', 'Min', 'num'], ['max', 'Max', 'num'],
                 ['bind', 'Variable (pull)', 'asciitext']],
```

- [ ] **Step 2 : Conformité + suite `node`**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (les `compFields` ne sont pas contraints par le test de conformité).

- [ ] **Step 3 : Vérification navigateur (contrôleur)**

Sélectionner un `readout`/`bar`/`ring`/`label` : un champ « Variable (pull) » apparaît. Saisir `temp` → le JSON du composant montre `"bind":"temp"` ; effacer → la clé disparaît (`setComponentProp` supprime sur valeur vide) ; `✓ valide`. Un caractère non-ASCII déclenche l'avertissement ⚠ ASCII.

- [ ] **Step 4 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/registry.js
git commit -m "Rich_Telemetry: designer — champ bind dans l'inspecteur (types existants)"
```

---

## Task 5 : Aperçu `chart` + `meter` — math pure + builders

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/js/render.js`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/tests/render.test.js`

Cette tâche ajoute le code d'aperçu mais ne le branche pas encore au registre (Task 6) — `render.js` exporte simplement deux builders + deux helpers de plus ; `registry.test.js` reste vert (aucun type ajouté).

- [ ] **Step 1 : Écrire les tests de la math (échouent)**

Dans `designer/tests/render.test.js`, étendre l'import de tête :

```js
import {
  pickFontPx, barFill, pickThresholdColor, formatValue, formatRemaining,
  ringSweepDeg, pointOnArc, arcPath, ringPaths
} from '../js/render.js';
```

en :

```js
import {
  pickFontPx, barFill, pickThresholdColor, formatValue, formatRemaining,
  ringSweepDeg, pointOnArc, arcPath, ringPaths, sparklinePoints, meterAngle
} from '../js/render.js';
```

Puis ajouter à la fin du fichier :

```js
test('sparklinePoints : points SVG normalises (x reparti, y inverse)', () => {
  assert.equal(sparklinePoints([0, 50, 100], 0, 100, 100, 100),
    '0.00,100.00 50.00,50.00 100.00,0.00');
  assert.equal(sparklinePoints([], 0, 100, 100, 100), '');
  assert.equal(sparklinePoints([42], 0, 100, 100, 100), '0.00,58.00'); // 1 point : x=0, y=100-0.42*100
});

test('meterAngle : 270° de 135° (min) a 405° (max), convention pointOnArc', () => {
  assert.equal(meterAngle(0, 0, 100), 135);
  assert.equal(meterAngle(50, 0, 100), 270);
  assert.equal(meterAngle(100, 0, 100), 405);
});
```

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/render.test.js`
Expected: FAIL (`sparklinePoints`/`meterAngle` non exportés).

- [ ] **Step 3 : Implémenter la math pure dans `render.js`**

Dans `designer/js/render.js`, après la fonction `ringPaths` (vers la ligne 77, avant le commentaire `// --- Builders DOM`), ajouter :

```js
// chart : suite de points SVG "x,y …" pour une polyline. x reparti sur la largeur, y inverse
// (0 en bas) et clampe via barFill. Miroir best-effort de lv_chart LINE (view.cpp:181).
export function sparklinePoints(hist, min, max, w, h) {
  if (!hist || hist.length === 0) return '';
  const n = hist.length;
  const f = v => v.toFixed(2);
  return hist.map((v, i) => {
    const x = n > 1 ? (i / (n - 1)) * w : 0;
    const y = h - barFill(v, min, max) * h;
    return `${f(x)},${f(y)}`;
  }).join(' ');
}

// meter : angle de l'aiguille (deg, convention pointOnArc : 0°=droite, horaire, y bas). Miroir
// lv_meter_set_scale_range(min, max, 270, 135) (view.cpp:216) : 135° a min → 405° a max.
export function meterAngle(value, min, max) {
  return 135 + barFill(value, min, max) * 270;
}
```

- [ ] **Step 4 : Ajouter les mocks chart/meter**

Dans `designer/js/render.js`, remplacer le bloc `MOCKS` (lignes 6-10) :

```js
export const MOCKS = {
  readout: { value: 42 },
  bar:     { value: 60 },
  ring:    { value: 72, reset_in_s: 18000 }
};
```

par :

```js
export const MOCKS = {
  readout: { value: 42 },
  bar:     { value: 60 },
  ring:    { value: 72, reset_in_s: 18000 },
  chart:   { hist: [20, 35, 30, 50, 45, 60, 55, 70, 65, 80, 60, 75, 50, 65, 55, 72] },  // serie demo (forme indicative)
  meter:   { value: 60 }
};
```

- [ ] **Step 5 : Implémenter les builders DOM**

Dans `designer/js/render.js`, après `buildRing` (avant `buildBadge`, vers la ligne 170), ajouter :

```js
export function buildChart(comp, placement, mock = MOCKS.chart) {
  const w = placement.width || 200, h = placement.height || 100;  // defauts firmware (view.cpp:184)
  const wrap = document.createElement('div');
  wrap.className = 'w w-chart';
  wrap.style.width = w + 'px'; wrap.style.height = h + 'px';
  const svg = document.createElementNS(SVGNS, 'svg');
  svg.setAttribute('width', w); svg.setAttribute('height', h);
  svg.setAttribute('viewBox', `0 0 ${w} ${h}`);
  const line = document.createElementNS(SVGNS, 'polyline');
  line.setAttribute('points', sparklinePoints(mock.hist || [], comp.min ?? 0, comp.max ?? 100, w, h));
  line.setAttribute('fill', 'none');
  line.setAttribute('stroke', comp.color || '#38BDF8');
  line.setAttribute('stroke-width', 2);
  svg.appendChild(line);
  wrap.appendChild(svg);
  return wrap;
}

export function buildMeter(comp, placement, mock = MOCKS.meter) {
  const w = placement.width || 160;             // defauts firmware (view.cpp:211-212)
  const h = placement.height || w;
  const size = Math.min(w, h);
  const cx = w / 2, cy = h / 2, r = size / 2 - 6;
  const min = comp.min ?? 0, max = comp.max ?? 100;
  const wrap = document.createElement('div');
  wrap.className = 'w w-meter';
  wrap.style.width = w + 'px'; wrap.style.height = h + 'px';
  const svg = document.createElementNS(SVGNS, 'svg');
  svg.setAttribute('width', w); svg.setAttribute('height', h);
  svg.setAttribute('viewBox', `0 0 ${w} ${h}`);
  const mkPath = (d, stroke, sw) => {
    const p = document.createElementNS(SVGNS, 'path');
    p.setAttribute('d', d); p.setAttribute('fill', 'none');
    p.setAttribute('stroke', stroke); p.setAttribute('stroke-width', sw);
    return p;
  };
  svg.appendChild(mkPath(arcPath(cx, cy, r, 135, 270), '#4B5563', 4));   // arc de fond 270° (view.cpp:216)
  let prev = min;                                                         // zones (prev, limit] (view.cpp:217-224)
  for (const [limit, color] of comp.thresholds || []) {
    const a0 = meterAngle(prev, min, max);
    const a1 = meterAngle(limit, min, max);
    svg.appendChild(mkPath(arcPath(cx, cy, r, a0, a1 - a0), color, 6));
    prev = limit;
  }
  const [nx, ny] = pointOnArc(cx, cy, r - 4, meterAngle(mock.value, min, max));  // aiguille
  const needle = document.createElementNS(SVGNS, 'line');
  needle.setAttribute('x1', cx); needle.setAttribute('y1', cy);
  needle.setAttribute('x2', nx.toFixed(2)); needle.setAttribute('y2', ny.toFixed(2));
  needle.setAttribute('stroke', comp.color || '#38BDF8');
  needle.setAttribute('stroke-width', 3);
  needle.setAttribute('stroke-linecap', 'round');
  svg.appendChild(needle);
  wrap.appendChild(svg);
  return wrap;
}
```

- [ ] **Step 6 : Lancer pour voir passer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/render.test.js`
Expected: PASS.

- [ ] **Step 7 : Suite complète**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (tous ; `registry.test.js` toujours vert — aucun type ajouté).

- [ ] **Step 8 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/render.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/tests/render.test.js
git commit -m "Rich_Telemetry: designer — apercu chart/meter (math pure + builders)"
```

---

## Task 6 : chart/meter de bout en bout — schema + registre + inspecteur

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/js/registry.js`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/js/inspector.js`
- Modify: `devices/guition_knob/projects/Rich_Telemetry/designer/tests/schema.test.js`

Schema et registre bougent **ensemble** ⇒ la conformité `registry.test.js` reste verte à chaque commit.

- [ ] **Step 1 : Étendre le test schema pour chart/meter (échoue)**

Ajouter à la fin de `designer/tests/schema.test.js` :

```js
test('schema : composant chart valide (points + bind)', () => {
  const l = base();
  l.components.g = { type: 'chart', color: '#38BDF8', min: 0, max: 100, points: 30, bind: 'cpu' };
  l.pages[0].place.push({ ref: 'g', anchor: 'CENTER', width: 200, height: 100 });
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : composant meter valide (thresholds + bind)', () => {
  const l = base();
  l.components.m = {
    type: 'meter', color: '#38BDF8', min: 0, max: 100,
    thresholds: [[50, '#22C55E'], [80, '#F59E0B']], bind: 'temp'
  };
  l.pages[0].place.push({ ref: 'm', anchor: 'CENTER', width: 160, height: 160 });
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : propriete inconnue sur un chart est rejetee', () => {
  const l = base();
  l.components.g = { type: 'chart', wat: 1 };
  l.pages[0].place.push({ ref: 'g' });
  assert.equal(validate(l).valid, false);
});
```

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/schema.test.js`
Expected: FAIL — `chart`/`meter` ne sont pas (encore) au schema (oneOf ne matche pas).

Note : `registry.test.js` est aussi rouge à cet instant si on l'exécute — c'est attendu **dans la tâche**, résolu par les Steps suivants (schema + registre ensemble). On ne commit qu'une fois tout vert (Step 7).

- [ ] **Step 3 : Ajouter `comp_chart`/`comp_meter` au schema + oneOf**

Dans `schema/layout.schema.json`, étendre `component.oneOf`. Remplacer :

```json
      "oneOf": [
        { "$ref": "#/$defs/comp_label" },
        { "$ref": "#/$defs/comp_readout" },
        { "$ref": "#/$defs/comp_bar" },
        { "$ref": "#/$defs/comp_ring" },
        { "$ref": "#/$defs/comp_led_ring" },
        { "$ref": "#/$defs/comp_sound" }
      ]
```

par :

```json
      "oneOf": [
        { "$ref": "#/$defs/comp_label" },
        { "$ref": "#/$defs/comp_readout" },
        { "$ref": "#/$defs/comp_bar" },
        { "$ref": "#/$defs/comp_ring" },
        { "$ref": "#/$defs/comp_led_ring" },
        { "$ref": "#/$defs/comp_sound" },
        { "$ref": "#/$defs/comp_chart" },
        { "$ref": "#/$defs/comp_meter" }
      ]
```

Puis ajouter les deux `$defs` après `comp_sound` (avant `page`, vers la ligne 150) :

```json
    "comp_chart": {
      "type": "object",
      "additionalProperties": false,
      "required": ["type"],
      "description": "Graphe d'historique (lv_chart LINE). L'historique vit dans le device ; /update pousse un scalaire (append). Place via width/height/anchor/dx/dy.",
      "properties": {
        "type": { "const": "chart" },
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id." },
        "color": { "$ref": "#/$defs/hexColor", "description": "Couleur de la serie." },
        "min": { "type": "number", "description": "Borne basse de l'axe Y. Defaut 0." },
        "max": { "type": "number", "description": "Borne haute de l'axe Y. Defaut 100." },
        "points": { "type": "integer", "minimum": 1, "maximum": 60, "description": "Longueur de la fenetre d'historique. Defaut 30, borne CHART_MAX_POINTS=60." }
      }
    },
    "comp_meter": {
      "type": "object",
      "additionalProperties": false,
      "required": ["type"],
      "description": "Jauge analogique a aiguille (lv_meter, arc 270°). thresholds reutilises en zones d'arc colorees (bande (prev, limite]). Place via width/height/anchor/dx/dy.",
      "properties": {
        "type": { "const": "meter" },
        "bind": { "$ref": "#/$defs/ascii", "description": "Nom d'une variable du contexte (pull). Present = lit la variable au lieu d'etre pousse par id." },
        "color": { "$ref": "#/$defs/hexColor", "description": "Couleur de l'aiguille." },
        "min": { "type": "number", "description": "Borne basse de l'echelle. Defaut 0." },
        "max": { "type": "number", "description": "Borne haute de l'echelle. Defaut 100." },
        "thresholds": {
          "type": "array",
          "description": "Zones d'arc colorees : chaque [limite, \"#hex\"] colore la bande de la limite precedente (vmin au depart) jusqu'a limite.",
          "items": { "$ref": "#/$defs/threshold" }
        }
      }
    },
```

- [ ] **Step 4 : Ajouter les entrées `chart`/`meter` au registre**

Dans `designer/js/registry.js`, étendre l'import de `render.js`. Remplacer :

```js
import { buildLabel, buildReadout, buildBar, buildRing } from './render.js';
```

par :

```js
import { buildLabel, buildReadout, buildBar, buildRing, buildChart, buildMeter } from './render.js';
```

Puis, dans l'objet `COMPONENTS`, après l'entrée `ring` (avant `led_ring`), ajouter :

```js
  chart: {
    label: 'Graphe',
    defaults: () => ({ type: 'chart', color: '#38BDF8', min: 0, max: 100, points: 30 }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['color', 'Couleur', 'color'], ['min', 'Min', 'num'], ['max', 'Max', 'num'],
                 ['points', 'Points', 'num'], ['bind', 'Variable (pull)', 'asciitext']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num'],
                  ['width', 'Largeur', 'num'], ['height', 'Hauteur', 'num']],
    mockFields: [],
    build: (comp, pl, mock) => buildChart(comp, pl, mock),
  },
  meter: {
    label: 'Jauge',
    defaults: () => ({ type: 'meter', color: '#38BDF8', min: 0, max: 100 }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['color', 'Couleur', 'color'], ['min', 'Min', 'num'], ['max', 'Max', 'num'],
                 ['bind', 'Variable (pull)', 'asciitext']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num'],
                  ['width', 'Largeur', 'num'], ['height', 'Hauteur', 'num']],
    mockFields: [['value', 'Valeur (aperçu)']],
    build: (comp, pl, mock) => buildMeter(comp, pl, mock),
  },
```

- [ ] **Step 5 : Étendre l'éditeur de seuils de l'inspecteur au `meter`**

Dans `designer/js/inspector.js`, dans `renderExtras`, remplacer le bloc :

```js
    // --- Seuils du ring (liste éditable de [limite, #couleur]) ---
    if (c.type === 'ring') {
      sub(body, 'Seuils (couleur si valeur < limite)');
```

par :

```js
    // --- Seuils ring/meter (liste éditable de [limite, #couleur]) ---
    // ring : couleur si valeur < limite ; meter : zone d'arc (limite précédente → limite).
    if (c.type === 'ring' || c.type === 'meter') {
      sub(body, c.type === 'meter' ? 'Zones (couleur de la limite précédente à la limite)'
                                   : 'Seuils (couleur si valeur < limite)');
```

(Le reste du bloc — copie locale `ths`, `commitThs`, lignes +/×, bouton « + seuil » — est inchangé.)

- [ ] **Step 6 : Lancer les tests — schema + conformité + math**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS (tous) — en particulier `schema.test.js` (chart/meter valides) **et** `registry.test.js` (registre == types du schema, chart/meter inclus des deux côtés).

- [ ] **Step 7 : Garde-fou contrat firmware (sans carte)**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native`
Expected: PASS — le test C de conformité vérifie désormais que `parse_type` résout `chart` et `meter` (déjà dans `COMP_NAMES`) ; aucun type orphelin.

- [ ] **Step 8 : Vérification navigateur (contrôleur)**

Servir depuis `Rich_Telemetry/` sur un port neuf. Vérifier (Playwright MCP) :
1. La palette montre « Graphe » et « Jauge ». Glisser « Graphe » sur le canvas → crée un `chart`, aperçu = sparkline ; « Jauge » → `meter`, aperçu = arc 270° + aiguille.
2. Inspecteur chart : Couleur/Min/Max/**Points**/**Variable (pull)** + géométrie (Ancrage/dx/dy/Largeur/Hauteur). Changer la couleur/min/max met à jour l'aperçu.
3. Inspecteur meter : Couleur/Min/Max/**Variable (pull)** + **section Zones** (ajouter [50,#22C55E], [80,#F59E0B]) → arcs colorés dans l'aperçu ; **Valeur (aperçu)** bouge l'aiguille (mock, hors undo/JSON).
4. Le JSON avancé reflète `chart`/`meter` ; `✓ valide`. Undo/redo cohérent.
5. Reprendre brièvement un type existant (ex. ring) pour confirmer la non-régression de l'éditeur de seuils.
Nettoyer les `*.png` de test laissés à la racine du repo.

- [ ] **Step 9 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json \
        devices/guition_knob/projects/Rich_Telemetry/designer/js/registry.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/js/inspector.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/tests/schema.test.js
git commit -m "Rich_Telemetry: designer — types chart/meter de bout en bout (schema + registre + inspecteur)"
```

---

## Mise à jour de la documentation (après les 6 tâches)

- [ ] Mettre à jour `designer/HANDOFF.md` et `docs/superpowers/2026-06-16-HANDOFF-rich-telemetry-post-P2.md` : marquer **Pull P3 + chart/meter designer FAITS** ; ne reste plus de track ouvert (firmware et designer alignés). Mentionner la décision « secrets hors designer » et le panneau Sources dans le footer.
- [ ] Mettre à jour la mémoire `project-rich-telemetry` / `project-rt-designer` en conséquence.

## Self-Review (effectuée à l'écriture)

**Couverture du spec :**
- Pull P3 « éditeur de sources (url/interval/headers/vars) » → Tasks 2-3. ✓
- Pull P3 « champ bind dans l'inspecteur » → Task 4 (types existants) + Task 6 (chart/meter). ✓
- Pull P3 « entrée schema » → Task 1 (`sources` + `bind`). ✓ ; `secrets` volontairement exclu (décision validée + spec « absent du designer »). ✓
- chart/meter « entrée schema + oneOf » → Task 6. ✓
- chart/meter « registry.js (defaults/makePlacement/compFields/placeFields/mockFields/build) » → Task 6. ✓
- chart/meter « buildChart/buildMeter d'aperçu SVG » → Task 5. ✓
- Conformité JS (registre == schema) jamais cassée : Task 6 bouge schema+registre ensemble. ✓
- Garde-fou contrat firmware (`pio test -e native`) après chaque modif schema (Tasks 1 et 6). ✓

**Cohérence des types/signatures :** `buildChart(comp, placement, mock)` / `buildMeter(comp, placement, mock)` — mêmes signatures que `buildBar`/`buildRing`, appelées par `registry.build` via `canvas.buildNode(pl, comp)` → `COMPONENTS[type].build(comp, pl, getMock(...))`. `sparklinePoints`/`meterAngle` exportées de `render.js`, importées par `render.test.js`. Mutations sources : noms identiques entre `sources.js`, `mutations.js` et `mutations.test.js` (`addSource`/`removeSource`/`setSourceProp`/`setSourceHeaders`/`setSourceVars`/`uniqueSourceName`). MOCKS keys (`chart`/`meter`) ↔ `getMock(id, type)` ↔ `mockFields`. ✓

**Pas de placeholder :** chaque step de code donne le contenu verbatim. ✓
