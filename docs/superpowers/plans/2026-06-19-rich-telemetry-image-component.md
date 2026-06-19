# Composant `image` (Rich_Telemetry) — Plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ajouter un type de composant `image` au dashboard Rich_Telemetry : un bitmap statique placé, redimensionnable en étirement libre W×H, avec transparence (RGB565A8).

**Architecture:** Le navigateur (designer) décode/rastérise l'image à W×H exacts → RGB565A8 (3 o/px), hash FNV-1a = clé d'asset, upload `POST /image?key=`. Le firmware charge `/img/<clé>.565a` en PSRAM, monte un `lv_img_dsc_t` `TRUE_COLOR_ALPHA` (w/h lus du composant) et l'affiche en `lv_img`. La taille vit **sur le composant** (`src`/`w`/`h`) — couplée à l'asset, car LVGL 8 ne sait pas étirer non-uniformément ; le placement ne porte que la position.

**Tech Stack:** JS ESM + `node --test` (designer) ; Arduino/PlatformIO + LVGL 8.4 + LittleFS (firmware) ; assets RGB565A8 réutilisant le pipeline du fond de page (`bg-image.js` / `/bgimage`).

**Réfs source de vérité (déjà lues) :**
- Designer : `registry.js`, `render.js`, `canvas.js`, `inspector.js`, `app.js`, `device.js`, `bg-image.js`, `mutations.js`, `schema/layout.schema.json`, `tests/*.test.js`.
- Firmware : `dashboard.h`, `dashboard.cpp`, `view.cpp`, `config.h`, `api.cpp`.
- LVGL 8.4 : `LV_IMG_CF_TRUE_COLOR_ALPHA` à `LV_COLOR_DEPTH=16` ⇒ `data_size = w*h*3` (2 o couleur respectant `LV_COLOR_16_SWAP`, +1 o alpha, interleavé), blendé nativement par `lv_img`.

**Chemins (préfixe commun) :** `devices/guition_knob/projects/Rich_Telemetry/`
- Designer source : `designer/js/…` (stagé vers `data/designer/` par `tools/stage_fs.sh`, gitignoré).
- Firmware : `src/…`. Schéma : `schema/layout.schema.json`.

**Commandes :**
- Tests designer : `cd <préfixe>/designer && node --test` (baseline : 157 pass / 0 fail).
- Staging LittleFS : `<préfixe>/tools/stage_fs.sh`.
- Build firmware : depuis la racine repo, `./build.sh guition_knob Rich_Telemetry` (compile) ; `--upload` (flash) ; `--uploadfs` (LittleFS).

---

## File Structure

| Fichier | Création / Modif | Responsabilité |
|---|---|---|
| `designer/js/image-asset.js` | **Créer** | Conversion image↔RGB565A8 (alpha), cache d'asset + source re-dessinable, clés référencées. Miroir alpha de `bg-image.js`. |
| `designer/tests/image-asset.test.js` | **Créer** | Tests unitaires des fonctions pures (conversion, round-trip, swap, hash, referencedImageKeys). |
| `schema/layout.schema.json` | Modif | `$defs.comp_image` + entrée `oneOf`. |
| `designer/js/registry.js` | Modif | Entrée `image` dans `COMPONENTS`. |
| `designer/js/render.js` | Modif | `buildImage(comp)` + import `previewUrl`. |
| `designer/style.css` | Modif | Styles `.w-image` / placeholder. |
| `designer/js/inspector.js` | Modif | Champ `image` (file picker) bespoke dans la boucle `compFields`. |
| `designer/js/canvas.js` | Modif | `addImageHandles` (resize → `component.w/h` + re-render) + dispatch. |
| `designer/js/device.js` | Modif | `uploadImage` / `fetchImage` (endpoint `/image`). |
| `designer/js/app.js` | Modif | Boucles Load (réhydrate) / Push (upload) pour images de composants. |
| `designer/tests/schema.test.js` | Modif | Cas de validation `comp_image`. |
| `src/config.h` | Modif | `IMG_DIR` / `IMG_MAX_*` / `IMG_PX_BYTES`. |
| `src/dashboard.h` | Modif | `COMP_IMAGE` + champs `image_src/w/h`. |
| `src/dashboard.cpp` | Modif | `COMP_NAMES`, parse `src/w/h`, `apply_image`, vtable `APPLY`. |
| `src/view.cpp` | Modif | `build_image`, `img_load_component`, vtable `VIEW`, libération+chargement PSRAM. |
| `src/api.cpp` | Modif | Handlers `/image` POST/GET, sweep `/img`, routes. |
| `docs/index.html` | Modif | Section manuel du composant image. |

---

## Task 1: Module `image-asset.js` (conversion RGB565A8, TDD)

**Files:**
- Create: `designer/js/image-asset.js`
- Test: `designer/tests/image-asset.test.js`

- [ ] **Step 1: Écrire le test qui échoue**

Create `designer/tests/image-asset.test.js` :

```javascript
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { rgba8888ToRgb565a8, rgb565a8ToRgba8888, referencedImageKeys } from '../js/image-asset.js';
import { SWAP } from '../js/bg-image.js';

test('rgba8888ToRgb565a8 : 1 px rouge opaque → 3 octets [couleur swappée, alpha]', () => {
  // rouge pur R=255,G=0,B=0,A=255 → RGB565 = 0xF800 ; SWAP=true ⇒ octet fort d'abord.
  const out = rgba8888ToRgb565a8(new Uint8ClampedArray([255, 0, 0, 255]));
  assert.equal(out.length, 3);
  assert.deepEqual([...out], SWAP ? [0xF8, 0x00, 0xFF] : [0x00, 0xF8, 0xFF]);
});

test('rgba8888ToRgb565a8 : alpha préservé tel quel', () => {
  const out = rgba8888ToRgb565a8(new Uint8ClampedArray([0, 0, 0, 0x40]));
  assert.equal(out[2], 0x40);
});

test('round-trip 565a8 → rgba conserve l’alpha et approxime la couleur', () => {
  const rgba = new Uint8ClampedArray([248, 0, 0, 0x80]);   // R aligné sur un pas RGB565 (>>3<<3)
  const back = rgb565a8ToRgba8888(rgba8888ToRgb565a8(rgba));
  assert.equal(back[0], 248);
  assert.equal(back[3], 0x80);
});

test('referencedImageKeys : collecte les src des composants type image, dédupliqués', () => {
  const state = { components: {
    a: { type: 'image', src: 'aaaa' },
    b: { type: 'image', src: 'aaaa' },   // doublon
    c: { type: 'image' },                // pas de src
    d: { type: 'bar', src: 'zzzz' },     // pas une image
  } };
  assert.deepEqual(referencedImageKeys(state).sort(), ['aaaa']);
});
```

