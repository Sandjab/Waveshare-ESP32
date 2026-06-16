# Registre de types — Phase 1 (designer) — Plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Côté designer, faire de `designer/js/registry.js` la source unique « par type de composant », d'où palette, défauts, géométrie initiale, champs d'inspecteur et aperçu découlent ; un test de conformité garantit que le registre couvre exactement les types du schema.

**Architecture:** Un module `registry.js` exporte `COMPONENTS` (une entrée par type : `label`, `defaults`, `makePlacement`, `centered`, `physical`, `compFields`, `placeFields`, `mockFields`, `build`). Les builders d'aperçu **restent** dans `render.js` (double-maintenance du rendu firmware) ; le registre les *référence* via une signature normalisée. Les consommateurs (`palette.js`, `mutations.js`, `inspector.js`, `canvas.js`) lisent le registre au lieu de leurs tables locales. Migration consommateur-par-consommateur, chaque commit laisse l'éditeur fonctionnel et `node --test` vert.

**Tech Stack:** JS ES modules vanilla (zéro-build), `node:test`, ajv vendorisé (non touché ici). Tests : `node --test` depuis `designer/`. Vérif DOM : navigateur (non couvert par node, comme le reste de l'éditeur).

**Répertoire de travail :** toutes les commandes s'exécutent depuis `devices/guition_knob/projects/Rich_Telemetry/designer/` sauf mention contraire.

**Note sur la vérification navigateur :** servir depuis la **racine du projet** sur un **port neuf** (piège du cache de modules ES — cf. HANDOFF) :
```bash
# depuis devices/guition_knob/projects/Rich_Telemetry/
python3 -m http.server 8090
# puis ouvrir http://localhost:8090/designer/
```

**Résidus assumés (hors Phase 1, à ne pas traiter ici) :** les poignées de redimensionnement par type dans `canvas.js` (`addBarHandles`/`addRingHandles`, branche `applySelection`) restent du code interne canvas ; `pickFontPx` (`render.js`) plafonne l'aperçu à 28 px (best-effort) ; le firmware (Phase 2).

---

## File Structure

- **Create** `designer/js/registry.js` — registre `COMPONENTS`, source unique côté designer. Importe les builders de `render.js` et `snapPlacement` de `geometry.js`.
- **Create** `designer/tests/registry.test.js` — conformité (registre ↔ schema) + forme des entrées.
- **Modify** `designer/js/palette.js` — consomme `COMPONENTS` (labels, `makePlacement`, `defaults`) ; supprime `TYPES`, `makePlacement` local et l'import `DEFAULTS`/`snapPlacement`.
- **Modify** `designer/js/mutations.js` — supprime `DEFAULTS` (déplacé dans le registre).
- **Modify** `designer/tests/mutations.test.js` — retire l'import et le test `DEFAULTS` (couvert par `registry.test.js`).
- **Modify** `designer/js/inspector.js` — consomme `COMPONENTS[type].{compFields,placeFields,mockFields}` ; supprime les 3 tables locales. Garde `FONTS` + `makeInput`.
- **Modify** `designer/js/canvas.js` — `buildNode`/`position`/`render`/draggabilité pilotés par le registre ; ajuste l'import `render.js`.

---

## Task 1: Test de conformité (écrit avant le registre)

**Files:**
- Test: `designer/tests/registry.test.js`

- [ ] **Step 1: Write the failing test**

Créer `designer/tests/registry.test.js` :

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { COMPONENTS } from '../js/registry.js';

const schema = JSON.parse(
  readFileSync(new URL('../../schema/layout.schema.json', import.meta.url))
);

// Types déclarés par le schema : component.oneOf → comp_* → properties.type.const.
function schemaTypes() {
  const defs = schema.$defs;
  return defs.component.oneOf.map(ref => {
    const name = ref.$ref.split('/').pop();   // '#/$defs/comp_ring' → 'comp_ring'
    return defs[name].properties.type.const;
  });
}

test('le registre couvre exactement les types du schema', () => {
  const reg = Object.keys(COMPONENTS).sort();
  const sch = schemaTypes().sort();
  assert.deepEqual(reg, sch);
});

test('chaque entrée a les clés requises et un defaults() cohérent', () => {
  for (const [type, def] of Object.entries(COMPONENTS)) {
    for (const k of ['label', 'defaults', 'makePlacement', 'centered',
                     'physical', 'compFields', 'placeFields', 'mockFields', 'build']) {
      assert.ok(k in def, `${type} : clé '${k}' manquante`);
    }
    assert.equal(typeof def.defaults, 'function');
    assert.equal(def.defaults().type, type, `${type}.defaults().type doit valoir '${type}'`);
  }
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node --test tests/registry.test.js`
Expected: FAIL — `Cannot find module '../js/registry.js'` (le registre n'existe pas encore).

- [ ] **Step 3: Commit**

```bash
git add tests/registry.test.js
git commit -m "Rich_Telemetry: designer — test de conformité registre↔schema (rouge)"
```

---

## Task 2: Créer `registry.js`

**Files:**
- Create: `designer/js/registry.js`

- [ ] **Step 1: Create the registry module**

Créer `designer/js/registry.js` :

```js
// Registre unique des types de composants (designer). Source de vérité côté éditeur :
// palette, défauts, géométrie initiale, champs d'inspecteur et aperçu en découlent.
// Le test de conformité (tests/registry.test.js) vérifie que ces clés == les types du schema.
// L'aperçu (build) reste dans render.js (double-maintenance du rendu firmware) ; ici on le référence
// via une signature normalisée (comp, placement, mock).
import { snapPlacement } from './geometry.js';
import { buildLabel, buildReadout, buildBar, buildRing } from './render.js';

// Placement initial d'un widget d'écran : ancrage + offset déduits du point de dépôt (boîte ~0).
const screenPlacement = (id, x, y) => {
  const { anchor, dx, dy } = snapPlacement(x, y, 0, 0, 16);
  return { ref: id, anchor, dx, dy };
};

export const COMPONENTS = {
  label: {
    label: 'Label',
    defaults: () => ({ type: 'label', text: 'Texte', font: 20, color: '#FFFFFF' }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['text', 'Texte', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
    mockFields: [],
    build: (comp) => buildLabel(comp),
  },
  readout: {
    label: 'Lecture',
    defaults: () => ({ type: 'readout', label: 'Label', font: 20, color: '#FFFFFF' }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['label', 'Label', 'asciitext'], ['unit', 'Unité', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
    mockFields: [['value', 'Valeur (aperçu)']],
    build: (comp, pl, mock) => buildReadout(comp, mock),
  },
  bar: {
    label: 'Barre',
    defaults: () => ({ type: 'bar', label: 'Bar', min: 0, max: 100, color: '#38BDF8' }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num'], ['width', 'Largeur', 'num'], ['height', 'Hauteur', 'num']],
    mockFields: [['value', 'Valeur (aperçu)']],
    build: (comp, pl, mock) => buildBar(comp, pl, mock),
  },
  ring: {
    label: 'Anneau',
    defaults: () => ({ type: 'ring', color: '#38BDF8', pill: true, min: 0, max: 100 }),
    makePlacement: (id) => ({ ref: id, radius: 80, thickness: 16, gap_deg: 70 }),
    centered: true, physical: false,
    compFields: [['color', 'Couleur', 'color'],
                 ['pill', 'Pastille %', 'bool', c => !c.center_pct],         // ignoré quand center_pct (prioritaire)
                 ['center_pct', 'Centre %', 'bool'],
                 ['font', 'Police centre', 'font', c => !!c.center_pct],     // dimensionne le chiffre central
                 ['center_color', 'Couleur centre', 'color', c => !!c.center_pct],
                 ['countdown', 'Countdown', 'bool'], ['min', 'Min', 'num'], ['max', 'Max', 'num']],
    placeFields: [['radius', 'Rayon', 'num'], ['thickness', 'Épaisseur', 'num'], ['gap_deg', 'Ouverture°', 'num'], ['start_angle', 'Angle départ°', 'num']],
    mockFields: [['value', 'Valeur % (aperçu)'], ['reset_in_s', 'Countdown (s)']],
    build: (comp, pl, mock) => buildRing(comp, pl, mock),
  },
  led_ring: {
    label: 'LED ring',
    defaults: () => ({ type: 'led_ring', color: '#FFFFFF', brightness: 64 }),
    makePlacement: (id) => ({ ref: id }),
    centered: false, physical: true,
    compFields: [['color', 'Couleur', 'color'], ['brightness', 'Luminosité (0-255)', 'num']],
    placeFields: [],
    mockFields: [],
    build: null,   // physique : rendu en badge hors canvas
  },
  sound: {
    label: 'Son',
    defaults: () => ({ type: 'sound' }),
    makePlacement: (id) => ({ ref: id }),
    centered: false, physical: true,
    compFields: [],
    placeFields: [],
    mockFields: [],
    build: null,
  },
};
```

- [ ] **Step 2: Run the conformance test to verify it passes**

Run: `node --test tests/registry.test.js`
Expected: PASS (2 tests).

- [ ] **Step 3: Run the full suite (no regression)**

Run: `node --test`
Expected: PASS — 71 tests existants + 2 nouveaux = 73, 0 fail.

- [ ] **Step 4: Commit**

```bash
git add js/registry.js
git commit -m "Rich_Telemetry: designer — registry.js (source unique des types), conformité verte"
```

---

## Task 3: `palette.js` consomme le registre

**Files:**
- Modify: `designer/js/palette.js`

- [ ] **Step 1: Replace imports + remove local TYPES/makePlacement**

Dans `designer/js/palette.js`, remplacer les lignes 5–20 (les deux imports, `TYPES`, `makePlacement`) par :

```js
import { uniqueId, addComponent, addPlacement } from './mutations.js';
import { COMPONENTS } from './registry.js';
```

(On supprime l'import `DEFAULTS`, l'import `snapPlacement` de `geometry.js`, la const `TYPES` et la fonction `makePlacement` — tout vient désormais du registre.)

- [ ] **Step 2: Drive the palette list from the registry**

Remplacer la boucle des créateurs de type (anciennement `for (const [type, libelle] of TYPES)`) par :

```js
  for (const [type, def] of Object.entries(COMPONENTS)) {
    const item = document.createElement('div');
    item.className = 'palette-item';
    item.draggable = true;
    item.dataset.type = type;
    item.textContent = def.label;
    item.addEventListener('dragstart', e => e.dataTransfer.setData('text/rt-type', type));
    list.appendChild(item);
  }
```

- [ ] **Step 3: Drive create/place from the registry**

Dans le handler `drop`, remplacer le `model.commit(...)` interne par :

```js
    model.commit(s => {
      if (type) {
        const id = uniqueId(s, type);
        addComponent(s, id, COMPONENTS[type].defaults());
        addPlacement(s, pi, COMPONENTS[type].makePlacement(id, x, y));
      } else {
        const existing = s.components[ref];
        if (!existing) return;                            // ref disparue : rien à placer
        addPlacement(s, pi, COMPONENTS[existing.type].makePlacement(ref, x, y));
      }
      newIndex = s.pages[pi].place.length - 1;
    });
```

- [ ] **Step 4: Run the suite (imports resolve, no regression)**

Run: `node --test`
Expected: PASS — 73 tests, 0 fail. (`palette.js` n'a pas de test unitaire ; ce run confirme qu'aucun module importé n'est cassé.)

- [ ] **Step 5: Browser smoke**

Servir (cf. en-tête) et ouvrir `http://localhost:8090/designer/`. Vérifier : la palette affiche les 6 types (`Label, Lecture, Barre, Anneau, LED ring, Son`) ; glisser **chaque** type sur le canvas crée le widget attendu (anneau centré ; led_ring/sound en badge ; label/readout/bar au point de dépôt).
Expected: comportement identique à avant.

- [ ] **Step 6: Commit**

```bash
git add js/palette.js
git commit -m "Rich_Telemetry: designer — palette pilotée par le registre"
```

---

## Task 4: Retirer `DEFAULTS` de `mutations.js`

**Files:**
- Modify: `designer/js/mutations.js`
- Modify: `designer/tests/mutations.test.js`

- [ ] **Step 1: Remove the DEFAULTS export**

Dans `designer/js/mutations.js`, supprimer le bloc `export const DEFAULTS = { … };` (les 6 fabriques par type). Le reste du module (`uniqueId`, `addComponent`, `addPlacement`, `removePlacement`, `setComponentProp`, `setPlacementProp`, `setThresholds`, `addPage`, …) est inchangé.

- [ ] **Step 2: Update the test file**

Dans `designer/tests/mutations.test.js` :
- retirer `DEFAULTS,` de la liste d'import (ligne ~4) ;
- supprimer le test `test('DEFAULTS produit une définition valide par type', …)` (couvert désormais par `tests/registry.test.js`, qui assert `defaults().type === type`).

- [ ] **Step 3: Run the suite**

Run: `node --test`
Expected: PASS — 72 tests (73 − 1 test DEFAULTS retiré), 0 fail. Aucune erreur d'import (`DEFAULTS` n'est plus référencé : Task 3 a déjà basculé `palette.js`).

- [ ] **Step 4: Commit**

```bash
git add js/mutations.js tests/mutations.test.js
git commit -m "Rich_Telemetry: designer — DEFAULTS déplacé dans le registre"
```

---

## Task 5: `inspector.js` consomme les champs du registre

**Files:**
- Modify: `designer/js/inspector.js`

- [ ] **Step 1: Import the registry, remove the 3 local tables**

Dans `designer/js/inspector.js` :
- ajouter en tête, après l'import `mutations.js` : `import { COMPONENTS } from './registry.js';`
- supprimer les trois const `COMP_FIELDS`, `PLACE_FIELDS`, `MOCK_FIELDS` (leurs définitions complètes).
- **Conserver** `const FONTS = [14, 20, 28, 36, 48];` et la fonction `makeInput` (la liste de polices est un détail de `makeInput`, pas une donnée par type).

- [ ] **Step 2: Point the 3 usages at the registry**

Remplacer les trois accès :

```js
    // dans renderExtras(body, c) :
    const gf = COMPONENTS[c.type].placeFields;
    // …
    const mf = COMPONENTS[c.type].mockFields;
```
```js
    // dans render(), boucle des champs de composant :
    for (const [key, label, kind, enableWhen] of COMPONENTS[c.type].compFields) {
```

(Ces trois listes ne sont jamais `undefined` — chaque type du registre définit les trois, vides si besoin — donc le `|| []` devient inutile.)

- [ ] **Step 3: Run the suite**

Run: `node --test`
Expected: PASS — 72 tests, 0 fail (`inspector.js` non testé en node ; run de non-régression d'imports).

- [ ] **Step 4: Browser smoke**

Ouvrir l'éditeur, sélectionner un widget de **chaque** type :
- les champs de composant/placement/aperçu sont identiques à avant ;
- sur un **anneau** : `center_pct` décoché ⇒ `Police centre`/`Couleur centre` grisés ; coché ⇒ `Pastille %` grisé (le grisage live fonctionne toujours).
Expected: comportement identique.

- [ ] **Step 5: Commit**

```bash
git add js/inspector.js
git commit -m "Rich_Telemetry: designer — inspecteur piloté par les champs du registre"
```

---

## Task 6: `canvas.js` consomme le registre (build/position/physique/draggabilité)

**Files:**
- Modify: `designer/js/canvas.js`

- [ ] **Step 1: Adjust imports**

Dans `designer/js/canvas.js` :
- l'import `render.js` (lignes 7–10) ne garde que ce qui est encore appelé directement : `import { buildBadge, ringPaths, pickThresholdColor } from './render.js';` (les builders `buildLabel/buildReadout/buildBar/buildRing` sont désormais référencés *via le registre*).
- ajouter : `import { COMPONENTS } from './registry.js';`
- supprimer la const `const DRAGGABLE = new Set(['label', 'readout', 'bar']);`.

- [ ] **Step 2: buildNode via le registre**

Remplacer `buildNode` par :

```js
  function buildNode(pl, comp) {
    return COMPONENTS[comp.type].build(comp, pl, getMock(pl.ref, comp.type));
  }
```

- [ ] **Step 3: position via `centered`**

Dans `position(node, pl, comp)`, remplacer `if (comp.type === 'ring') {` par :

```js
    if (COMPONENTS[comp.type].centered) {                 // centré, ignore anchor/dx/dy
```

- [ ] **Step 4: render() — repli défini sur type inconnu + badge via `physical`**

Dans `render()`, juste après `if (!comp) return;`, ajouter une garde de type inconnu, puis remplacer la branche physique. Le bloc devient :

```js
      const comp = comps()[pl.ref];
      if (!comp) return;                         // ref inconnue : la validation le signale déjà
      const def = COMPONENTS[comp.type];
      if (!def) return;                          // type inconnu : signalé par la validation, on ne le dessine pas (repli défini, pas un buildLabel silencieux)
      if (def.physical) {
        badges.appendChild(buildBadge(pl.ref, comp));
        return;
      }
```

- [ ] **Step 5: draggabilité via le registre**

Dans `onPointerDown`, remplacer `if (!DRAGGABLE.has(comp.type)) return;` par :

```js
    const def = COMPONENTS[comp.type];
    if (def.centered || def.physical) return;             // ring centré / physique : non déplaçable
```

- [ ] **Step 6: Run the suite**

Run: `node --test`
Expected: PASS — 72 tests, 0 fail.

- [ ] **Step 7: Browser smoke (non-régression complète de l'éditeur)**

Ouvrir l'éditeur. Vérifier sur **chaque** type :
- aperçu rendu correctement (label/readout/bar au bon endroit, anneau centré, led_ring/sound en badge) ;
- déplacement : label/readout/bar déplaçables ; anneau non déplaçable mais redimensionnable ; badges non déplaçables ;
- sélection + inspecteur OK ; création depuis la palette OK ; export `layout.json` valide (panneau « ✓ valide »).
Expected: comportement identique à `master` avant la Phase 1.

- [ ] **Step 8: Commit**

```bash
git add js/canvas.js
git commit -m "Rich_Telemetry: designer — canvas piloté par le registre (build/position/physique)"
```

---

## Task 7: Vérification finale

- [ ] **Step 1: Full suite**

Run: `node --test`
Expected: PASS — 72 tests, 0 fail.

- [ ] **Step 2: Revue « ajouter un type » (validation du but)**

Relire `js/registry.js` : confirmer qu'ajouter une 7e entrée fictive (sans l'ajouter au schema) ferait **échouer** `tests/registry.test.js` (preuve que le garde-fou mord). Optionnel : l'essayer en local, voir le rouge, annuler.

- [ ] **Step 3: Grep de non-résidu**

Run: `grep -rn "DEFAULTS\|const TYPES\|COMP_FIELDS\|PLACE_FIELDS\|MOCK_FIELDS" js/`
Expected: aucune occurrence (toutes les tables par type ont migré dans le registre ; seuls subsistent les usages `COMPONENTS[...].compFields` etc.).

- [ ] **Step 4: Browser final**

Smoke complet de l'éditeur (création/édition/suppression/placement/export) sur les 6 types — identique à avant.

Phase 1 terminée. Phase 2 (firmware : table de noms + vtable + test C natif) = plan séparé.
```
