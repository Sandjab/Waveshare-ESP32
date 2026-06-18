# Rich_Telemetry — Zone « Device » pour composants physiques : plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Éditer les composants physiques (`led_ring`, `sound`) exclusivement dans une nouvelle zone « Device » du designer (hors pages), reflétant leur nature de sorties device-globales.

**Architecture:** Changement **designer-seul** (firmware & schéma inchangés — le firmware pilote déjà les physiques globalement, et un composant peut vivre dans `components` sans placement). Helpers purs dans `js/physical.js` (testés node), panneau UI `js/device-panel.js` calqué sur `js/sources.js`, retrait des physiques de la palette/canvas, migration douce des anciens layouts. Généralisé par le flag `physical` du registre.

**Tech Stack:** designer ES modules (vanilla JS) + `node --test` ; pas de build. Aucun code firmware.

**Répertoire :** `devices/guition_knob/projects/Rich_Telemetry/` (commandes depuis là ; tests designer depuis `designer/`).

**Spec :** `docs/superpowers/specs/2026-06-18-rt-physical-device-zone-design.md`.

**Note commits :** chaque commit termine par `Claude-Session: https://claude.ai/code/session_01Dx1wnFnR1mzCbAHVJa2N3H` (ligne précédée d'une ligne vide). Omise ci-dessous pour la lisibilité.

---

## Récapitulatif des fichiers touchés

- `designer/js/registry.js` — Modify : `singleton: true` sur `led_ring`.
- `designer/js/physical.js` — **Create** : helpers purs (types physiques, add/remove, migration, cardinalité).
- `designer/tests/physical.test.js` — **Create** : tests node des helpers.
- `designer/js/device-panel.js` — **Create** : panneau « Device » (UI), calqué sur `sources.js`.
- `designer/index.html` — Modify : section `<details>` + `<div id="device">` dans le `<footer>`.
- `designer/js/app.js` — Modify : instancier le panneau + brancher la migration (démarrage / Charger / import).
- `designer/js/palette.js` — Modify : exclure les physiques (liste de types, bibliothèque, drop).
- `designer/js/canvas.js` — Modify : ne plus rendre les physiques en badge.

---

## Task 1 : Flag `singleton` sur `led_ring` (registry)

**Files:**
- Modify: `designer/js/registry.js` (entrée `led_ring`, ~l.86-94)

- [ ] **Step 1 : Ajouter le flag**

Dans l'entrée `led_ring`, ajouter `singleton: true` sur la ligne des flags. Remplacer :

```js
    centered: false, physical: true,
    compFields: [['color', 'Couleur', 'color'], ['brightness', 'Luminosité (0-255)', 'num']],
```

par (uniquement dans le bloc `led_ring:`) :

```js
    centered: false, physical: true, singleton: true,
    compFields: [['color', 'Couleur', 'color'], ['brightness', 'Luminosité (0-255)', 'num']],
```

(Ne PAS toucher `sound:` — il reste sans `singleton`, donc 0..N.)

- [ ] **Step 2 : Vérifier l'absence de régression (le test de conformité registre↔schema)**

Run: `cd designer && node --test`
Expected: PASS, même total qu'avant (ajouter une clé au registre ne change pas les *types* validés par `registry.test.js`).

- [ ] **Step 3 : Commit**

```bash
git add designer/js/registry.js
git commit -m "feat(Rich_Telemetry designer): flag singleton sur led_ring"
```

---

## Task 2 : Helpers purs `js/physical.js` (testés node)

**Files:**
- Create: `designer/js/physical.js`
- Create: `designer/tests/physical.test.js`

- [ ] **Step 1 : Écrire les tests qui échouent**

Créer `designer/tests/physical.test.js` :

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  isPhysicalType, physicalTypes, physicalComponentIds,
  addPhysicalComponent, removeComponent, stripPhysicalPlacements, canAddType
} from '../js/physical.js';

const fresh = () => ({
  components: { titre: { type: 'label', text: 'Hi' } },
  pages: [{ name: 'P1', place: [{ ref: 'titre', anchor: 'CENTER' }] }]
});