- [ ] **Step 2: Lancer le test pour vérifier qu'il échoue**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test tests/image-asset.test.js`
Expected: FAIL — `Cannot find module '../js/image-asset.js'`.

- [ ] **Step 3: Implémenter le module**

Create `designer/js/image-asset.js` :

```javascript
// Conversion image -> RGB565A8 (avec alpha) pour les images placées. Le NAVIGATEUR fait tout le
// decodage/rasterisation ; le device n'affiche que du RGB565A8 deja pret (cf. view.cpp build_image).
// Miroir de bg-image.js, mais : (1) 3 octets/px (2 couleur + 1 alpha), (2) etirement LIBRE a w×h (pas
// de cover-crop), (3) garde une SOURCE re-dessinable par composant pour re-rendre au resize.
// Reutilise SWAP (LV_COLOR_16_SWAP=1) et fnv1a64Hex de bg-image.js (meme device, meme contrat).
import { SWAP, fnv1a64Hex } from './bg-image.js';

// RGBA8888 (Uint8ClampedArray) -> RGB565A8 interleaved (Uint8Array, 3 octets/px).
export function rgba8888ToRgb565a8(rgba, swap = SWAP) {
  const px = rgba.length >> 2;
  const out = new Uint8Array(px * 3);
  for (let i = 0, o = 0; i < px; i++) {
    const r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
    const v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    if (swap) { out[o++] = (v >> 8) & 0xFF; out[o++] = v & 0xFF; }
    else      { out[o++] = v & 0xFF;        out[o++] = (v >> 8) & 0xFF; }
    out[o++] = a;
  }
  return out;
}

// RGB565A8 -> RGBA8888 (pour reconstruire un apercu).
export function rgb565a8ToRgba8888(bytes, swap = SWAP) {
  const px = (bytes.length / 3) | 0;
  const out = new Uint8ClampedArray(px * 4);
  for (let i = 0, o = 0; i < px; i++) {
    const b0 = bytes[i * 3], b1 = bytes[i * 3 + 1], a = bytes[i * 3 + 2];
    const v = swap ? (b0 << 8) | b1 : (b1 << 8) | b0;
    const r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
    out[o++] = r5 << 3; out[o++] = g6 << 2; out[o++] = b5 << 3; out[o++] = a;
  }
  return out;
}

// Cles d'asset referencees par des composants image (pour upload/sweep cote app.js).
export function referencedImageKeys(state) {
  return [...new Set(Object.values(state.components || {})
    .map(c => (c && c.type === 'image') ? c.src : null).filter(Boolean))];
}

// --- Cache d'asset (cle -> {bytes, url}) + source re-dessinable par composant (compId -> drawable). ---
// Non persiste : au reload, repeuple via rehydrate() depuis le device (cf. app.js).
const _cache = new Map();     // key -> { bytes: Uint8Array RGB565A8, url: dataURL }
const _sources = new Map();   // compId -> ImageBitmap | HTMLCanvasElement (source pour re-render au resize)

export function cacheBytes(key) { return _cache.get(key)?.bytes || null; }
export function previewUrl(key) { return _cache.get(key)?.url || null; }
export function sourceFor(compId) { return _sources.get(compId) || null; }

// Construit un dataURL d'apercu depuis des octets RGB565A8 (w×h).
function buildUrl(bytes, w, h) {
  const cnv = document.createElement('canvas'); cnv.width = w; cnv.height = h;
  const ctx = cnv.getContext('2d');
  const img = ctx.createImageData(w, h);
  img.data.set(rgb565a8ToRgba8888(bytes, SWAP));
  ctx.putImageData(img, 0, 0);
  return cnv.toDataURL();
}

// Rasterise un drawable (ImageBitmap/canvas) a w×h ETIRE (deformation assumee) -> { key, bytes }. Cache.
export function renderToAsset(drawable, w, h) {
  const cnv = document.createElement('canvas'); cnv.width = w; cnv.height = h;
  const ctx = cnv.getContext('2d');
  ctx.clearRect(0, 0, w, h);
  ctx.drawImage(drawable, 0, 0, w, h);
  const rgba = ctx.getImageData(0, 0, w, h).data;
  const bytes = rgba8888ToRgb565a8(rgba, SWAP);
  const key = fnv1a64Hex(bytes);
  _cache.set(key, { bytes, url: buildUrl(bytes, w, h) });
  return { key, bytes };
}

// Fichier choisi pour un composant -> { key, bytes }. Memorise la source (pour re-render au resize).
export async function imageFileToAsset(file, compId, w, h) {
  const bmp = await createImageBitmap(file);
  _sources.set(compId, bmp);
  return renderToAsset(bmp, w, h);
}

// Rehydrate depuis le device : octets RGB565A8 + dims -> cache + source de repli (canvas redessinable,
// qualite degradee si un resize survient ensuite, faute de l'original).
export function rehydrate(key, compId, bytes, w, h) {
  const cnv = document.createElement('canvas'); cnv.width = w; cnv.height = h;
  const ctx = cnv.getContext('2d');
  const img = ctx.createImageData(w, h);
  img.data.set(rgb565a8ToRgba8888(bytes, SWAP));
  ctx.putImageData(img, 0, 0);
  _cache.set(key, { bytes, url: cnv.toDataURL() });
  _sources.set(compId, cnv);
}
```

- [ ] **Step 4: Lancer le test pour vérifier qu'il passe**

