# Prompt — Corriger les écarts de rendu du designer Rich_Telemetry vs device

## Contexte
`devices/guition_knob/projects/Rich_Telemetry/designer/js/render.js` est une **2e implémentation
« best-effort »** du rendu firmware (`src/view.cpp` + `src/dashboard.cpp`). **Le device arbitre.**
Une comparaison composant par composant (rapport : `snapshots/parity/parity-report.html`, images
`device-*.png` / `designer-*.png`) a isolé les écarts ci-dessous. Méthodologie : layout banc d'essai
(1 composant/page) poussé au device avec **les mêmes valeurs que les mocks** de `render.js`
(`MOCKS`), échelle 1:1.

Objectif : rapprocher visuellement l'aperçu designer du rendu device. **Pas de pixel-perfect**
attendu sur les widgets natifs LVGL (chart/meter) ; viser la proximité. Ne **rien** changer au
firmware. Conserver le style du fichier (math pure testée + builders DOM). Après modif :
`node --test` doit rester vert (88/88) et le rendu doit être vérifié au navigateur (servir
`Rich_Telemetry/`, charger un layout, comparer au device via `GET /screenshot`).

Fichiers concernés : `designer/js/render.js`, `designer/js/canvas.js` (fonction `paintRing` qui
duplique la géométrie ring au resize — à garder synchrone), `designer/style.css`.

---

## Fix 1 — Police plafonnée (HAUT impact, faible coût) — label, readout, ring center_pct
`pickFontPx` (`render.js:15`) ne renvoie que 14/20/28. Le firmware `pick_font` (`view.cpp:21-27`)
gère **36 et 48**. Mettre en miroir exact :

```js
export function pickFontPx(font) {
  if (font >= 48) return 48;
  if (font >= 36) return 36;
  if (font >= 28) return 28;
  if (font >= 20) return 20;
  return 14;
}
```
Vérifier qu'un `label`/`readout` `font:48` s'affiche en 48 px (test : `tests/render.test.js` couvre
`pickFontPx` — ajouter les cas 36/48).

---

## Fix 2 — ring `center_pct` non rendu (HAUT) — `render.js` `buildRing`
`buildRing` (`render.js:146-191`) gère `pill` et `countdown` mais **pas `center_pct`**. Le firmware
(`view.cpp:89-93` build, `view.cpp:165-169` sync) : si `center_pct`, affiche `format_value(value, unit)`
au **centre géométrique**, en police `pick_font(c.font)`, couleur = `center_color` si défini, sinon la
**couleur de seuil** (la même que l'arc). `center_pct` est **prioritaire sur `pill`** (firmware :
`if (center_pct) … else if (pill) …`).

À implémenter dans `buildRing`, **avant** le bloc `if (comp.pill)`, et rendre `pill` exclusif :
```js
const col = pickThresholdColor(comp.thresholds, mock.value, comp.color || '#38BDF8');
// … (track + indicator déjà tracés) …
if (comp.center_pct) {                         // prioritaire sur pill (view.cpp:89)
  const c = document.createElement('div');
  c.className = 'w-ring-center';
  c.style.font = FONT(pickFontPx(comp.font ?? 20));
  c.style.color = comp.center_color || col;    // center_color surcharge le seuil (view.cpp:168)
  c.textContent = formatValue(mock.value, comp.unit || '');
  wrap.appendChild(c);
} else if (comp.pill) {
  /* … bloc pill existant … */
}
```
CSS (`style.css`, à côté de `.w-ring-pill`) — centrer dans le wrap carré du ring :
```css
.w-ring-center{ position:absolute; left:50%; top:50%; transform:translate(-50%,-50%);
                white-space:nowrap; }
```
Note : `formatValue` (`render.js:36`) reproduit déjà `format_value`. Limite firmware connue : unités
non-ASCII non rendues — ne pas « corriger » au-delà du device.

---

## Fix 3 — chart : panneau + grille + points (HAUT) — `render.js` `buildChart` + CSS
Le device rend un `lv_chart` natif (thème par défaut) : **panneau de fond clair arrondi**, **grille**
(lignes de division), **points** à chaque échantillon (`LV_CHART_TYPE_LINE`). Le designer
(`render.js:193-209`) ne trace qu'une polyligne. Ajouter, en visant le thème LVGL par défaut :

1. **Fond** : appliquer au `wrap` (ou à un `<rect>` SVG plein) un fond clair arrondi + bordure légère.
   Référence visuelle device : panneau quasi-blanc, rayon ~8 px, fine bordure grise.