test('isPhysicalType : led_ring/sound physiques, label/inconnu non', () => {
  assert.equal(isPhysicalType('led_ring'), true);
  assert.equal(isPhysicalType('sound'), true);
  assert.equal(isPhysicalType('label'), false);
  assert.equal(isPhysicalType('inconnu'), false);
});

test('physicalTypes : contient led_ring et sound, pas label', () => {
  const ts = physicalTypes();
  assert.ok(ts.includes('led_ring') && ts.includes('sound'));
  assert.equal(ts.includes('label'), false);
});

test('addPhysicalComponent : ajoute dans components SANS placement', () => {
  const s = fresh();
  const id = addPhysicalComponent(s, 'led_ring');
  assert.equal(s.components[id].type, 'led_ring');
  assert.equal(s.pages.some(p => p.place.some(pl => pl.ref === id)), false);
});

test('addPhysicalComponent : id unique par type', () => {
  const s = fresh();
  assert.notEqual(addPhysicalComponent(s, 'sound'), addPhysicalComponent(s, 'sound'));
});

test('physicalComponentIds : ne renvoie que les physiques', () => {
  const s = fresh();
  const id = addPhysicalComponent(s, 'led_ring');
  assert.deepEqual(physicalComponentIds(s), [id]);   // 'titre' (label) exclu
});

test('removeComponent : purge components + placements sur toutes les pages', () => {
  const s = {
    components: { led: { type: 'led_ring' }, titre: { type: 'label' } },
    pages: [
      { name: 'P1', place: [{ ref: 'led' }, { ref: 'titre' }] },
      { name: 'P2', place: [{ ref: 'led' }] }
    ]
  };
  removeComponent(s, 'led');
  assert.equal('led' in s.components, false);
  assert.equal(s.components.titre.type, 'label');
  assert.deepEqual(s.pages[0].place, [{ ref: 'titre' }]);
  assert.deepEqual(s.pages[1].place, []);
});

test('stripPhysicalPlacements : retire physiques, garde visuels + composants', () => {
  const s = {
    components: { led: { type: 'led_ring' }, buzz: { type: 'sound' }, titre: { type: 'label' } },
    pages: [
      { name: 'P1', place: [{ ref: 'led' }, { ref: 'titre' }] },
      { name: 'P2', place: [{ ref: 'buzz' }] }
    ]
  };
  stripPhysicalPlacements(s);
  assert.deepEqual(s.pages[0].place, [{ ref: 'titre' }]);
  assert.deepEqual(s.pages[1].place, []);
  assert.ok(s.components.led && s.components.buzz && s.components.titre);   // composants conservés
});

test('stripPhysicalPlacements : idempotent', () => {
  const s = { components: { led: { type: 'led_ring' } }, pages: [{ name: 'P1', place: [{ ref: 'led' }] }] };
  stripPhysicalPlacements(s); stripPhysicalPlacements(s);
  assert.deepEqual(s.pages[0].place, []);
});

test('canAddType : led_ring singleton (true puis false)', () => {
  const s = fresh();
  assert.equal(canAddType(s, 'led_ring'), true);
  addPhysicalComponent(s, 'led_ring');
  assert.equal(canAddType(s, 'led_ring'), false);
});

test('canAddType : sound 0..N (toujours true)', () => {
  const s = fresh();
  addPhysicalComponent(s, 'sound');
  assert.equal(canAddType(s, 'sound'), true);
});
```

- [ ] **Step 2 : Lancer pour voir échouer**

Run: `cd designer && node --test`
Expected: FAIL (`physical.js` introuvable).

- [ ] **Step 3 : Créer `designer/js/physical.js`**

```js
// Helpers « composants physiques » (sorties device globales : led_ring, sound). PURS (testés node).
// Source de vérité des types : le flag `physical` du registre. Les physiques vivent dans `components`
// SANS placement ; le firmware les pilote globalement (cf. spec 2026-06-18-rt-physical-device-zone).
import { COMPONENTS } from './registry.js';
import { uniqueId, addComponent } from './mutations.js';

