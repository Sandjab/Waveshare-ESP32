# Rich_Telemetry Designer — Plan B : Canvas WYSIWYG Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Donner un canvas qui rend les widgets « best-effort » (label/readout/bar/ring + badges led_ring/sound) et permet de placer (drag + snap aux ancrages) et redimensionner (bar : width/height ; ring : radius/thickness/gap_deg) à la souris, le tout reflété live dans le modèle et le JSON avancé.

**Architecture:** On prolonge la séparation du Plan A : la **math pure** (redim + conscience du cercle) va dans `geometry.js`, la **math de rendu** (fraction de barre, couleur de seuil, chemin d'arc SVG, formats) dans `render.js` — toutes deux couvertes par `node --test`. Les **builders DOM** (`render.js`) et **l'interaction pointeur** (`canvas.js`) ne sont pas testables sous Node → vérifiés au navigateur. Le rendu est une **2e implémentation** du rendu firmware (`src/view.cpp` + `src/dashboard.cpp`) : double-maintenance assumée, le device arbitre.

**Tech Stack:** JavaScript (modules ES), `node --test` (zéro dépendance), SVG inline pour les arcs (zéro build), pointer events pour le drag/resize, webfont Montserrat vendorisée. Servir via `python3 -m http.server` **depuis `Rich_Telemetry/`** (le schéma vit dans `../schema`).

---

## Décisions verrouillées pour ce plan (au-delà du spec)

Le spec (`specs/2026-06-15-wysiwyg-editor-design.md`) décrit un modèle de positionnement **ancrage + offset universel**. La lecture du firmware révèle deux écarts à respecter pour rester fidèle (décision #3 du spec : *le device arbitre*) :

1. **Le ring est toujours CENTRÉ.** `src/view.cpp:51` fait `lv_obj_center(arc)` : `anchor/dx/dy` sont **ignorés** pour un `ring`. → Dans l'éditeur, un ring **n'est pas déplaçable**, seulement **redimensionnable** (radius/thickness/gap_deg). On le rend centré. *(Si on voulait quand même un ring déplaçable, ce serait une évolution firmware — hors scope ; à valider avec le user, voir note de fin.)*
2. **Rendu = DOM + SVG, échelle 1:1.** Le canvas est un carré fixe de **360 px** (1 px CSS = 1 unité écran), donc les coords pointeur se mappent directement sur les coords de `geometry.js` sans transformation. Les widgets sont des nœuds DOM positionnés en absolu ; les arcs de ring sont des `<path>` SVG. Choix : le hit-testing, la sélection et les poignées sont triviaux en DOM, les arcs triviaux en SVG, et tout est zéro-build. *(Alternative `<canvas>` 2D écartée : hit-testing et redraw manuels, plus lourds.)*
3. **Valeurs d'aperçu = mocks fixes** (`MOCKS` dans `render.js`). Plan C les rendra éditables via l'inspecteur ; en Plan B elles sont constantes (juste de quoi voir le rendu).

---

## File Structure

```
designer/
├── index.html              # MODIF : la colonne Canvas reçoit #stage (360px) + #badges
├── style.css               # MODIF : @font-face Montserrat + styles stage/widgets/poignées/badges
├── vendor/
│   ├── ajv.min.js          # (Plan A)
│   └── fonts/
│       └── montserrat-500.woff2   # NOUVEAU : webfont vendorisée (best-effort, fallback system-ui)
└── js/
    ├── geometry.js         # MODIF : + resizeBox/ringRadiusAt/ringThicknessAt/gapDegAt/cornersOutsideCircle (PUR, testé)
    ├── render.js           # NOUVEAU : math de rendu (PUR, testé) + builders DOM (vérif navigateur)
    ├── canvas.js           # NOUVEAU : page active, sélection, drag+snap (commit-on-drop), resize (vérif navigateur)
    └── app.js              # MODIF : instancie le canvas (câble model ↔ canvas)
└── tests/
    ├── geometry.test.js    # MODIF : + tests des helpers de redim/cercle
    └── render.test.js      # NOUVEAU : tests de la math de rendu
```

> Hors scope (Plan C) : `palette.js`, `inspector.js`, `pages.js`, `file-io.js`, et les mutations dédiées de `model.js`. En Plan B, l'édition se fait par drag/resize sur le canvas **et** par le JSON avancé (déjà là). Le canvas n'affiche que **la page 0** (`pages[0]`) ; le multi-pages arrive en Plan C.

---

## Task 1 : `geometry.js` — redimensionnement + conscience du cercle (TDD)

**Files:**
- Modify: `designer/js/geometry.js` (ajout en fin de fichier)
- Modify: `designer/tests/geometry.test.js` (ajout en fin de fichier)

- [ ] **Step 1 : écrire les tests qui échouent**

Ajouter à la fin de `designer/tests/geometry.test.js` :
```js
import {
  resizeBox, ringRadiusAt, ringThicknessAt, gapDegAt, cornersOutsideCircle
} from '../js/geometry.js';

test('resizeBox agrandit selon le delta pointeur', () => {
  assert.deepEqual(resizeBox(200, 16, 40, 10), { width: 240, height: 26 });
});

test('resizeBox clampe au minimum', () => {
  assert.deepEqual(resizeBox(200, 16, -1000, -1000, 8), { width: 8, height: 8 });
});

test('ringRadiusAt = distance centre→pointeur', () => {
  assert.equal(ringRadiusAt(180, 0), 180); // centre (180,180), pointeur en haut → 180
});

test('ringThicknessAt = rayon − distance centre→pointeur', () => {
  assert.equal(ringThicknessAt(180, 30, 176), 26); // dist=150, 176-150=26
});

test('gapDegAt = 0 quand le pointeur est droit en bas', () => {
  assert.equal(gapDegAt(180, 300), 0);
});

test('gapDegAt = 2× écart à la verticale basse', () => {
  assert.equal(gapDegAt(130, 230), 90); // angle 135°, |135−90|=45, ×2=90
});

test('cornersOutsideCircle : boîte centrée → dedans', () => {
  assert.equal(cornersOutsideCircle(160, 170, 40, 20), false);
});

test('cornersOutsideCircle : coin TOP_LEFT → dehors (écran rond)', () => {
  assert.equal(cornersOutsideCircle(0, 0, 40, 20), true);
});
```

- [ ] **Step 2 : lancer les tests, vérifier l'échec**

Run (depuis `designer/`) : `node --test tests/geometry.test.js`
Expected : FAIL — `resizeBox`/`ringRadiusAt`/… `is not a function` (imports non résolus).

- [ ] **Step 3 : implémenter les helpers dans `js/geometry.js`**

Ajouter à la fin de `designer/js/geometry.js` :
```js
// --- Plan B : redimensionnement + conscience de l'écran rond (net-new, consommé par canvas.js) ---

// Bar : redim depuis la poignée bas-droite. dxPx/dyPx = déplacement pointeur en px écran (1:1).
export function resizeBox(startW, startH, dxPx, dyPx, min = 8) {
  return {
    width:  Math.max(min, Math.round(startW + dxPx)),
    height: Math.max(min, Math.round(startH + dyPx))
  };
}

// Ring : rayon = distance centre→pointeur (poignée bord externe).
export function ringRadiusAt(px, py, cx = SCREEN / 2, cy = SCREEN / 2, min = 8) {
  return Math.max(min, Math.round(Math.hypot(px - cx, py - cy)));
}

// Ring : épaisseur de bande = rayon − distance centre→pointeur (poignée bord interne).
export function ringThicknessAt(px, py, radius, cx = SCREEN / 2, cy = SCREEN / 2, min = 1) {
  return Math.max(min, Math.round(radius - Math.hypot(px - cx, py - cy)));
}

// Ring : ouverture = 2×|angle(pointeur) − bas|. L'ouverture est centrée en bas (90°,
// convention écran y-vers-le-bas = convention LVGL). cf. lv_arc_set_bg_angles (view.cpp:54).
export function gapDegAt(px, py, cx = SCREEN / 2, cy = SCREEN / 2) {
  const deg = Math.atan2(py - cy, px - cx) * 180 / Math.PI; // 0=droite, 90=bas
  const fromBottom = Math.abs(deg - 90);
  return Math.max(0, Math.min(180, Math.round(2 * fromBottom)));
}

// Écran rond / parent carré : un coin de la boîte sort-il du cercle visible
// (centre SCREEN/2, rayon SCREEN/2) ? Rappel pédagogique (spec § « écran rond »).
export function cornersOutsideCircle(x, y, w, h, screen = SCREEN) {
  const c = screen / 2, R = screen / 2;
  const corners = [[x, y], [x + w, y], [x, y + h], [x + w, y + h]];
  return corners.some(([px, py]) => Math.hypot(px - c, py - c) > R);
}
```

- [ ] **Step 4 : lancer les tests, vérifier le succès**

Run : `node --test tests/geometry.test.js`
Expected : PASS (les anciens tests du Plan A + les 8 nouveaux).

- [ ] **Step 5 : Commit**

```bash
git add designer/js/geometry.js designer/tests/geometry.test.js
git commit -m "Rich_Telemetry: designer geometry resize + circle helpers (Plan B) + tests"
```

---

## Task 2 : `render.js` — math de rendu (TDD) + builders DOM

**Files:**
- Create: `designer/js/render.js`
- Test: `designer/tests/render.test.js`

> La **math** (Steps 1–4) est testée sous Node. Les **builders DOM** (Step 5) ne le sont pas (vérifiés au navigateur en Task 4) ; on les ajoute au même fichier mais aucune ligne de builder ne s'exécute à l'import.

- [ ] **Step 1 : écrire les tests qui échouent**

`designer/tests/render.test.js` :
```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  pickFontPx, barFill, pickThresholdColor, formatValue, formatRemaining,
  ringSweepDeg, pointOnArc, arcPath, ringPaths
} from '../js/render.js';

test('pickFontPx retombe sur les 3 tailles LVGL', () => {
  assert.equal(pickFontPx(28), 28);
  assert.equal(pickFontPx(20), 20);
  assert.equal(pickFontPx(14), 14);
  assert.equal(pickFontPx(11), 14); // toute autre valeur → 14
});

test('barFill = fraction clampée', () => {
  assert.equal(barFill(60, 0, 100), 0.6);
  assert.equal(barFill(150, 0, 100), 1);
  assert.equal(barFill(-5, 0, 100), 0);
  assert.equal(barFill(5, 0, 0), 0); // garde anti division par zéro
});

test('pickThresholdColor : 1er seuil dont value < limite, sinon base', () => {
  const th = [[20, '#FF0000'], [50, '#FFAA00']];
  assert.equal(pickThresholdColor(th, 10, '#00FF00'), '#FF0000');
  assert.equal(pickThresholdColor(th, 30, '#00FF00'), '#FFAA00');
  assert.equal(pickThresholdColor(th, 80, '#00FF00'), '#00FF00');
  assert.equal(pickThresholdColor(undefined, 80, '#00FF00'), '#00FF00');
});

test('formatValue : entier brut, sinon 1 décimale, + unité', () => {
  assert.equal(formatValue(42, '%'), '42 %');
  assert.equal(formatValue(3.14, ''), '3.1');
  assert.equal(formatValue(10, ''), '10');
});

test('formatRemaining miroir du firmware', () => {
  assert.equal(formatRemaining(45), '45s');
  assert.equal(formatRemaining(90), '1m');
  assert.equal(formatRemaining(3661), '1h01');
  assert.equal(formatRemaining(90000), '1j1h');
});

test('ringSweepDeg = fraction × (360 − gap)', () => {
  assert.equal(ringSweepDeg(50, 0, 100, 70), 145);
});

test('pointOnArc : 90° = bas (y vers le bas)', () => {
  const [x, y] = pointOnArc(180, 180, 100, 90);
  assert.ok(Math.abs(x - 180) < 1e-9);
  assert.ok(Math.abs(y - 280) < 1e-9);
});

test('arcPath : quart de cercle déterministe', () => {
  assert.equal(arcPath(0, 0, 100, 0, 90), 'M 100.00 0.00 A 100 100 0 0 1 0.00 100.00');
});

test('ringPaths expose rayon de tracé et angle de départ', () => {
  const p = ringPaths(80, 16, 70, 72, 0, 100);
  assert.equal(p.rr, 72);     // 80 − 16/2
  assert.equal(p.start, 125); // 90 + 70/2
  assert.ok(p.track.startsWith('M'));
  assert.ok(p.indicator.startsWith('M'));
});
```

- [ ] **Step 2 : lancer les tests, vérifier l'échec**

Run : `node --test tests/render.test.js`
Expected : FAIL — `Cannot find module '../js/render.js'`.

- [ ] **Step 3 : implémenter la math de rendu dans `js/render.js`**

Créer `designer/js/render.js` avec d'abord la math pure (les builders DOM viennent au Step 5) :
```js
// Rendu best-effort des widgets — 2e implémentation du rendu firmware (src/dashboard.cpp + src/view.cpp).
// ⚠ Double-maintenance assumée : tout changement de rendu firmware doit être répliqué ici. Le device arbitre.
// La math (ci-dessous) est pure et testée ; les builders DOM (plus bas) sont vérifiés au navigateur.

// Valeurs d'aperçu mock par défaut. Plan C les rendra éditables via l'inspecteur ; ici elles sont fixes.
export const MOCKS = {
  readout: { value: 42 },
  bar:     { value: 60 },
  ring:    { value: 72, reset_in_s: 18000 }
};

// Police LVGL embarquée : 14/20/28 px (pick_font, view.cpp:19). Toute autre valeur retombe sur 14.
export function pickFontPx(font) {
  if (font >= 28) return 28;
  if (font >= 20) return 20;
  return 14;
}

// bar : fraction remplie (clampée). Miroir lv_bar : (value − min) / (max − min).
export function barFill(value, min = 0, max = 100) {
  if (max === min) return 0;
  return Math.max(0, Math.min(1, (value - min) / (max - min)));
}

// ring : couleur de seuil — 1er seuil dont value < limite, sinon couleur de base. Miroir threshold_color (color.cpp:13).
export function pickThresholdColor(thresholds, value, base) {
  for (const [limit, color] of thresholds || []) {
    if (value < limit) return color;
  }
  return base;
}

// readout : "<num> <unit>". Miroir format_value (format.cpp:19) : entier brut sinon 1 décimale.
export function formatValue(v, unit) {
  const num = Number.isInteger(v) ? String(v) : v.toFixed(1);
  return unit ? `${num} ${unit}` : num;
}

// ring countdown : reste formaté. Miroir format_remaining (format.cpp:5).
export function formatRemaining(s) {
  if (s >= 86400) return `${Math.floor(s / 86400)}j${Math.floor((s % 86400) / 3600)}h`;
  if (s >= 3600)  return `${Math.floor(s / 3600)}h${String(Math.floor((s % 3600) / 60)).padStart(2, '0')}`;
  if (s >= 60)    return `${Math.floor(s / 60)}m`;
  return `${s}s`;
}

// ring : balayage (deg) de l'indicateur = fraction × (360 − gap). L'ouverture (gap) reste en bas.
export function ringSweepDeg(value, min, max, gapDeg) {
  return barFill(value, min, max) * (360 - gapDeg);
}

// Point sur un cercle, convention écran (0°=droite, 90°=bas car y vers le bas) — identique à LVGL.
export function pointOnArc(cx, cy, r, deg) {
  const rad = deg * Math.PI / 180;
  return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
}

// Chemin SVG d'un arc : centre (cx,cy), rayon r, de startDeg, balayé de sweepDeg dans le sens horaire écran.
export function arcPath(cx, cy, r, startDeg, sweepDeg) {
  const [x1, y1] = pointOnArc(cx, cy, r, startDeg);
  const [x2, y2] = pointOnArc(cx, cy, r, startDeg + sweepDeg);
  const large = sweepDeg > 180 ? 1 : 0;
  const f = n => n.toFixed(2);
  return `M ${f(x1)} ${f(y1)} A ${r} ${r} 0 ${large} 1 ${f(x2)} ${f(y2)}`;
}

// ring : chemins fond + indicateur (rayon de tracé au milieu de la bande). Centralise la géométrie
// d'arc partagée par buildRing (initial) et canvas.js paintRing (live resize). Miroir view.cpp:54.
export function ringPaths(r, th, gap, value, min, max) {
  const rr = r - th / 2;           // rayon au centre de la bande
  const start = 90 + gap / 2;      // lv_arc_set_bg_angles(arc, 90 + gap/2, 90 − gap/2)
  return {
    rr, start,
    track:     arcPath(r, r, rr, start, 360 - gap),
    indicator: arcPath(r, r, rr, start, ringSweepDeg(value, min, max, gap))
  };
}
```

- [ ] **Step 4 : lancer les tests, vérifier le succès**

Run : `node --test tests/render.test.js`
Expected : PASS (9 tests). Puis `node --test` (toute la suite) : PASS.

- [ ] **Step 5 : ajouter les builders DOM (vérifiés au navigateur en Task 4)**

Ajouter à la fin de `designer/js/render.js` :
```js
// --- Builders DOM (non testés sous Node ; vérifiés au navigateur). Aucun ne s'exécute à l'import. ---

const FONT = px => `${px}px Montserrat, system-ui, sans-serif`;
const SVGNS = 'http://www.w3.org/2000/svg';

export function buildLabel(comp) {
  const n = document.createElement('div');
  n.className = 'w w-label';
  n.style.font = FONT(pickFontPx(comp.font ?? 20));
  n.style.color = comp.color || '#FFFFFF';
  n.textContent = comp.text || 'Label';
  return n;
}

export function buildReadout(comp, mock = MOCKS.readout) {
  const n = document.createElement('div');
  n.className = 'w w-readout';
  n.style.font = FONT(pickFontPx(comp.font ?? 20));
  n.style.color = comp.color || '#FFFFFF';
  const val = formatValue(mock.value, comp.unit || '');
  n.textContent = comp.label ? `${comp.label} ${val}` : val; // miroir view.cpp:201-209
  return n;
}

export function buildBar(comp, placement, mock = MOCKS.bar) {
  const wrap = document.createElement('div');
  wrap.className = 'w w-bar';
  if (comp.label) {                          // label au-dessus (view.cpp:137-144)
    const lbl = document.createElement('div');
    lbl.className = 'w-bar-label';
    lbl.textContent = comp.label;
    wrap.appendChild(lbl);
  }
  const track = document.createElement('div');
  track.className = 'w-bar-track';
  track.style.width  = (placement.width  || 200) + 'px'; // défauts firmware (view.cpp:132)
  track.style.height = (placement.height || 16)  + 'px';
  const fill = document.createElement('div');
  fill.className = 'w-bar-fill';
  fill.style.width = (barFill(mock.value, comp.min ?? 0, comp.max ?? 100) * 100) + '%';
  fill.style.background = comp.color || '#38BDF8';
  track.appendChild(fill);
  wrap.appendChild(track);
  return wrap;
}

export function buildRing(comp, placement, mock = MOCKS.ring) {
  const r   = placement.radius    || 80;
  const th  = placement.thickness || 16;
  const gap = placement.gap_deg ?? 70;
  const size = r * 2;
  const wrap = document.createElement('div');
  wrap.className = 'w w-ring';
  wrap.style.width = size + 'px';
  wrap.style.height = size + 'px';
  const svg = document.createElementNS(SVGNS, 'svg');
  svg.setAttribute('width', size);
  svg.setAttribute('height', size);
  svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
  const { track, indicator } = ringPaths(r, th, gap, mock.value, comp.min ?? 0, comp.max ?? 100);
  const col = pickThresholdColor(comp.thresholds, mock.value, comp.color || '#38BDF8');
  const mk = (cls, d, stroke) => {
    const p = document.createElementNS(SVGNS, 'path');
    p.setAttribute('class', cls);
    p.setAttribute('d', d);
    p.setAttribute('fill', 'none');
    p.setAttribute('stroke', stroke);
    p.setAttribute('stroke-width', th);
    p.setAttribute('stroke-linecap', 'round');
    return p;
  };
  svg.appendChild(mk('ring-track', track, '#1F2937')); // fond firmware (view.cpp:58)
  svg.appendChild(mk('ring-ind', indicator, col));
  wrap.appendChild(svg);
  if (comp.pill) {                            // pastille % en haut de bande (view.cpp:66-74)
    const pill = document.createElement('div');
    pill.className = 'w-ring-pill';
    pill.textContent = `${Math.round(mock.value)}%`;
    pill.style.background = col;
    pill.style.top = th + 'px';
    wrap.appendChild(pill);
  }
  if (comp.countdown) {                       // légende dans l'ouverture du bas (view.cpp:43)
    const cap = document.createElement('div');
    cap.className = 'w-ring-cap';
    cap.textContent = formatRemaining(mock.reset_in_s);
    cap.style.color = comp.color || '#38BDF8';
    cap.style.bottom = th + 'px';
    wrap.appendChild(cap);
  }
  return wrap;
}

// led_ring / sound : physiques, pas de rendu écran → badge hors canvas (spec § rendu).
export function buildBadge(id, comp) {
  const n = document.createElement('span');
  n.className = 'badge badge-' + comp.type;
  n.textContent = (comp.type === 'led_ring' ? '◉ LED ring' : '♪ sound') + ' : ' + id;
  return n;
}
```

- [ ] **Step 6 : Commit**

```bash
git add designer/js/render.js designer/tests/render.test.js
git commit -m "Rich_Telemetry: designer render module (best-effort widgets + math) + tests"
```

---

## Task 3 : `index.html` + `style.css` — canvas, poignées, badges, webfont

**Files:**
- Modify: `designer/index.html` (colonne Canvas)
- Modify: `designer/style.css` (ajouts en fin)
- Create: `designer/vendor/fonts/montserrat-500.woff2` (téléchargé)

> Vérification visuelle seule (DOM/CSS). `canvas.js` arrive en Task 4 ; à ce stade le canvas est une zone vide stylée.

- [ ] **Step 1 : remplacer la colonne Canvas dans `index.html`**

Remplacer la ligne :
```html
    <section id="canvas-col" class="col"><h2>Canvas</h2><p class="todo">Plan B</p></section>
```
par :
```html
    <section id="canvas-col" class="col">
      <h2>Canvas</h2>
      <div id="stage" class="stage">
        <div class="screen-circle"></div>
      </div>
      <div id="badges" class="badges"></div>
    </section>
```

- [ ] **Step 2 : vendoriser Montserrat (best-effort, fallback system-ui)**

Run (depuis `designer/`) :
```bash
mkdir -p vendor/fonts
# Découvre l'URL woff2 latine courante via l'API CSS Google Fonts, puis télécharge.
url=$(curl -s -A "Mozilla/5.0" "https://fonts.googleapis.com/css2?family=Montserrat:wght@500" | grep -oE "https://[^)]+\.woff2" | head -1)
curl -L "$url" -o vendor/fonts/montserrat-500.woff2
```
Expected : `vendor/fonts/montserrat-500.woff2` créé et non vide :
```bash
test -s vendor/fonts/montserrat-500.woff2 && echo OK
```
Expected : `OK`. Si offline/bloqué (pas de `OK`), continuer quand même : le `@font-face` échoue silencieusement et l'aperçu retombe sur `system-ui` — acceptable (aperçu best-effort, le device porte la vraie police).

- [ ] **Step 3 : ajouter les styles à `style.css`**

Ajouter à la fin de `designer/style.css` :
```css
/* --- Plan B : webfont + canvas --- */
@font-face {
  font-family: 'Montserrat';
  src: url('vendor/fonts/montserrat-500.woff2') format('woff2');
  font-weight: 500;
  font-display: swap;
}

#canvas-col { display: flex; flex-direction: column; align-items: center; }
.stage {
  position: relative;
  width: 360px; height: 360px;     /* 1 px CSS = 1 unité écran (échelle 1:1) */
  border-radius: 50%;
  background: #000;
  overflow: hidden;
  flex: none;
  touch-action: none;              /* pointer events propres (pas de scroll/zoom) */
}
.screen-circle {
  position: absolute; inset: 0;
  border-radius: 50%;
  border: 1px dashed #334155;      /* matérialise la zone ronde visible */
  pointer-events: none;
}
.w { position: absolute; user-select: none; cursor: move; white-space: nowrap; }
.w.selected { outline: 1px solid var(--accent); outline-offset: 2px; }
.w.snapped { outline: 1px solid var(--ok); }
.w.outside { outline: 1px dashed var(--err); }  /* coin hors zone ronde */

.w-bar-label { font: 12px Montserrat, system-ui, sans-serif; color: #9AA0AA; margin-bottom: 4px; }
.w-bar-track { background: #1F2937; border-radius: 3px; overflow: hidden; }
.w-bar-fill { height: 100%; }

.w-ring { cursor: default; }       /* ring centré, non déplaçable (view.cpp:51) */
.w-ring svg { display: block; }
.w-ring-pill, .w-ring-cap {
  position: absolute; left: 50%; transform: translateX(-50%);
  font: 12px Montserrat, system-ui, sans-serif;
}
.w-ring-pill { color: #04121A; border-radius: 13px; padding: 2px 8px; }
.w-ring-cap { font-size: 13px; }

.handle {
  position: absolute; width: 11px; height: 11px; box-sizing: border-box;
  background: var(--accent); border: 1px solid #04121A; border-radius: 2px; z-index: 5;
}
.handle-radius, .handle-thick, .handle-gap { transform: translate(-50%, -50%); }
.handle-radius, .handle-thick { cursor: ns-resize; }
.handle-gap { cursor: ew-resize; }
.handle-br { right: -6px; bottom: -6px; cursor: nwse-resize; }

.badges { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 10px; justify-content: center; }
.badge { font-size: 12px; padding: 3px 8px; border-radius: 12px; border: 1px solid var(--line); color: var(--muted); }
```

- [ ] **Step 4 : vérification visuelle**

Run (depuis `Rich_Telemetry/`, pas `designer/`) :
```bash
python3 -m http.server 8000
```
Ouvrir `http://localhost:8000/designer/`. Expected : la colonne Canvas montre un disque noir 360 px avec un liseré rond en pointillés ; pas d'erreur console (sauf que rien n'est encore dessiné — `canvas.js` arrive en Task 4).

- [ ] **Step 5 : Commit**

```bash
git add designer/index.html designer/style.css designer/vendor/fonts/montserrat-500.woff2
git commit -m "Rich_Telemetry: designer canvas markup + styles + vendored Montserrat"
```

---

## Task 4 : `canvas.js` — page active, sélection, drag+snap, resize + câblage `app.js`

**Files:**
- Create: `designer/js/canvas.js`
- Modify: `designer/js/app.js`

> Vérification au navigateur (le DOM/pointeur n'est pas couvert par `node --test`). La math consommée (`geometry.js`, `render.js`) est déjà testée.

- [ ] **Step 1 : `js/canvas.js`**

```js
// Canvas WYSIWYG : construit la page active (pages[0]) depuis le modèle, gère sélection,
// drag + snap (commit-on-drop) et poignées de redimensionnement. Vérifié au navigateur.
import {
  snapPlacement, placeAt, resizeBox,
  ringRadiusAt, ringThicknessAt, gapDegAt, cornersOutsideCircle, SCREEN
} from './geometry.js';
import {
  buildLabel, buildReadout, buildBar, buildRing, buildBadge,
  ringPaths, pickThresholdColor, MOCKS
} from './render.js';

// Le firmware CENTRE le ring (lv_obj_center, view.cpp:51) : anchor/dx/dy ignorés.
// → un ring n'est pas déplaçable, seulement redimensionnable.
const DRAGGABLE = new Set(['label', 'readout', 'bar']);

export function createCanvas({ stage, badges }, model, { onSelect } = {}) {
  let selected = null; // index du placement sélectionné sur la page active

  const placements = () => model.state.pages?.[0]?.place ?? [];
  const comps = () => model.state.components || {};
  const nodeFor = i => stage.querySelector(`.w[data-pi="${i}"]`);

  function buildNode(pl, comp) {
    if (comp.type === 'bar')     return buildBar(comp, pl);
    if (comp.type === 'ring')    return buildRing(comp, pl);
    if (comp.type === 'readout') return buildReadout(comp);
    return buildLabel(comp); // label
  }

  function position(node, pl, comp) {
    if (comp.type === 'ring') {                 // centré, ignore anchor/dx/dy
      const r = pl.radius || 80;
      node.style.left = (SCREEN / 2 - r) + 'px';
      node.style.top  = (SCREEN / 2 - r) + 'px';
      return;
    }
    const rect = node.getBoundingClientRect();  // 1:1 → px = unités écran
    const { x, y } = placeAt(pl.anchor || 'CENTER', pl.dx || 0, pl.dy || 0, rect.width, rect.height);
    node.style.left = x + 'px';
    node.style.top  = y + 'px';
    node.classList.toggle('outside', cornersOutsideCircle(x, y, rect.width, rect.height));
  }

  function render() {
    stage.querySelectorAll('.w').forEach(n => n.remove());
    badges.replaceChildren();
    stage.style.background = model.state.background || '#000000';
    placements().forEach((pl, i) => {
      const comp = comps()[pl.ref];
      if (!comp) return;                         // ref inconnue : la validation le signale déjà
      if (comp.type === 'led_ring' || comp.type === 'sound') {
        badges.appendChild(buildBadge(pl.ref, comp));
        return;
      }
      const node = buildNode(pl, comp);
      node.dataset.pi = i;
      stage.appendChild(node);                   // append avant de mesurer
      position(node, pl, comp);
      node.addEventListener('pointerdown', e => onPointerDown(e, i, node, comp, pl));
    });
    applySelection();
  }

  function applySelection() {
    stage.querySelectorAll('.w.selected').forEach(n => n.classList.remove('selected'));
    stage.querySelectorAll('.handle').forEach(n => n.remove());
    if (selected == null) return;
    const node = nodeFor(selected);
    if (!node) { selected = null; return; }
    node.classList.add('selected');
    const pl = placements()[selected];
    const comp = comps()[pl.ref];
    if (comp.type === 'bar')  addBarHandles(node, selected, pl);
    if (comp.type === 'ring') addRingHandles(node, selected, comp, pl);
  }

  function select(i) {
    selected = i;
    applySelection();
    onSelect && onSelect(i == null ? null : placements()[i]);
  }

  // --- Drag (label/readout/bar) : aperçu live, UN SEUL commit au drop (piège HANDOFF a) ---
  function onPointerDown(e, i, node, comp, pl) {
    if (e.target.classList.contains('handle')) return; // laisser le resize gérer
    select(i);
    if (!DRAGGABLE.has(comp.type)) return;             // ring : centré, non déplaçable
    e.preventDefault();
    const sr = stage.getBoundingClientRect();
    const nr = node.getBoundingClientRect();
    const grabX = e.clientX - nr.left, grabY = e.clientY - nr.top;
    const w = nr.width, h = nr.height;
    node.setPointerCapture(e.pointerId);
    let live = null;
    const move = ev => {
      const x = ev.clientX - sr.left - grabX;
      const y = ev.clientY - sr.top  - grabY;
      live = snapPlacement(x, y, w, h, 16);
      const p = placeAt(live.anchor, live.dx, live.dy, w, h);
      node.style.left = p.x + 'px'; node.style.top = p.y + 'px';
      node.classList.toggle('snapped', live.snapped);
      node.classList.toggle('outside', cornersOutsideCircle(p.x, p.y, w, h));
    };
    const up = () => {
      node.releasePointerCapture(e.pointerId);
      node.removeEventListener('pointermove', move);
      node.removeEventListener('pointerup', up);
      node.classList.remove('snapped');
      if (live) model.commit(s => {                    // commit unique, pas par frame
        const q = s.pages[0].place[i];
        q.anchor = live.anchor; q.dx = live.dx; q.dy = live.dy;
      });
    };
    node.addEventListener('pointermove', move);
    node.addEventListener('pointerup', up);
  }

  // --- Resize bar : poignée bas-droite → width/height ---
  function addBarHandles(node, i, pl) {
    const h = document.createElement('div');
    h.className = 'handle handle-br';
    node.appendChild(h);
    h.addEventListener('pointerdown', e => {
      e.stopPropagation(); e.preventDefault();
      const startW = pl.width || 200, startH = pl.height || 16;
      const sx = e.clientX, sy = e.clientY;
      const track = node.querySelector('.w-bar-track');
      h.setPointerCapture(e.pointerId);
      let dim = null;
      const move = ev => {
        dim = resizeBox(startW, startH, ev.clientX - sx, ev.clientY - sy, 8);
        track.style.width = dim.width + 'px'; track.style.height = dim.height + 'px';
      };
      const up = () => {
        h.releasePointerCapture(e.pointerId);
        h.removeEventListener('pointermove', move); h.removeEventListener('pointerup', up);
        if (dim) model.commit(s => { const q = s.pages[0].place[i]; q.width = dim.width; q.height = dim.height; });
      };
      h.addEventListener('pointermove', move); h.addEventListener('pointerup', up);
    });
  }

  // --- Resize ring : 3 poignées (radius / thickness / gap_deg), aperçu live, commit au drop ---
  function positionRingHandles(node, g) {
    const c = g.r; // centre dans le repère du wrap (taille 2r)
    const set = (sel, x, y) => { const el = node.querySelector(sel); if (el) { el.style.left = x + 'px'; el.style.top = y + 'px'; } };
    set('.handle-radius', c, c - g.r);            // bord externe, haut
    set('.handle-thick',  c, c - g.r + g.th);     // bord interne, haut
    const a = (90 + g.gap / 2) * Math.PI / 180;   // extrémité de l'ouverture (bas)
    set('.handle-gap', c + g.r * Math.cos(a), c + g.r * Math.sin(a));
  }

  function paintRing(node, comp, g) {
    const size = g.r * 2;
    node.style.width = size + 'px'; node.style.height = size + 'px';
    node.style.left = (SCREEN / 2 - g.r) + 'px'; node.style.top = (SCREEN / 2 - g.r) + 'px';
    const svg = node.querySelector('svg');
    svg.setAttribute('width', size); svg.setAttribute('height', size);
    svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
    const p = ringPaths(g.r, g.th, g.gap, MOCKS.ring.value, comp.min ?? 0, comp.max ?? 100);
    const col = pickThresholdColor(comp.thresholds, MOCKS.ring.value, comp.color || '#38BDF8');
    const t = svg.querySelector('.ring-track'), ind = svg.querySelector('.ring-ind');
    t.setAttribute('d', p.track); t.setAttribute('stroke-width', g.th);
    ind.setAttribute('d', p.indicator); ind.setAttribute('stroke-width', g.th); ind.setAttribute('stroke', col);
    const pill = node.querySelector('.w-ring-pill'); if (pill) pill.style.top = g.th + 'px';
    const cap = node.querySelector('.w-ring-cap');   if (cap)  cap.style.bottom = g.th + 'px';
    positionRingHandles(node, g);
  }

  function addRingHandles(node, i, comp, pl) {
    const geo = () => ({ r: pl.radius || 80, th: pl.thickness || 16, gap: pl.gap_deg ?? 70 });
    for (const kind of ['radius', 'thick', 'gap']) {
      const h = document.createElement('div');
      h.className = 'handle handle-' + kind;
      node.appendChild(h);
      h.addEventListener('pointerdown', e => {
        e.stopPropagation(); e.preventDefault();
        const sr = stage.getBoundingClientRect();
        h.setPointerCapture(e.pointerId);
        let g = geo();
        const move = ev => {
          const px = ev.clientX - sr.left, py = ev.clientY - sr.top; // coords écran
          const base = geo();
          if (kind === 'radius')      g = { ...base, r:  ringRadiusAt(px, py) };
          else if (kind === 'thick')  g = { ...base, th: ringThicknessAt(px, py, base.r) };
          else                        g = { ...base, gap: gapDegAt(px, py) };
          paintRing(node, comp, g);
        };
        const up = () => {
          h.releasePointerCapture(e.pointerId);
          h.removeEventListener('pointermove', move); h.removeEventListener('pointerup', up);
          model.commit(s => {
            const q = s.pages[0].place[i];
            q.radius = g.r; q.thickness = g.th; q.gap_deg = g.gap;
          });
        };
        h.addEventListener('pointermove', move); h.addEventListener('pointerup', up);
      });
    }
    positionRingHandles(node, geo());
  }

  // Clic dans le vide (fond ou liseré) → désélection.
  stage.addEventListener('pointerdown', e => {
    if (e.target === stage || e.target.classList.contains('screen-circle')) select(null);
  });

  model.subscribe(render);
  render();
  return { render, getSelected: () => selected };
}
```

- [ ] **Step 2 : câbler le canvas dans `js/app.js`**

Ajouter l'import en tête de `designer/js/app.js` (sous les imports existants) :
```js
import { createCanvas } from './canvas.js';
```
Puis, dans `main()`, juste après `const model = createModel();` :
```js
  // Canvas WYSIWYG (page 0). La sélection sera consommée par l'inspecteur en Plan C.
  createCanvas({ stage: $('stage'), badges: $('badges') }, model, {
    onSelect: () => {}
  });
```

- [ ] **Step 3 : remplacer le layout de départ par un layout de démo (pour voir le rendu)**

> Le `DEFAULT_LAYOUT` actuel (un seul label) ne montre pas grand-chose. On l'enrichit pour exercer les 4 widgets + un badge. Reste **valide** vis-à-vis du schéma.

Remplacer le contenu de `designer/js/default-layout.js` par :
```js
// Layout de départ de l'éditeur. Valide vis-à-vis de layout.schema.json. Indépendant du firmware.
export const DEFAULT_LAYOUT = {
  title: "Dashboard",
  background: "#0B0B0F",
  components: {
    titre: { type: "label", text: "Dashboard", font: 20, color: "#FFFFFF" },
    cpu:   { type: "readout", label: "CPU", unit: "%", font: 20, color: "#38BDF8" },
    ram:   { type: "bar", label: "RAM", min: 0, max: 100, color: "#38BDF8" },
    jauge: { type: "ring", color: "#A78BFA", pill: true, countdown: true,
             thresholds: [[20, "#F87171"], [50, "#FBBF24"]] },
    led:   { type: "led_ring" },
    buzz:  { type: "sound" }
  },
  pages: [
    { name: "Page 1", place: [
      { ref: "jauge", radius: 160, thickness: 16, gap_deg: 70 },
      { ref: "titre", anchor: "TOP_MID", dy: 40 },
      { ref: "cpu", anchor: "CENTER", dy: -20 },
      { ref: "ram", anchor: "BOTTOM_MID", dy: -60, width: 200, height: 16 },
      { ref: "led" },
      { ref: "buzz" }
    ] }
  ]
};
```

- [ ] **Step 4 : vérification au navigateur (Playwright ou manuel)**

Run (depuis `Rich_Telemetry/`) : `python3 -m http.server 8000`, ouvrir `http://localhost:8000/designer/`. Vérifier :
1. **Rendu** : l'anneau violet centré (avec pastille `72%` en haut et un countdown `5h00` dans l'ouverture du bas), le titre en haut, le readout `CPU 42 %` au centre, la barre `RAM` remplie ~60 % en bas ; deux badges `◉ LED ring : led` et `♪ sound : buzz` sous le canvas. Le `✓ valide` du Plan A reste vert.
2. **Sélection** : cliquer un widget l'entoure d'un liseré bleu ; cliquer le fond désélectionne.
3. **Drag + snap** : glisser le titre → il suit la souris ; près d'un ancrage le liseré passe vert (snap) et le JSON avancé montre `anchor`/`dx`/`dy` mis à jour ; **une seule** entrée Undo créée par déplacement (pas une par frame).
4. **Resize bar** : glisser la poignée bas-droite de la barre → `width`/`height` changent dans le JSON.
5. **Resize ring** : les 3 poignées changent `radius` / `thickness` / `gap_deg` ; l'arc se redessine live ; au drop le JSON reflète les 3 valeurs.
6. **Ring non déplaçable** : cliquer-glisser le corps de l'anneau (hors poignées) ne le déplace pas (comportement voulu — le firmware le centre).
7. **Undo/Redo** reviennent/réappliquent ; **circle-awareness** : déplacer un widget vers un coin (TOP_LEFT) affiche un liseré rouge pointillé (hors zone ronde).

Expected : tous les points OK, aucune erreur console.

- [ ] **Step 5 : Commit**

```bash
git add designer/js/canvas.js designer/js/app.js designer/js/default-layout.js
git commit -m "Rich_Telemetry: designer canvas (drag/snap/resize) + demo default layout"
```

---

## Task 5 : README « aperçu indicatif » + vérification finale

**Files:**
- Modify: `designer/README.md`

- [ ] **Step 1 : documenter le caractère indicatif de l'aperçu**

Ajouter dans `designer/README.md` une section (l'emplacement exact suit la structure du README ; à défaut, en fin de fichier) :
```markdown
## Aperçu : indicatif, pas pixel-exact

Le canvas est une **2e implémentation** du rendu (la 1re étant le firmware, `src/view.cpp` + `src/dashboard.cpp`).
Il vise le « best-effort » : positions et métriques à quelques pixels près, polices approchées. **Le device
reste l'arbitre final.** Conséquences à connaître :

- Les **valeurs affichées sont des mocks** (voir `MOCKS` dans `js/render.js`) ; à l'exécution, `/update` les remplace.
- Le **ring est toujours centré** (le firmware fait `lv_obj_center`) : `anchor`/`dx`/`dy` sont ignorés pour un ring ;
  dans l'éditeur il n'est que redimensionnable (radius / thickness / gap_deg).
- Tout changement de rendu firmware (nouveau widget, nouveau style) **doit être répliqué** dans `js/render.js`.
```

- [ ] **Step 2 : suite de tests complète**

Run (depuis `designer/`) : `node --test`
Expected : PASS — tous les tests Plan A + les nouveaux de Task 1 (geometry) et Task 2 (render).

- [ ] **Step 3 : revue navigateur holistique**

Re-dérouler la checklist de Task 4 Step 4 une fois de plus, à froid (recharger la page). Confirmer en particulier qu'aucune action de drag/resize ne crée plus d'une entrée Undo, et que JSON avancé ↔ canvas restent synchronisés dans les deux sens (éditer le JSON + Appliquer → le canvas se met à jour).

- [ ] **Step 4 : Commit**

```bash
git add designer/README.md
git commit -m "Rich_Telemetry: designer README — apercu indicatif (Plan B)"
```

---

## Self-Review (effectuée à la rédaction)

**1. Couverture du spec / HANDOFF (Plan B) :**
- `render.js` best-effort label/readout/bar/ring → Task 2 ✓ ; badges led_ring/sound → Task 2 (`buildBadge`) ✓
- Webfont Montserrat → Task 3 ✓ (vendorisée, fallback system-ui)
- Ring = arc avec ouverture `gap_deg` en bas + pill + couleurs de seuil → Task 2 (`ringPaths`/`buildRing`, miroir `view.cpp:54`/`color.cpp:13`) ✓
- `canvas.js` drag + snap via `geometry.js` + sélection + poignées de redim (bar width/height ; ring radius/thickness/gap_deg) → Task 4 ✓
- Valeurs d'aperçu mock → Task 2 (`MOCKS`) ✓ (éditables = Plan C)
- Piège (a) commit-on-drop / coalescer → Task 4 : `model.commit` **uniquement** au `pointerup`, jamais par frame ✓
- Piège (b) math de resize + circle-awareness net-new → Task 1 (geometry, testée) ✓
- Champs réservés (`center_pct`, `start_angle`) : non rendus, non manipulés par le canvas ✓ (conformes au spec)

**2. Placeholders :** aucun. Tout le code (helpers, builders, canvas, tests, CSS, layout démo) est fourni en entier. Le seul élément réseau (téléchargement de la police) a une issue de repli explicite.

**3. Cohérence des types/symboles :**
- `geometry.js` : nouveaux exports `resizeBox / ringRadiusAt / ringThicknessAt / gapDegAt / cornersOutsideCircle` — mêmes signatures entre Task 1 (impl + tests) et Task 4 (canvas).
- `render.js` : `ringPaths(r, th, gap, value, min, max) → {rr, start, track, indicator}` consommé identiquement par `buildRing` (Task 2) et `paintRing` (Task 4) ; `pickThresholdColor`, `MOCKS`, `buildLabel/Readout/Bar/Ring/Badge` exportés et utilisés tels quels par `canvas.js`.
- `createCanvas({stage, badges}, model, {onSelect}) → {render, getSelected}` instancié en cohérence dans `app.js`.
- Classes DOM partagées render↔canvas : `.w`, `.w-bar-track`, `svg .ring-track`/`.ring-ind`, `.w-ring-pill`, `.w-ring-cap`, `.handle-*` — définies en Task 2/3 et requêtées en Task 4.

**4. Convention codebase :** TDD `node --test` pour la logique pure, vérif navigateur pour le DOM (calque exact du Plan A) ; modules ES zéro-build ; vendoring d'un artefact unique (la police, comme ajv) ; commits fréquents.

---

## Décision à confirmer avec l'utilisateur

**Ring centré, non déplaçable.** Le firmware (`src/view.cpp:51`) centre les rings et ignore `anchor/dx/dy`. Ce
plan rend donc le ring centré et non déplaçable (resize seul), par fidélité (le device arbitre — décision #3 du
spec). C'est le seul point où le Plan B s'écarte de la lettre du spec (positionnement universel par ancrage).
**Alternative** si on veut des rings librement positionnables : ce serait une évolution **firmware** (remplacer
`lv_obj_center` par un `lv_obj_align(anchor, dx, dy)`), donc hors scope du designer et à arbitrer séparément.

---

## Execution Handoff

Plan complet et sauvegardé dans `designer/plans/2026-06-15-wysiwyg-editor-plan-b-canvas.md`. Deux options d'exécution :

1. **Subagent-Driven (recommandé)** — un subagent frais par tâche, revue (spec puis qualité) entre tâches, vérif navigateur du DOM par le contrôleur (Playwright). Cf. `superpowers:subagent-driven-development`.
2. **Inline** — exécution dans cette session, par lots avec checkpoints. Cf. `superpowers:executing-plans`.

Après B viendra **Plan C** (palette, inspecteur, pages CRUD, file-io, mutations dédiées `model.js`).
