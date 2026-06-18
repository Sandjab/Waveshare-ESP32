# Spec — Style du label de la Bar (alignement + couleur + police)

> Date : 2026-06-18 · Statut : design approuvé (prêt pour le plan) · Projet : `devices/guition_knob/projects/Rich_Telemetry`

## Objectif

Rendre configurables, pour le composant **Bar**, les trois propriétés de son **label** :

1. **Alignement** du label autour de la barre, parmi **8 positions** (les bords/coins extérieurs).
2. **Couleur** du label.
3. **Taille de police** du label.

Aujourd'hui ces trois valeurs sont **codées en dur** dans le firmware.

## État actuel (référence)

- **Firmware** `src/view.cpp` `build_bar()` (≈ lignes 133-149) : le label de la barre est créé en sous-objet LVGL avec :
  - police figée `&lv_font_montserrat_14` ;
  - couleur figée `0x9AA0AA` (gris) ;
  - position figée `lv_obj_align_to(bl, b, LV_ALIGN_OUT_TOP_MID, 0, -6)` (au-dessus, centré, marge −6).
- **Designer** `designer/js/render.js` `buildBar()` : label `.w-bar-label` rendu au-dessus du track, style CSS fixe (pas de couleur/police propres).
- **Schéma** `schema/layout.schema.json` `$defs/comp_bar` : propriétés `type, bind, label, min, max, color` (où `color` = couleur de **remplissage** de la barre, à ne pas confondre).
- **Briques réutilisables** :
  - `$defs/font` (enum `14, 20, 28, 36, 48`), `$defs/hexColor` (`#RRGGBB`).
  - `$defs/anchor` (9 positions : `CENTER` + 8 périphériques).
  - Firmware : `pick_font()` (déjà utilisé par `build_text`), `parse_anchor()` (`dashboard.cpp`), `ALIGN_MAP[]` (`view.cpp:22`, indexé par l'enum `Anchor`, vers `LV_ALIGN_*` **interne**), `parse_hex_color()`.

## Décisions cadrées avec l'utilisateur

- **Position du label = autour de la barre (extérieur)** → mappé sur `LV_ALIGN_OUT_*`. (Pas de surimpression interne.)
- **8 positions** = `TOP_LEFT, TOP_MID, TOP_RIGHT, LEFT_MID, RIGHT_MID, BOTTOM_LEFT, BOTTOM_MID, BOTTOM_RIGHT` (sans `CENTER`).
- **Structure plate** (approche A) : trois champs `label_color`, `label_font`, `label_align` directement sur `comp_bar` — cohérent avec le schéma et la struct firmware, tous deux plats. (Sous-objet `label_style` écarté.)
- **Marge label↔barre fixe**, non configurable (pas de `dx`/`dy` pour le label) — YAGNI.
- **Défauts = apparence actuelle exacte** (rétrocompat) : un `bar` sans les nouveaux champs reste gris `#9AA0AA`, police `14`, aligné `TOP_MID`.

## Design

### 1. Schéma (`schema/layout.schema.json`) — source de vérité, à committer en premier

- Ajouter à `$defs/comp_bar.properties` (tous **optionnels**) :
  - `label_color` → `{ "$ref": "#/$defs/hexColor", "description": "Couleur du libellé. Defaut #9AA0AA." }`
  - `label_font` → `{ "$ref": "#/$defs/font", "description": "Taille de police du libellé. Defaut 14." }`
  - `label_align` → `{ "$ref": "#/$defs/anchorOut", "description": "Position du libellé autour de la barre. Defaut TOP_MID." }`
- Ajouter `$defs/anchorOut` : enum des **8** positions périphériques (mêmes noms que `$defs/anchor`, **sans `CENTER`**). Réutiliser les noms permet de réemployer `parse_anchor()` côté firmware.
- `additionalProperties:false` est conservé : le designer attrape les fautes de frappe.

### 2. Firmware

- **`src/dashboard.h`** — `struct Component` += :
  - `uint32_t label_color;`
  - `uint16_t label_font;`
  - `Anchor   label_align;`
- **`src/dashboard.cpp`** — dans le parsing des composants (≈ lignes 73-85), ajouter (défauts = apparence actuelle) :
  - `c.label_color = parse_hex_color(o["label_color"] | "#9AA0AA", 0x9AA0AA);`
  - `c.label_font  = o["label_font"] | 14;`
  - `c.label_align = parse_anchor(o["label_align"] | "TOP_MID");`
- **`src/view.cpp`** :
  - Ajouter `ALIGN_OUT_MAP[]` — table `lv_align_t` **parallèle** à `ALIGN_MAP[]`, indexée par l'enum `Anchor`, vers les `LV_ALIGN_OUT_*`. Pour l'index `CENTER` (ne devrait pas survenir car le schéma l'exclut), repli sur `LV_ALIGN_OUT_TOP_MID` (= comportement actuel).
  - Dans `build_bar()`, remplacer les trois valeurs figées du label par :
    - `lv_obj_set_style_text_font(bl, pick_font(c.label_font), 0);`
    - `lv_obj_set_style_text_color(bl, lv_color_hex(c.label_color), 0);`
    - `lv_obj_align_to(bl, b, ALIGN_OUT_MAP[c.label_align], <gapX>, <gapY>);`
  - **Marge** : constante `BAR_LABEL_GAP` (≈ 6 px, valeur conservant le rendu actuel pour `TOP_MID`), appliquée sur l'axe pertinent selon la position (signe dépendant du côté). Détail d'implémentation au plan.

### 3. Designer

- **`designer/js/registry.js`** :
  - `bar.defaults()` += `label_color: '#9AA0AA'`, `label_font: 14`, `label_align: 'TOP_MID'`.
  - `bar.compFields` += `['label_color', 'Couleur label', 'color']`, `['label_font', 'Police label', 'font']`, `['label_align', 'Alignement label', 'anchorOut']`.
- **`designer/js/inspector.js`** : nouveau `kind` d'éditeur `anchorOut` dans `makeInput()` — un `<select>` listant les 8 positions (analogue au `kind` `anchor`, mais sur la liste des 8 valeurs `anchorOut`). La liste des 8 valeurs est exportée depuis `geometry.js` (à côté de `ANCHORS`).
- **`designer/js/render.js`** `buildBar()` + **`designer/style.css`** : appliquer couleur + police au label, et le **positionner autour du track** selon les 8 positions (le track devient la référence, le label en `position:absolute`). Couvrir les 8 cas en CSS (classe dérivée de l'alignement, ex. `w-bar-label--top-left`).

### 4. Défauts & rétrocompatibilité

Les trois champs sont optionnels partout (schéma, firmware via `| défaut`, designer via `defaults`). Un layout `bar` antérieur, sans ces champs, s'affiche **exactement** comme aujourd'hui : gris `#9AA0AA`, police `14`, label au-dessus centré.

### 5. Point d'attention — fidélité du rendu designer

Le firmware aligne la **barre** seule par `anchor/dx/dy`, puis place le label **en débordement** (`LV_ALIGN_OUT_*`) sans déplacer la barre. Côté designer, `canvas.js` `position()` mesure la bounding box du nœud entier pour appliquer l'ancrage du placement : si le label déborde du wrap, il fausse le centrage de la barre. L'implémentation devra donc positionner le label **hors flux** (absolu) de sorte que le placement de la barre ne dépende pas du label (ou mesurer le track seul). À trancher dans le plan ; à vérifier visuellement (canvas vs `GET /screenshot`).

### 6. Tests / validation

- **`tests/schema.test.js`** : un `bar` avec `label_color`/`label_font`/`label_align` valides est accepté ; `label_align: "CENTER"` ou valeur inconnue est **rejeté** ; `label_font` hors enum rejeté.
- **`tests/registry.test.js`** : conformité clés registre ↔ schéma (déjà en place) reste verte.
- **Designer** : `node --test` au vert ; validation navigateur des 8 positions (rendu correct, couleur/police appliquées, placement de la barre inchangé).
- **Firmware** : `pio test -e native` (parsing des nouveaux champs + défauts) ; `pio run -e esp32s3` SUCCESS ; validation on-device par `GET /screenshot` (les 8 positions + défaut rétrocompat).

## Hors scope (YAGNI)

- Offset `dx`/`dy` propre au label (marge fixe seulement).
- Surimpression du label *dans* la barre / position `CENTER` extérieure.
- Style de label configurable pour les autres composants (readout/chart/meter) — non demandé.

## Ordre de livraison (rappel du contrat du schéma)

Le schéma est la source de vérité unique : **commit dédié du schéma d'abord**, puis firmware et designer s'y alignent (cf. en-tête de `schema/layout.schema.json`).