Run: `node --test tests/image-asset.test.js`
Expected: PASS (4 tests). Puis `node --test` (suite complète) → 161 pass / 0 fail.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/image-asset.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/tests/image-asset.test.js
git commit -m "feat(Rich_Telemetry designer): module image-asset (conversion RGB565A8 alpha)"
```

---

## Task 2: Contrat + registre + rendu de l'aperçu

**Files:**
- Modify: `schema/layout.schema.json` (oneOf ~ligne 90 ; `$defs` après `comp_meter` ~ligne 205)
- Modify: `designer/js/registry.js` (avant la fermeture de `COMPONENTS`, ~ligne 105)
- Modify: `designer/js/render.js` (import en tête ; `buildImage` en fin de fichier ~ligne 316)
- Modify: `designer/style.css` (append)
- Test: `designer/tests/registry.test.js` (parité, déjà présent) + `designer/tests/schema.test.js` (nouveaux cas)

- [ ] **Step 1: Écrire les cas de schéma qui échouent**

Dans `designer/tests/schema.test.js`, ajouter à la fin :

```javascript
test('schema : composant image valide (src/w/h)', () => {
  const l = base();
  l.components.logo = { type: 'image', src: 'deadbeef', w: 120, h: 80 };
  l.pages[0].place.push({ ref: 'logo', anchor: 'TOP_LEFT' });
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : composant image — propriété inconnue rejetée', () => {
  const l = base();
  l.components.logo = { type: 'image', src: 'deadbeef', zoom: 2 };
  l.pages[0].place.push({ ref: 'logo', anchor: 'CENTER' });
  assert.equal(validate(l).valid, false);
});
```

- [ ] **Step 2: Lancer pour vérifier l'échec**

Run: `node --test tests/schema.test.js tests/registry.test.js`
Expected: FAIL — `schema : composant image valide` échoue (`image` inconnu : `oneOf` ne matche pas) ET `registry : le registre couvre exactement les types du schema` reste vert pour l'instant (ni l'un ni l'autre n'a `image`).

- [ ] **Step 3a: Ajouter `comp_image` au schéma**

Dans `schema/layout.schema.json`, ajouter la référence dans `component.oneOf` (après `comp_meter`) :

```json
        { "$ref": "#/$defs/comp_meter" },
        { "$ref": "#/$defs/comp_image" }
```

Puis ajouter la définition dans `$defs` (après le bloc `comp_meter`, avant `page`) :

```json
    "comp_image": {
      "type": "object",
      "additionalProperties": false,
      "required": ["type"],
      "description": "Image bitmap statique RGB565A8 (alpha). Rasterisee par le navigateur a w×h exacts (etirement libre), uploadee via POST /image?key=. La taille vit sur le composant (couplee a l'asset : LVGL 8 ne sait pas etirer un lv_img non-uniformement). Le placement ne porte que la position.",
      "properties": {
        "type": { "const": "image" },
        "src": { "$ref": "#/$defs/ascii", "description": "Cle d'asset (hash FNV-1a du contenu RGB565A8). Absente tant qu'aucune image n'est choisie." },
        "w": { "type": "integer", "minimum": 1, "maximum": 360, "description": "Largeur en pixels (= largeur de l'asset). Defaut 120." },
        "h": { "type": "integer", "minimum": 1, "maximum": 360, "description": "Hauteur en pixels (= hauteur de l'asset). Defaut 120." }
      }
    },