export function isPhysicalType(type) {
  return !!COMPONENTS[type]?.physical;
}

export function physicalTypes() {
  return Object.keys(COMPONENTS).filter(t => COMPONENTS[t].physical);
}

export function physicalComponentIds(state) {
  const comps = state.components || {};
  return Object.keys(comps).filter(id => isPhysicalType(comps[id].type));
}

// Ajoute un composant physique global : entrée dans `components`, AUCUN placement. Retourne l'id.
export function addPhysicalComponent(state, type) {
  const id = uniqueId(state, type);
  addComponent(state, id, COMPONENTS[type].defaults());
  return id;
}

// Supprime un composant de `components` ET retire tout placement le référençant sur toutes les pages.
export function removeComponent(state, id) {
  if (state.components) delete state.components[id];
  for (const page of state.pages || []) {
    if (page.place) page.place = page.place.filter(pl => pl.ref !== id);
  }
}

// Migration : retire les placements dont le composant référencé est physique (composants conservés).
export function stripPhysicalPlacements(state) {
  const comps = state.components || {};
  for (const page of state.pages || []) {
    if (page.place) page.place = page.place.filter(pl => !isPhysicalType(comps[pl.ref]?.type));
  }
}

// Cardinalité : un type marqué `singleton` (ex. led_ring) ne peut exister qu'en un exemplaire.
export function canAddType(state, type) {
  if (!COMPONENTS[type]?.singleton) return true;
  return !Object.values(state.components || {}).some(c => c.type === type);
}
```

- [ ] **Step 4 : Lancer les tests**

Run: `cd designer && node --test`
Expected: PASS (tous les `physical.test.js` verts, le reste inchangé).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/physical.js designer/tests/physical.test.js
git commit -m "feat(Rich_Telemetry designer): helpers purs composants physiques (physical.js)"
```

---

## Task 3 : Panneau « Device » (`device-panel.js` + HTML + wiring)

Browser-only (pas couvert par node ; validé navigateur/on-device en Task 6). Le garde-fou node = `node --test` reste vert (imports propres).

**Files:**
- Create: `designer/js/device-panel.js`
- Modify: `designer/index.html` (footer)
- Modify: `designer/js/app.js` (import + instanciation)

- [ ] **Step 1 : Créer `designer/js/device-panel.js`**

```js
// Panneau « Device » : édite les composants physiques (sorties globales : led_ring, sound), HORS pages.
// Ils vivent dans `components` sans placement ; le firmware les pilote globalement. Calqué sur
// sources.js (cards, commit sur 'change', garde-focus). Réutilise les classes CSS src-* (zéro CSS neuf).
import { COMPONENTS } from './registry.js';
import { setComponentProp } from './mutations.js';
import { physicalTypes, physicalComponentIds, addPhysicalComponent, removeComponent, canAddType } from './physical.js';

function fieldInput(kind, value, onChange) {
  const el = document.createElement('input');
  if (kind === 'color') {
    el.type = 'color'; el.value = value || '#FFFFFF';
    el.addEventListener('change', () => onChange(el.value.toUpperCase()));
  } else if (kind === 'num') {
    el.type = 'number'; el.value = value ?? '';
    el.addEventListener('change', () => onChange(el.value === '' ? '' : Number(el.value)));
  } else {
    el.type = 'text'; el.value = value ?? '';
    el.addEventListener('change', () => onChange(el.value));
  }
  return el;
}

function labelled(text, input) {
  const l = document.createElement('label'); l.className = 'src-field';
  const s = document.createElement('span'); s.textContent = text;
  l.appendChild(s); l.appendChild(input);
  return l;
}

export function createDevicePanel(root, model) {
  function render() {
    // Garde-focus : ne pas reconstruire pendant l'édition d'un champ du panneau.
    if (root.contains(document.activeElement) && document.activeElement !== document.body) return;
    root.replaceChildren();
    const comps = model.state.components || {};

    for (const id of physicalComponentIds(model.state)) {
      const c = comps[id];
      const def = COMPONENTS[c.type];
      const card = document.createElement('div'); card.className = 'src-card';

      const head = document.createElement('div'); head.className = 'src-head';
      const title = document.createElement('span'); title.className = 'src-title';
      title.textContent = `${id} · ${def.label}`;
      const del = document.createElement('button'); del.className = 'src-del'; del.textContent = 'Supprimer';
      del.addEventListener('click', () => model.commit(s => removeComponent(s, id)));
      head.appendChild(title); head.appendChild(del);
      card.appendChild(head);

      for (const [key, label, kind] of (def.compFields || [])) {
        card.appendChild(labelled(label, fieldInput(kind, c[key], v => model.commit(s => setComponentProp(s, id, key, v)))));
      }
      root.appendChild(card);
    }

    for (const type of physicalTypes()) {
      const add = document.createElement('button'); add.className = 'src-add';
      add.textContent = '+ ' + COMPONENTS[type].label;
      add.disabled = !canAddType(model.state, type);
      add.addEventListener('click', () => model.commit(s => addPhysicalComponent(s, type)));
      root.appendChild(add);
    }
  }

  model.subscribe(render);
  render();
  return { render };
}
```