2. **Grille** : tracer des lignes de division. LVGL par défaut ≈ **3 div. horizontales** et
   **5 div. verticales** (à ajuster à l'œil sur `device-chart.png`), en gris clair (`#E5E7EB`-ish),
   fines (1 px).
3. **Points** : un `<circle>` (r ≈ 3) couleur série à chaque point de `sparklinePoints`.
4. **Padding interne** : insérer un petit padding (~6-8 px) pour que la courbe ne colle pas au bord
   (le device a une marge de plot).

Garder `sparklinePoints` (math juste) ; l'enrichir pour retourner aussi les coordonnées des points,
ou ajouter une fonction sœur `sparklineDots(hist, …)`. Couleur ligne/points = `comp.color`.

> Best-effort : viser le « cadre + grille + ligne pointée », pas la réplication exacte du thème.

---

## Fix 4 — meter : fond + graduations + chiffres + moyeu (HAUT) — `render.js` `buildMeter` + CSS
Le device rend un `lv_meter` natif : **fond circulaire clair**, **21 ticks mineurs + 5 majeurs**
(`view.cpp:214-215`), **chiffres d'échelle 0/25/50/75/100**, **moyeu** central. Le designer
(`render.js:211-246`) ne dessine que l'arc de fond, les zones de seuil et une aiguille fine. Ajouter :

1. **Fond** : disque clair (panneau) sous la jauge (rayon ≈ `size/2`), arrondi/ombre légère.
2. **Graduations** : pour `v` de `min` à `max` par pas de `(max-min)/20` → 21 ticks ; tracer un petit
   trait radial à l'angle `meterAngle(v, min, max)` (réutiliser `pointOnArc`), longueur ~8 px
   (mineurs) / ~12 px (majeurs, tous les `(max-min)/4`), couleurs `#4B5563` / `#9CA3AF`
   (mêmes hex que `view.cpp:214-215`).
3. **Chiffres majeurs** : à chaque tick majeur, un `<text>` (ou div) positionné juste à l'intérieur du
   tick, valeur = `v`, couleur `#9CA3AF`.
4. **Moyeu** : `<circle>` plein (r ≈ 6) au centre, couleur sombre, par-dessus la base de l'aiguille.
5. **Cohérence fine** : largeur des arcs de zones **5** (pas 6 ; `view.cpp:220`), aiguille **4 px**
   (`view.cpp:225`), rayon des zones aligné sur le cercle des ticks.

`meterAngle` (math, `render.js:96`) est correct (135°→405°) : le réutiliser pour ticks ET chiffres.

> Best-effort : l'objectif est « cadran gradué chiffré avec moyeu », proche du device.

---

## Fix 5 — bar : centrage du label + forme pilule (MOYEN) — `render.js` `buildBar` + CSS
Device (`view.cpp:134-141`) : label **centré** au-dessus (`LV_ALIGN_OUT_TOP_MID`), barre LVGL
**entièrement arrondie** (pilule, rayon = ½ hauteur), extrémité droite du remplissage arrondie.
Designer : label à gauche, `.w-bar-track` rayon 3 px, remplissage clippé → coin droit franc.

- CSS `.w-bar-label` : ajouter `text-align:center;` et s'assurer que `.w-bar` (wrap) a la **largeur de
  la piste** (sinon le centrage n'est pas relatif à la barre). Optionnel : police 14 px (vs 12) pour
  matcher `montserrat_14`.
- CSS `.w-bar-track` : `border-radius` = ½ hauteur (pilule). Donner aussi au `.w-bar-fill` un
  `border-radius` plein (ou ne pas clipper son extrémité droite) pour l'arrondi à droite comme LVGL.
- Optionnel mineur : rapprocher la nuance du fond de piste de la valeur LVGL par défaut.

---

## Fix 6 — ring : pill non centrée verticalement + liseré (MOYEN) — `render.js` + `canvas.js` + `style.css`
Device (`view.cpp:56-63`, `ring_place_labels`) : la pill est centrée (`LV_ALIGN_CENTER`) sur le
**milieu de la bande** au sommet, rayon `rp = radius − thickness/2` ; sa hauteur déborde la bande de
±2 px → elle **chevauche** la bande. Designer : `pill.style.top = th px` avec CSS `translateX(-50%)`
seul → le **haut** de la pill est au bord interne de la bande, la pill **pend vers l'intérieur** (non
centrée). Le bug est **dupliqué** à deux endroits à garder synchrones :
- `render.js` `buildRing` (`render.js:179`)
- `canvas.js` `paintRing` (`canvas.js:164` — repositionnement au resize live de l'anneau)

Corriger pour centrer la pill sur le milieu de la bande (les **DEUX** endroits) :
```js
// render.js buildRing  ET  canvas.js paintRing
pill.style.top = (th / 2) + 'px';        // dans canvas.js : (g.th / 2)
```
CSS `.w-ring-pill` : lui donner sa **propre** règle de transform centrant aussi en vertical, + le liseré :
```css
.w-ring-pill{ transform:translate(-50%,-50%); border:1px solid #000; }  /* liseré : view.cpp:101 */
```
⚠ `.w-ring-pill` et `.w-ring-cap` partagent aujourd'hui `transform:translateX(-50%)` ; ne **pas** appliquer
le `translateY` au `.w-ring-cap` (la caption est positionnée par `bottom`, pas par son centre).

À vérifier au passage — caption countdown : le firmware la place centrée à rayon `radius − thickness`
DANS l'ouverture (`view.cpp:51-54`) ; le designer utilise `cap.style.bottom = th px` (`render.js:187`,
`canvas.js:165`). Comparer `device-ring.png` / `designer-ring.png` et ajuster si elle est décalée (même
classe de bug de positionnement vertical approximatif). Optionnel : `.w-ring-cap` en 14 px (vs 12).

---

## Vérification attendue
1. `node --test` (designer/) vert ; ajouter cas `pickFontPx(36)===36`, `pickFontPx(48)===48`.
2. Servir `Rich_Telemetry/`, charger le layout banc d'essai (ou via le device), comparer chaque page
   au `device-*.png` correspondant. Les écarts HAUT (center_pct, chart, meter) doivent être levés ;
   les MOYEN/FAIBLE rapprochés.
3. Mettre à jour le commentaire d'en-tête de `render.js` si le périmètre « best-effort » évolue.