```

- [ ] **Step 3b: Ajouter l'entrée `image` au registre**

Dans `designer/js/registry.js`, d'abord étendre l'import des builders (ligne 7) :

```javascript
import { buildLabel, buildReadout, buildBar, buildRing, buildChart, buildMeter, buildImage } from './render.js';
```

Puis ajouter dans `COMPONENTS` après l'entrée `meter` (avant `led_ring`) :

```javascript
  image: {
    label: 'Image',
    defaults: () => ({ type: 'image', w: 120, h: 120 }),
    makePlacement: screenPlacement,
    centered: false, physical: false,
    compFields: [['src', 'Image', 'image']],
    placeFields: [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
    mockFields: [],
    build: (comp) => buildImage(comp),
  },
```

- [ ] **Step 3c: Ajouter `buildImage` + import dans render.js**

En tête de `designer/js/render.js` (juste après la ligne 3 de commentaire, avant `export const MOCKS`), ajouter :

```javascript
import { previewUrl } from './image-asset.js';
```

En fin de `designer/js/render.js` (après `buildMeter`, à la fin du fichier) :

```javascript

// Image placee : bitmap statique a w×h (taille sur le composant). Apercu depuis le cache image-asset
// (previewUrl) ; placeholder borde tant qu'aucune image n'est choisie ou que le cache n'a pas d'octets
// (post-reload avant « Charger »). Le firmware rend un lv_img RGB565A8 (cf. view.cpp build_image).
export function buildImage(comp) {
  const wrap = document.createElement('div');
  wrap.className = 'w w-image';
  wrap.style.width  = (comp.w || 120) + 'px';
  wrap.style.height = (comp.h || 120) + 'px';
  const url = comp.src ? previewUrl(comp.src) : null;
  if (url) {
    const img = document.createElement('img');
    img.className = 'w-image-img';
    img.src = url;
    img.style.width = '100%'; img.style.height = '100%';
    img.style.display = 'block'; img.style.objectFit = 'fill';   // etirement libre = deformation assumee
    wrap.appendChild(img);
  } else {
    wrap.classList.add('w-image--empty');
  }
  return wrap;
}
```

- [ ] **Step 3d: Styles de l'aperçu**

Ajouter à la fin de `designer/style.css` :

```css
/* Composant image : conteneur a la taille du composant (w×h) ; placeholder si pas d'asset. */
.w-image { overflow: hidden; }
.w-image--empty { border: 1px dashed #4B5563; box-sizing: border-box; }
```

- [ ] **Step 4: Lancer la suite complète**

Run: `node --test`
Expected: PASS — 163 pass / 0 fail (157 baseline + 4 image-asset + 2 schema). En particulier `registry : le registre couvre exactement les types du schema` reste vert (`image` ajouté des deux côtés) et `chaque entrée a les clés requises` couvre la nouvelle entrée.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json \
        devices/guition_knob/projects/Rich_Telemetry/designer/js/registry.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/js/render.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/style.css \
        devices/guition_knob/projects/Rich_Telemetry/designer/tests/schema.test.js
git commit -m "feat(Rich_Telemetry designer): type image (schema + registre + apercu)"
```

---

## Task 3: Champ image dans l'inspecteur (file picker)

**Files:**
- Modify: `designer/js/inspector.js` (import ligne 5 ; helper + branche `kind === 'image'` dans la boucle `compFields` ligne 209)

Vérification : navigateur (ouvrir le designer, sélectionner une image, choisir un fichier → l'aperçu apparaît). Pas de test node (DOM/canvas).

- [ ] **Step 1: Ajouter l'import**

Dans `designer/js/inspector.js`, après la ligne 5 (`import { imageFileToBg, previewUrl } from './bg-image.js';`), ajouter :

```javascript
import { imageFileToAsset, previewUrl as imagePreviewUrl } from './image-asset.js';
```

- [ ] **Step 2: Ajouter le helper `imageField` (dans `createInspector`, près de `renderExtras`)**

Dans `designer/js/inspector.js`, à l'intérieur de `createInspector`, ajouter cette fonction juste avant `function render() {` (ligne 192) :

```javascript
  // Champ « Image » d'un composant image : file picker + miniature + reset. Convertit au navigateur a
  // la taille COURANTE du composant (c.w×c.h) et committe la cle dans `src` ; la source est memorisee
  // (image-asset) pour permettre le re-render au resize (cf. canvas.addImageHandles).
  function imageField(label, c) {
    const row = document.createElement('div'); row.className = 'insp-row';
    const span = document.createElement('span'); span.className = 'insp-label'; span.textContent = label;
    row.appendChild(span);
    const file = document.createElement('input');
    file.type = 'file'; file.accept = 'image/*'; file.className = 'insp-bg-file';
    file.addEventListener('change', async () => {
      const f = file.files?.[0]; if (!f) return;
      try {
        const { key } = await imageFileToAsset(f, sel.ref, c.w || 120, c.h || 120);
        model.commit(st => setComponentProp(st, sel.ref, 'src', key));
      } catch (e) { console.error('image:', e); }
      file.value = '';
    });
    row.appendChild(file);
    if (c.src) {
      const thumb = document.createElement('img'); thumb.className = 'insp-bg-thumb';
      const u = imagePreviewUrl(c.src);
      if (u) thumb.src = u; else thumb.alt = '(recharger depuis le device)';
      row.appendChild(thumb);
      const del = document.createElement('button');
      del.type = 'button'; del.className = 'insp-bg-reset'; del.textContent = '↺';
      del.title = "Retirer l'image";
      del.addEventListener('click', () => model.commit(st => setComponentProp(st, sel.ref, 'src', null)));
      row.appendChild(del);
    }
    return row;
  }
```

- [ ] **Step 3: Brancher le type `image` dans la boucle `compFields`**

Dans `designer/js/inspector.js`, fonction `render()`, remplacer la boucle (ligne 209-214) :

```javascript
    const rows = {};
    for (const [key, label, kind, enableWhen] of COMPONENTS[c.type].compFields) {
      const input = makeInput(kind, c[key], v => model.commit(s => setComponentProp(s, sel.ref, key, v)));
      const row = fieldRow(label, input, { ascii: kind === 'asciitext' });
      rows[key] = { input, row, enableWhen };
      body.appendChild(row);
    }
```

par :

```javascript
    const rows = {};
    for (const [key, label, kind, enableWhen] of COMPONENTS[c.type].compFields) {
      if (kind === 'image') { body.appendChild(imageField(label, c)); continue; }   // picker bespoke
      const input = makeInput(kind, c[key], v => model.commit(s => setComponentProp(s, sel.ref, key, v)));
      const row = fieldRow(label, input, { ascii: kind === 'asciitext' });
      rows[key] = { input, row, enableWhen };
      body.appendChild(row);
    }
```

- [ ] **Step 4: Vérifier (navigateur) + non-régression node**

Run: `node --test`
Expected: PASS — 163 / 0 (aucun test ne touche l'inspecteur, mais on garantit l'absence de régression d'import).
Vérif manuelle (sera faite en Task 10) : palette → ajouter Image → inspecteur affiche « Image » + file input.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/inspector.js
git commit -m "feat(Rich_Telemetry designer): champ image (file picker) dans l'inspecteur"
```

---

## Task 4: Poignée de resize image (canvas)

**Files:**
- Modify: `designer/js/canvas.js` (import ligne 13 ; dispatch `applySelection` ligne 125-126 ; `addImageHandles` après `addBarHandles` ligne 200)

Vérification : navigateur (resize d'une image → re-rendu net + nouvelle clé). Pas de test node.

- [ ] **Step 1: Ajouter l'import**

Dans `designer/js/canvas.js`, après la ligne 13 (`import { previewUrl } from './bg-image.js';`), ajouter :

```javascript
import { sourceFor, renderToAsset } from './image-asset.js';
```

- [ ] **Step 2: Brancher le dispatch dans `applySelection`**

Dans `designer/js/canvas.js`, fonction `applySelection`, après la ligne 126 (`if (comp.type === 'ring') addRingHandles(node, selected, comp, pl);`), ajouter :

```javascript
    if (comp.type === 'image') addImageHandles(node, selected, pl, comp);
```

- [ ] **Step 3: Ajouter `addImageHandles`**

Dans `designer/js/canvas.js`, juste après la fonction `addBarHandles` (fin ligne 200), ajouter :

```javascript
  // --- Resize image : poignee bas-droite -> component.w/h (la taille vit sur le composant). Au drop,
  // re-rasterise la source a la nouvelle taille (etirement libre) -> nouvelle cle `src`, gardant l'asset
  // coherent avec w×h. Sans source memorisee (ex. asset jamais charge), on met juste a jour w/h.
  function addImageHandles(node, i, pl, comp) {
    const h = document.createElement('div');
    h.className = 'handle handle-br';
    node.appendChild(h);
    h.addEventListener('pointerdown', e => {
      e.stopPropagation(); e.preventDefault();
      const s = zoomScale();
      const startW = comp.w || 120, startH = comp.h || 120;
      const sx = e.clientX, sy = e.clientY;
      h.setPointerCapture(e.pointerId);
      let dim = null;
      const move = ev => {
        dim = resizeBox(startW, startH, (ev.clientX - sx) / s, (ev.clientY - sy) / s, 8);
        node.style.width = dim.width + 'px'; node.style.height = dim.height + 'px';
        const img = node.querySelector('img'); if (img) { img.style.width = '100%'; img.style.height = '100%'; }
      };
      const up = () => {
        h.releasePointerCapture(e.pointerId);
        h.removeEventListener('pointermove', move); h.removeEventListener('pointerup', up);
        if (!dim) return;
        const src = sourceFor(pl.ref);
        const key = src ? renderToAsset(src, dim.width, dim.height).key : null;
        model.commit(st => {
          const c = st.components[pl.ref];
          c.w = dim.width; c.h = dim.height;
          if (key) c.src = key;   // garde src <-> w×h coherent ; sans source on ne touche pas src
        });
      };
      h.addEventListener('pointermove', move); h.addEventListener('pointerup', up);
    });
  }
```

- [ ] **Step 4: Non-régression node**

Run: `node --test`
Expected: PASS — 163 / 0.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/canvas.js
git commit -m "feat(Rich_Telemetry designer): resize image (re-render au drop, taille sur le composant)"
```

---

## Task 5: Endpoints `/image` + boucles Load/Push

**Files:**
- Modify: `designer/js/device.js` (après `fetchBgImage`, ligne 69)
- Modify: `designer/js/app.js` (imports lignes 4-5 ; boucle Load après ligne 150 ; boucle Push après ligne 165)

Vérification : navigateur (Charger/Pousser un layout avec image). Pas de test node (fetch/DOM).

- [ ] **Step 1: Ajouter `uploadImage` / `fetchImage` dans device.js**

À la fin de `designer/js/device.js` (après `fetchBgImage`, ligne 69) :

```javascript

// POST /image?key=<hex> : upload d'une image placee RGB565A8 (multipart, streame en LittleFS).
export async function uploadImage(base, key, bytes) {
  const fd = new FormData();
  fd.append('img', new Blob([bytes], { type: 'application/octet-stream' }), key + '.565a');
  const r = await fetch(clean(base) + '/image?key=' + encodeURIComponent(key), { method: 'POST', body: fd });
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json().catch(() => ({}));
}

// GET /image?key=<hex> : recupere les octets RGB565A8 (Uint8Array), ou null si 404.
export async function fetchImage(base, key) {
  const r = await fetch(clean(base) + '/image?key=' + encodeURIComponent(key));
  if (r.status === 404) return null;
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return new Uint8Array(await r.arrayBuffer());
}
```

- [ ] **Step 2: Étendre les imports d'app.js**

Dans `designer/js/app.js`, remplacer la ligne 4 :

```javascript
import { loadLayout, pushLayout, captureScreenshot, getStatus, setDevicePage, pushValues, uploadBgImage, fetchBgImage } from './device.js';
```

par :

```javascript
import { loadLayout, pushLayout, captureScreenshot, getStatus, setDevicePage, pushValues, uploadBgImage, fetchBgImage, uploadImage, fetchImage } from './device.js';
```

Et ajouter après la ligne 5 (`import { referencedKeys, cacheBytes, cachePut, previewUrl } from './bg-image.js';`) :

```javascript
import { referencedImageKeys, cacheBytes as imageCacheBytes, previewUrl as imagePreviewUrl, rehydrate as rehydrateImage } from './image-asset.js';
```

- [ ] **Step 3: Réhydrater les images au Load**

Dans `designer/js/app.js`, fonction `$('load').onclick`, après la boucle des fonds (ligne 150, juste avant `setStatus('Chargé', 'ok');`), ajouter :

```javascript
      for (const [id, ic] of Object.entries(model.state.components || {})) {
        if (ic.type !== 'image' || !ic.src || imagePreviewUrl(ic.src)) continue;
        const b = await fetchImage(base, ic.src);
        if (b) rehydrateImage(ic.src, id, b, ic.w || 0, ic.h || 0);
      }
```

- [ ] **Step 4: Uploader les images au Push**

Dans `designer/js/app.js`, fonction `$('push').onclick`, après la boucle des fonds (ligne 165, juste avant `await pushLayout(base, model.toJSON());`), ajouter :

```javascript
      for (const k of referencedImageKeys(model.state)) {
        const bytes = imageCacheBytes(k);
        if (bytes) await uploadImage(base, k, bytes);   // avant pushLayout (le sweep tourne au POST /layout)
      }
```

- [ ] **Step 5: Non-régression node + vérif navigateur (Task 10)**

Run: `node --test`
Expected: PASS — 163 / 0.

- [ ] **Step 6: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/designer/js/device.js \
        devices/guition_knob/projects/Rich_Telemetry/designer/js/app.js
git commit -m "feat(Rich_Telemetry designer): endpoints /image + upload/rehydrate au Push/Load"
```

---

## Task 6: Firmware — contrat (config + struct + parse)

**Files:**
- Modify: `src/config.h` (après ligne 31, le bloc `BG_*`)
- Modify: `src/dashboard.h` (enum ligne 7 ; struct `Component` après `chart_points` ligne 34)
- Modify: `src/dashboard.cpp` (`COMP_NAMES` ligne 30 ; parse après ligne 93 ; `apply_image` après `apply_meter` ligne 220 ; vtable `APPLY` ligne 231)
- Modify: `src/view.cpp` (stub vtable `VIEW` ligne ~276 — pour garder ce commit compilable ; rempli en Task 7)

Vérification : compilation. **Note :** bumper `COMP_COUNT` casse les deux `static_assert == COMP_COUNT` (APPLY *et* VIEW). On ajoute donc dès cette tâche un stub `VIEW` pour `COMP_IMAGE` ; Task 7 le remplacera par les vrais builders.

- [ ] **Step 1: Constantes config.h**

Dans `src/config.h`, après la ligne `#define BG_DIR "/bg"` (ligne 31), ajouter :

```cpp
#define IMG_MAX_W      360                                   // image placee : ne depasse pas l'ecran
#define IMG_MAX_H      360
#define IMG_PX_BYTES   3                                     // RGB565A8 = 2 octets couleur + 1 alpha
#define IMG_MAX_BYTES  (IMG_MAX_W * IMG_MAX_H * IMG_PX_BYTES) // 388800
#define IMG_DIR        "/img"                                // repertoire LittleFS des images placees
```

- [ ] **Step 2: Enum + champs struct dans dashboard.h**

Dans `src/dashboard.h`, ligne 7, ajouter `COMP_IMAGE` avant `COMP_COUNT` :

```cpp
enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND, COMP_CHART, COMP_METER, COMP_IMAGE, COMP_COUNT };
```

Dans la struct `Component`, après la ligne `int chart_points; ...` (ligne 34), ajouter :

```cpp
    char     image_src[ID_LEN];      // image : cle d'asset (/img/<src>.565a) ; vide = pas d'image
    int      image_w, image_h;       // image : dimensions de l'asset RGB565A8 (octets attendus = w*h*3)
```

- [ ] **Step 3: COMP_NAMES + parse dans dashboard.cpp**

Dans `src/dashboard.cpp`, tableau `COMP_NAMES` (ligne 27-31), ajouter l'entrée `image` :

```cpp
static const struct { const char* name; CompType type; } COMP_NAMES[] = {
    { "label",    COMP_LABEL    }, { "readout",  COMP_READOUT  }, { "bar",   COMP_BAR   },
    { "ring",     COMP_RING     }, { "led_ring", COMP_LED_RING }, { "sound", COMP_SOUND },
    { "chart",    COMP_CHART    }, { "meter",    COMP_METER    }, { "image", COMP_IMAGE },
};
```

Dans la boucle de parse des composants, juste après le bloc `c.chart_points = …` / clamp (ligne ~93, avant la lecture des `thresholds`), ajouter :

```cpp
        const char* isrc = o["src"] | "";
        strlcpy(c.image_src, bg_key_valid(isrc) ? isrc : "", sizeof(c.image_src));
        c.image_w = o["w"] | 0;
        c.image_h = o["h"] | 0;
```

- [ ] **Step 4: apply_image + vtable APPLY**

Dans `src/dashboard.cpp`, après `apply_meter` (ligne 220), ajouter :

```cpp
static void apply_image(Component&, JsonVariantConst) {
    // Image statique : pas de /update en v1 (asset GET-only). Entree de vtable requise.
}
```

Dans le tableau `APPLY` (ligne 222-231), ajouter la ligne `COMP_IMAGE` avant `};` :

```cpp
    /* COMP_METER    */ apply_meter,
    /* COMP_IMAGE    */ apply_image,
};
```

- [ ] **Step 5: Stub VIEW (garde view.cpp compilable)**

Dans `src/view.cpp`, tableau `VIEW` (ligne 267-276), ajouter un stub `COMP_IMAGE` avant `};` (remplacé par les vrais builders en Task 7) :

```cpp
    /* COMP_METER    */ { build_meter, sync_meter },
    /* COMP_IMAGE    */ { nullptr,     nullptr     },
};
```

- [ ] **Step 6: Compiler**

Run (depuis la racine repo) : `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS. Les deux `static_assert == COMP_COUNT` (APPLY + VIEW) passent. Le composant image parse mais ne rend rien encore (stub VIEW) — comblé en Task 7.

- [ ] **Step 7: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/config.h \
        devices/guition_knob/projects/Rich_Telemetry/src/dashboard.h \
        devices/guition_knob/projects/Rich_Telemetry/src/dashboard.cpp \
        devices/guition_knob/projects/Rich_Telemetry/src/view.cpp
git commit -m "feat(Rich_Telemetry firmware): contrat image (config + struct + parse src/w/h)"
```

---

## Task 7: Firmware — rendu LVGL (`build_image` + chargement PSRAM)

**Files:**
- Modify: `src/view.cpp` (buffers après ligne 20 ; `build_image` après `build_meter` ~ligne 257 ; `img_load_component` après `bg_load_page` ~ligne 402 ; vtable `VIEW` ~ligne 276 ; `view_rebuild` : libération ~ligne 410 et chargement ~ligne 438)

Vérification : compilation + (on-device en Task 10).

- [ ] **Step 1: Buffers PSRAM par composant**

Dans `src/view.cpp`, après la ligne 20 (`static lv_img_dsc_t s_bg_dsc[MAX_PAGES];`), ajouter :

```cpp
// Images placees : RGB565A8 en PSRAM, indexees par composant (un component partage = un seul buffer).
static uint8_t*     s_img_buf[MAX_COMPONENTS] = {0};
static lv_img_dsc_t s_img_dsc[MAX_COMPONENTS];
```

- [ ] **Step 2: `build_image` + `img_load_component`**

Dans `src/view.cpp`, après `build_meter`/`sync_meter` (ligne ~257), ajouter :

```cpp
static void build_image(lv_obj_t* parent, Component& c, Placement& q,
                        lv_obj_t** main, lv_obj_t**, lv_obj_t**) {
    lv_obj_t* img = lv_img_create(parent);
    int idx = q.comp_index;
    if (idx >= 0 && idx < MAX_COMPONENTS && s_img_buf[idx]) {
        lv_img_set_src(img, &s_img_dsc[idx]);     // lv_img dimensionne via header.w/h
    } else {
        // Asset non charge : placeholder borde a w×h (ou 120 par defaut).
        lv_obj_set_size(img, c.image_w > 0 ? c.image_w : 120, c.image_h > 0 ? c.image_h : 120);
        lv_obj_set_style_border_width(img, 1, 0);
        lv_obj_set_style_border_color(img, lv_color_hex(0x4B5563), 0);
        lv_obj_set_style_border_opa(img, LV_OPA_COVER, 0);
    }
    lv_obj_align(img, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = img;
}
```

Dans `src/view.cpp`, après `bg_load_page` (ligne ~402), ajouter :

```cpp
// Charge /img/<src>.565a en PSRAM pour un composant image (RGB565A8, w×h lus du composant).
// Idempotent : un component partage sur plusieurs pages n'est charge qu'une fois. false si invalide.
static bool img_load_component(Dashboard* d, int idx) {
    if (idx < 0 || idx >= d->comp_count || idx >= MAX_COMPONENTS) return false;
    Component& c = d->components[idx];
    if (!c.image_src[0] || c.image_w <= 0 || c.image_h <= 0) return false;
    if (s_img_buf[idx]) return true;                      // deja charge
    size_t need = (size_t)c.image_w * c.image_h * IMG_PX_BYTES;
    if (need == 0 || need > (size_t)IMG_MAX_BYTES) return false;
    char path[40];
    snprintf(path, sizeof(path), "%s/%s.565a", IMG_DIR, c.image_src);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if ((size_t)f.size() != need) { f.close(); return false; }
    uint8_t* buf = (uint8_t*)heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
    if (!buf) { f.close(); return false; }
    size_t rd = f.read(buf, need);
    f.close();
    if (rd != need) { heap_caps_free(buf); return false; }
    s_img_buf[idx] = buf;
    lv_img_dsc_t& dsc = s_img_dsc[idx];
    memset(&dsc, 0, sizeof(dsc));
    dsc.header.always_zero = 0;
    dsc.header.cf  = LV_IMG_CF_TRUE_COLOR_ALPHA;
    dsc.header.w   = c.image_w;
    dsc.header.h   = c.image_h;
    dsc.data       = buf;
    dsc.data_size  = need;
    return true;
}
```

- [ ] **Step 3: Entrée vtable VIEW (remplace le stub de Task 6)**

Dans `src/view.cpp`, tableau `VIEW`, remplacer la ligne stub `/* COMP_IMAGE */ { nullptr, nullptr }` (ajoutée en Task 6) par les vrais builders :

```cpp
    /* COMP_METER    */ { build_meter, sync_meter },
    /* COMP_IMAGE    */ { build_image, nullptr    },
};
```

- [ ] **Step 4: Libération + chargement dans `view_rebuild`**

Dans `src/view.cpp`, fonction `view_rebuild`, après la boucle de libération `s_bg_buf` (ligne ~410), ajouter :

```cpp
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (s_img_buf[i]) { heap_caps_free(s_img_buf[i]); s_img_buf[i] = nullptr; }
    }
```

Dans la même fonction, dans la boucle des placements, juste avant l'appel `VIEW[c.type].build(...)` (ligne ~438), ajouter le préchargement :

```cpp
            if (c.type == COMP_IMAGE) img_load_component(d, q.comp_index);
```

(c.-à-d. après `Component& c = d->components[q.comp_index];` et avant le `if ((unsigned)c.type < COMP_COUNT && VIEW[c.type].build)`.)

- [ ] **Step 5: Compiler**

Run : `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS. Les deux `static_assert == COMP_COUNT` (APPLY + VIEW) passent.

- [ ] **Step 6: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/view.cpp
git commit -m "feat(Rich_Telemetry firmware): rendu lv_img RGB565A8 (build_image + chargement PSRAM)"
```

---

## Task 8: Firmware — endpoints `/image` + sweep des orphelins

**Files:**
- Modify: `src/api.cpp` (handlers après `h_bgimage_get` ligne ~259 ; sweep après le sweep `/bg` ligne ~117 ; routes ligne ~273)

Vérification : compilation + (on-device en Task 10).

- [ ] **Step 1: Handlers `/image`**

Dans `src/api.cpp`, après `h_bgimage_get` (ligne ~259), ajouter :

```cpp
// --- POST /image?key=<hex> : upload d'une image placee RGB565A8 (taille variable) ---
static File   s_img_up;
static size_t s_img_written = 0;
static const char* IMG_TMP = IMG_DIR "/_upload.tmp";

static void h_image_upload() {
    HTTPUpload& up = S->upload();
    if (up.status == UPLOAD_FILE_START) {
        if (!LittleFS.exists(IMG_DIR)) LittleFS.mkdir(IMG_DIR);
        s_img_written = 0;
        s_img_up = LittleFS.open(IMG_TMP, "w");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_img_up) s_img_written += s_img_up.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_img_up) s_img_up.close();
    }
}

static void h_image_done() {
    String key = S->arg("key");
    // Taille variable : on borne (≤ plein ecran) et on exige un multiple de 3 (RGB565A8). La validation
    // forte len == w*h*3 a lieu au chargement (img_load_component, ou w/h sont connus).
    if (s_img_written == 0 || s_img_written > (size_t)IMG_MAX_BYTES || (s_img_written % IMG_PX_BYTES) != 0) {
        LittleFS.remove(IMG_TMP);
        S->send(400, "text/plain", "bad size\n"); return;
    }
    if (!bg_key_valid(key.c_str())) {
        LittleFS.remove(IMG_TMP);
        S->send(400, "text/plain", "bad key\n"); return;
    }
    String dst = String(IMG_DIR) + "/" + key + ".565a";
    LittleFS.remove(dst);
    if (!LittleFS.rename(IMG_TMP, dst)) {
        LittleFS.remove(IMG_TMP);
        S->send(500, "text/plain", "FS rename failed\n"); return;
    }
    S->send(200, "application/json", "{\"ok\":true}\n");
}

static void h_image_get() {
    String key = S->arg("key");
    if (!bg_key_valid(key.c_str())) { S->send(400, "text/plain", "bad key\n"); return; }
    String path = String(IMG_DIR) + "/" + key + ".565a";
    File f = LittleFS.open(path, "r");
    if (!f) { S->send(404, "text/plain", "not found\n"); return; }
    S->streamFile(f, "application/octet-stream");
    f.close();
}
```

- [ ] **Step 2: Sweep des images orphelines**

Dans `src/api.cpp`, dans `h_set_layout`, juste après le bloc de sweep des fonds `/bg` (qui se termine ligne ~117 par `for (int i = 0; i < nv; i++) LittleFS.remove(victims[i]);` puis `}`), ajouter un bloc symétrique :

```cpp
    // Sweep : supprime les images /img/*.565a que plus aucun composant image ne reference.
    {
        String victims[16]; int nv = 0;
        File dir = LittleFS.open(IMG_DIR);
        if (dir && dir.isDirectory()) {
            for (File e = dir.openNextFile(); e && nv < 16; e = dir.openNextFile()) {
                String full = e.name();
                e.close();
                int slash = full.lastIndexOf('/');
                String b = (slash >= 0) ? full.substring(slash + 1) : full;
                if (!b.endsWith(".565a")) continue;           // ignore _upload.tmp
                String key = b.substring(0, b.length() - 5);  // ".565a" = 5 caracteres
                bool referenced = false;
                for (int c = 0; c < D->comp_count; c++)
                    if (D->components[c].type == COMP_IMAGE && key.length() &&
                        strcmp(D->components[c].image_src, key.c_str()) == 0) { referenced = true; break; }
                if (!referenced) victims[nv++] = String(IMG_DIR) + "/" + b;
            }
            dir.close();
        }
        for (int i = 0; i < nv; i++) LittleFS.remove(victims[i]);
    }
```

- [ ] **Step 3: Routes**

Dans `src/api.cpp`, `api_register`, après les deux lignes `server.on("/bgimage", …)` (ligne ~273), ajouter :

```cpp
    server.on("/image", HTTP_POST, h_image_done, h_image_upload);
    server.on("/image", HTTP_GET,  h_image_get);
```

- [ ] **Step 4: Compiler**

Run : `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS.

- [ ] **Step 5: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/api.cpp
git commit -m "feat(Rich_Telemetry firmware): endpoints /image (upload/get) + sweep orphelins"
```

---

## Task 9: Manuel HTML (`docs/index.html`)

**Files:**
- Modify: `devices/guition_knob/projects/Rich_Telemetry/docs/index.html`

- [ ] **Step 1: Repérer le motif d'un composant existant**

Ouvrir `docs/index.html`, localiser la section décrivant les composants (chercher le bloc du composant `bar` ou `chart`). Noter le balisage exact (titre, paragraphe, éventuelle table de propriétés).

- [ ] **Step 2: Ajouter la section image (même balisage)**

Dupliquer le bloc d'un composant et le remplir avec ce contenu (adapter les balises au motif local) :

> **Image** — bitmap statique placé sur une page, redimensionnable librement (l'image peut être déformée). Choisir un fichier dans l'inspecteur : le navigateur le convertit en RGB565A8 (transparence des PNG conservée) à la taille du composant et l'envoie au device au « Pousser ». Propriétés : `src` (clé d'asset, posée automatiquement au choix du fichier), `w`/`h` (taille en pixels, ajustées par les poignées de redimensionnement). La taille est portée par le composant : la même image placée sur plusieurs pages s'affiche à une taille unique. Plafond : 360×360 px. Après rechargement du designer depuis le device, un redimensionnement repart de la version déjà rastérisée (qualité réduite) — re-choisir le fichier pour la pleine qualité.

- [ ] **Step 3: Vérifier le rendu HTML**

Ouvrir `docs/index.html` au navigateur ; confirmer que la section image s'affiche sans casser la mise en page (TOC/sidebar inclus si présents).

- [ ] **Step 4: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/docs/index.html
git commit -m "docs(Rich_Telemetry): manuel — section du composant image"
```

---

## Task 10: Staging, build complet et validation on-device

**Files:** aucun (orchestration + validation).

- [ ] **Step 1: Suite designer complète**

Run: `cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test`
Expected: PASS — 163 / 0.

- [ ] **Step 2: Stager le designer vers LittleFS**

Run: `devices/guition_knob/projects/Rich_Telemetry/tools/stage_fs.sh`
Expected: « Staged → data/designer … data/schema » (le nouveau `image-asset.js` est copié via `designer/js/*.js`).

- [ ] **Step 3: Build firmware**

Run (racine repo) : `./build.sh guition_knob Rich_Telemetry`
Expected: SUCCESS (compilation complète).

- [ ] **Step 4: Flash firmware + LittleFS**

Le device Guition est branché au Mac (IP `192.168.1.35`). Flasher (l'utilisateur peut lancer ces commandes via le préfixe `!` si une auth interactive est requise) :

```bash
./build.sh auto Rich_Telemetry --upload      # firmware (auto = identifie le device par son MAC)
./build.sh auto Rich_Telemetry --uploadfs    # LittleFS (designer embarqué + schema)
```

Expected: upload OK (cf. `tools/device_mac.py` valide l'identité). Si « No serial data received », re-tenter (contact USB partiel — cf. CLAUDE.md Guition).

- [ ] **Step 5: Validation visuelle (transparence sur fond coloré)**

Via le designer embarqué `http://192.168.1.35/designer/` (ou le designer local pointé sur cette IP) :
1. Mettre un fond de page coloré (ex. `#2222AA`).
2. Ajouter un composant **Image**, choisir un **PNG à fond transparent** (logo).
3. Le redimensionner (poignée), puis **Pousser**.
4. Capturer : `curl -s http://192.168.1.35/screenshot -o /tmp/rt.bmp && sips -s format png /tmp/rt.bmp --out /tmp/rt.png`
5. Envoyer `/tmp/rt.png` à l'utilisateur (SendUserFile).

Critère de succès : le logo s'affiche à la taille choisie, **sans rectangle opaque** autour (la transparence laisse voir le fond coloré). Vérifier aussi un second placement / une seconde page si le même composant est réutilisé (même taille attendue).

- [ ] **Step 6: Vérifier le sweep + persistance**

1. Retirer l'image (bouton ↺) ou la remplacer, **Pousser** → `GET http://192.168.1.35/image?key=<ancienne_clé>` doit renvoyer 404 (orphelin balayé).
2. `curl -s http://192.168.1.35/layout` → confirmer que le composant image a `src`/`w`/`h` cohérents.
3. Recharger le designer (« Charger ») → l'aperçu de l'image se réhydrate (via `GET /image`).

- [ ] **Step 7: Commit final éventuel (si ajustements)**

Si la validation a nécessité des correctifs, commiter par tâche concernée. Sinon, rien à committer (les commits des tâches 1-9 couvrent tout).

---

## Notes de risque / décisions

- **Taille sur le composant** (et non le placement) : entorse assumée à la convention bar/chart/meter, justifiée par le couplage octets↔w↔h d'un raster pré-étiré (LVGL 8 ne stretch pas non-uniformément). Le resize est donc spécial-casé (écrit `component.w/h`, pas `placement.width/height`).
- **`apply_image` vide** : l'image n'est pas poussée par `/update` (statique). Entrée de vtable requise par les `static_assert == COMP_COUNT`.
- **Réhydratation lossy au resize post-reload** : sans l'original (non persisté), un resize repart de l'asset déjà rastérisé. Choix v1 délibéré (cf. spec).
- **Plafond PSRAM** : `IMG_MAX_BYTES = 388800` o/image (360×360×3). PSRAM 8 MB ⇒ large marge pour une poignée d'images ; le sweep évite l'accumulation d'orphelins.
- **Validation de taille en deux temps** : à l'upload `≤ IMG_MAX_BYTES` et multiple de 3 (w/h inconnus du handler) ; au chargement `len == w*h*3` (w/h connus du composant).