- [ ] **Step 2 : Ajouter la section dans `index.html`**

Dans `designer/index.html`, juste après la balise `<footer>` (avant la section « Sources »), insérer :

```html
    <details>
      <summary>Device (sorties physiques)</summary>
      <div id="device" class="sources-panel"></div>
    </details>
```

- [ ] **Step 3 : Instancier le panneau dans `app.js`**

Ajouter l'import (près des autres, ex. après la ligne `import { createSources } from './sources.js';`) :

```js
import { createDevicePanel } from './device-panel.js';
```

Et l'instancier juste après la ligne `createSources($('sources'), model);` :

```js
  // Panneau Device : composants physiques (led_ring/sound) édités hors pages (sorties globales).
  createDevicePanel($('device'), model);
```

- [ ] **Step 4 : Vérifier (pas de régression node ; le module importe proprement)**

Run: `cd designer && node --test`
Expected: PASS (device-panel.js n'est pas importé par les tests, mais physical.js l'est et reste vert).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/device-panel.js designer/index.html designer/js/app.js
git commit -m "feat(Rich_Telemetry designer): panneau Device pour composants physiques"
```

---

## Task 4 : Retirer les physiques des pages (palette + canvas)

**Files:**
- Modify: `designer/js/palette.js`
- Modify: `designer/js/canvas.js`

- [ ] **Step 1 : `palette.js` — exclure les physiques de la liste de types**

Dans la boucle des créateurs de type (`for (const [type, def] of Object.entries(COMPONENTS)) {`), ajouter en **première ligne** du corps :

```js
    if (def.physical) continue;   // physiques : édités dans le panneau « Device », pas glissables sur une page
```

- [ ] **Step 2 : `palette.js` — exclure les physiques de la bibliothèque draggable**

Dans `renderLibrary`, remplacer :

```js
    const ids = Object.keys(comps);
```

par :

```js
    const ids = Object.keys(comps).filter(id => !COMPONENTS[comps[id].type]?.physical);
```

- [ ] **Step 3 : `palette.js` — garde au drop**

Dans le handler `stage.addEventListener('drop', …)`, juste après la ligne `if (!type && !ref) return;`, ajouter :

```js
    if (type && COMPONENTS[type]?.physical) return;                       // type physique : pas de placement
```

Et dans la branche `else` (placement d'un existant), remplacer :

```js
        const existing = s.components[ref];
        if (!existing) return;                            // ref disparue : rien à placer
```

par :

```js
        const existing = s.components[ref];
        if (!existing || COMPONENTS[existing.type]?.physical) return;     // ref disparue / physique : pas de placement
```

- [ ] **Step 4 : `canvas.js` — ne plus rendre les physiques en badge**

Dans `render()`, remplacer le bloc (~l.105-108) :

```js
      if (def.physical) {
        badges.appendChild(buildBadge(pl.ref, comp));
        return;
      }
```

par :

```js
      if (def.physical) return;   // physiques édités dans le panneau « Device » ; jamais rendus sur une page
```

(Après ce changement, `buildBadge` peut devenir un import inutilisé dans `canvas.js` : si c'est le cas, retire la ligne d'import correspondante. Le conteneur `#badges` reste vide — inoffensif.)

- [ ] **Step 5 : Vérifier (pas de régression node)**

Run: `cd designer && node --test`
Expected: PASS.

- [ ] **Step 6 : Commit**

```bash
git add designer/js/palette.js designer/js/canvas.js
git commit -m "feat(Rich_Telemetry designer): retirer les composants physiques de la palette/canvas"
```

---

## Task 5 : Migration douce des placements physiques (app.js)

Applique `stripPhysicalPlacements` à l'entrée d'un layout : autosave au démarrage, « Charger » device, import fichier. **Pas** au panneau « JSON avancé » (échappatoire brute, laissée telle quelle — cf. spec).

**Files:**
- Modify: `designer/js/app.js`

- [ ] **Step 1 : Importer le helper**

Ajouter `stripPhysicalPlacements` à un import depuis `./physical.js` (nouvel import, près des autres) :

```js
import { stripPhysicalPlacements } from './physical.js';
```

- [ ] **Step 2 : Migration au démarrage (autosave localStorage)**

Remplacer :

```js
  let saved;
  try { const s = localStorage.getItem(SAVE_KEY); if (s) saved = JSON.parse(s); } catch (e) {}
  const model = createModel(saved);
```

par :

```js
  let saved;
  try { const s = localStorage.getItem(SAVE_KEY); if (s) saved = JSON.parse(s); } catch (e) {}
  if (saved) stripPhysicalPlacements(saved);   // migration : physiques jamais attachés à une page
  const model = createModel(saved);
```

- [ ] **Step 3 : Migration au « Charger » device**

Dans le handler `$('load').onclick`, remplacer :

```js
      const base = $('base').value;
      model.loadJSON(JSON.stringify(await loadLayout(base)));
```

par :

```js
      const base = $('base').value;
      const lay = await loadLayout(base);
      stripPhysicalPlacements(lay);            // migration avant chargement dans le modèle
      model.loadJSON(JSON.stringify(lay));
```

- [ ] **Step 4 : Migration après import fichier**

Dans l'appel `bindFileIO(...)`, remplacer le `onLoad` :

```js
    onLoad: () => { canvas.setPage(0); pages.render(); }
```

par :

```js
    onLoad: () => { model.commit(s => stripPhysicalPlacements(s)); canvas.setPage(0); pages.render(); }
```

- [ ] **Step 5 : Vérifier (pas de régression node)**

Run: `cd designer && node --test`
Expected: PASS.

- [ ] **Step 6 : Commit**

```bash
git add designer/js/app.js
git commit -m "feat(Rich_Telemetry designer): migration douce des placements physiques au chargement"
```

---

## Task 6 : Validation navigateur + on-device

Designer embarqué `http://192.168.1.35/designer/` (re-stager après les changements JS). Device branché ; workflow [[feedback-device-validation-workflow]]. Backup/restore du layout autour des tests.

- [ ] **Step 1 : Backup du layout device**

Run: `curl --max-time 6 http://192.168.1.35/layout -o /tmp/rt-phys-backup.json && wc -c /tmp/rt-phys-backup.json`
Expected: fichier non vide.

- [ ] **Step 2 : Re-stager le designer embarqué + flasher le FS**

Run (depuis le projet) :
```bash
bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs
```
Expected: SUCCESS. ⚠ `uploadfs` écrase le layout persisté → re-POST du backup en Step 7. (Le firmware `src/` n'a PAS changé → pas besoin de re-flasher l'app, seulement le FS pour le designer à jour.)

- [ ] **Step 3 : Charger le layout device dans le designer, vérifier la migration**

Ouvrir `http://192.168.1.35/designer/`, saisir l'IP dans le champ Device, **Charger**. Vérifier :
- Le panneau **« Device (sorties physiques) »** (pied) liste `led`/`buzz` (ou équivalents) avec leurs champs (led_ring : couleur + luminosité ; sound : aucun champ) + boutons Supprimer.
- La **palette** ne propose plus « LED ring » ni « Son » ; la **bibliothèque** ne les liste plus ; le **canvas** n'affiche plus les badges physiques.
- Bouton **« + LED ring » désactivé** si un led_ring existe déjà ; **« + Son »** actif.

- [ ] **Step 4 : Éditer un physique + ajouter un son, puis Pousser**

Changer la couleur du led_ring dans le panneau Device, cliquer **« + Son »** (crée `sound1`), **Pousser**. Vérifier le statut « Poussé et persisté ».

- [ ] **Step 5 : Confirmer côté device que le comportement physique est inchangé**

```bash
curl --max-time 6 -s http://192.168.1.35/layout | python3 -c "import sys,json; d=json.load(sys.stdin); phys=[k for k,v in d['components'].items() if v['type'] in ('led_ring','sound')]; placed=any(pl.get('ref') in phys for p in d['pages'] for pl in p.get('place',[])); print('physiques:', phys, '| encore placés sur une page ?', placed)"
```
Expected: les composants physiques présents dans `components`, **`placed = False`** (aucun placement physique).

- [ ] **Step 6 : Pousser une commande LED via /update et capturer**

```bash
# adapter l'id au led_ring réel (cf. Step 5) ; mode solide rouge
curl --max-time 6 -s -X POST http://192.168.1.35/update -H 'Content-Type: application/json' -d '{"led":{"mode":"solid","color":"#FF0000"}}'
curl --max-time 10 -s http://192.168.1.35/screenshot -o /tmp/phys.bmp && sips -s format png /tmp/phys.bmp --out /tmp/phys.png >/dev/null 2>&1 && echo capturé
```
Confirmer (anneau RGB physique allumé ; l'écran n'est pas affecté par le led_ring). Envoyer `/tmp/phys.png` si pertinent.

- [ ] **Step 7 : Restaurer le layout d'origine**

```bash
curl --max-time 8 -s -X POST http://192.168.1.35/layout -H 'Content-Type: application/json' --data-binary @/tmp/rt-phys-backup.json
```
Expected: `{"ok":true}`.

---

## Self-review (auteur)

**Couverture du spec :**
- Édition exclusive dans la zone « Device » → Task 3 (panneau) + Task 4 (retrait pages). ✓
- Généralisation par le flag `physical` → `physical.js` lit `COMPONENTS[].physical` (Task 2). ✓
- Cardinalité led_ring singleton / sound 0..N → flag `singleton` (Task 1) + `canAddType` (Task 2) + bouton désactivé (Task 3). ✓
- Migration automatique (démarrage / Charger / import), exclut JSON avancé → Task 5. ✓
- Firmware & schéma inchangés → aucune task ne les touche. ✓
- Tests node des helpers → Task 2. ✓
- Validation navigateur + on-device → Task 6. ✓

**Placeholders :** aucun « TBD/TODO » ; chaque pas de code montre le code exact. Le seul renvoi conditionnel (Task 4 Step 4 : « si `buildBadge` devient un import inutilisé, retire-le ») est une consigne précise, pas un placeholder de logique.

**Cohérence des types/noms :** `physical.js` exporte `isPhysicalType`, `physicalTypes`, `physicalComponentIds`, `addPhysicalComponent`, `removeComponent`, `stripPhysicalPlacements`, `canAddType` — ces mêmes noms sont importés par `device-panel.js` (add/remove/types/ids/canAddType) et `app.js` (stripPhysicalPlacements). `singleton` posé en Task 1 est lu par `canAddType` (Task 2). `setComponentProp`/`uniqueId`/`addComponent` réutilisés (existants). Cohérent.
