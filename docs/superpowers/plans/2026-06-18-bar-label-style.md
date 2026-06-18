# Style du label de la Bar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rendre configurables l'alignement (8 positions extérieures), la couleur et la taille de police du label du composant Bar, aujourd'hui codés en dur.

**Architecture:** Le schéma `layout.schema.json` est la source de vérité : on l'étend en premier (3 champs optionnels sur `comp_bar` + `$defs/anchorOut`), puis le firmware (parsing + rendu LVGL via `LV_ALIGN_OUT_*`) et le designer (registre, inspecteur, rendu DOM) s'y alignent. Les défauts reproduisent l'apparence actuelle (gris `#9AA0AA`, police 14, `TOP_MID`) → rétrocompatibilité totale.

**Tech Stack:** JSON Schema (Ajv côté designer), C++/LVGL (firmware, PlatformIO), JS ES modules + `node:test` (designer), Unity (`pio test -e native`).

Spec : `docs/superpowers/specs/2026-06-18-bar-label-style-design.md`.

Répertoire de travail (tous les chemins sont relatifs à) : `devices/guition_knob/projects/Rich_Telemetry/`.

---

## Task 1 : Schéma — `anchorOut` + 3 champs sur `comp_bar`

**Files:**
- Modify: `schema/layout.schema.json` (`$defs/anchorOut` + `$defs/comp_bar.properties`)
- Test: `designer/tests/schema.test.js`

- [ ] **Step 1: Écrire les tests qui échouent**

Ajouter à la fin de `designer/tests/schema.test.js` (avant la fin du fichier) :

```js
test('schema : bar avec style de label (couleur/police/alignement) valide', () => {
  const l = base();
  l.components.b = { type: 'bar', label: 'RAM', label_color: '#FF0000', label_font: 20, label_align: 'BOTTOM_MID' };
  l.pages[0].place.push({ ref: 'b', anchor: 'CENTER', width: 200, height: 16 });
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : bar label_align = CENTER rejeté (8 positions extérieures seulement)', () => {
  const l = base();
  l.components.b = { type: 'bar', label_align: 'CENTER' };
  l.pages[0].place.push({ ref: 'b' });
  assert.equal(validate(l).valid, false);
});

test('schema : bar label_font hors enum rejeté', () => {
  const l = base();
  l.components.b = { type: 'bar', label_font: 17 };
  l.pages[0].place.push({ ref: 'b' });
  assert.equal(validate(l).valid, false);
});
```

- [ ] **Step 2: Lancer les tests pour vérifier l'échec**

Run: `cd designer && node --test`
Expected: FAIL — les 3 nouveaux tests échouent (le 1er car `label_color`/`label_font`/`label_align` sont rejetés par `additionalProperties:false` ; les 2 autres car aucune contrainte n'existe encore donc le rejet attendu ne se produit pas tel quel — le 1er au moins doit être rouge).

- [ ] **Step 3: Ajouter `$defs/anchorOut`**

Dans `schema/layout.schema.json`, juste après le bloc `$defs/anchor` (l'enum à 9 valeurs), ajouter :

```json
    "anchorOut": {
      "enum": [
        "TOP_LEFT", "TOP_MID", "TOP_RIGHT",
        "LEFT_MID", "RIGHT_MID",
        "BOTTOM_LEFT", "BOTTOM_MID", "BOTTOM_RIGHT"
      ],
      "description": "Position d'un libelle autour de son widget (LV_ALIGN_OUT_*). Les 8 bords/coins exterieurs, sans CENTER."
    },
```

- [ ] **Step 4: Étendre `comp_bar`**

Dans `$defs/comp_bar.properties`, après la ligne `"color": { "$ref": "#/$defs/hexColor" }`, ajouter (penser à la virgule sur la ligne `color`) :

```json
        "color": { "$ref": "#/$defs/hexColor" },
        "label_color": { "$ref": "#/$defs/hexColor", "description": "Couleur du libelle. Defaut #9AA0AA." },
        "label_font": { "$ref": "#/$defs/font", "description": "Taille de police du libelle. Defaut 14." },
        "label_align": { "$ref": "#/$defs/anchorOut", "description": "Position du libelle autour de la barre. Defaut TOP_MID." }
```

- [ ] **Step 5: Lancer les tests pour vérifier le succès**

Run: `cd designer && node --test`
Expected: PASS — 155 tests (152 + 3).

- [ ] **Step 6: Commit**

```bash
git add schema/layout.schema.json designer/tests/schema.test.js
git commit -m "feat(Rich_Telemetry schema): style de label sur la Bar (label_color/font/align + anchorOut)"
```

---

## Task 2 : Firmware — parsing des 3 champs

**Files:**
- Modify: `src/dashboard.h` (struct `Component`)
- Modify: `src/dashboard.cpp` (parsing, ≈ après la ligne `c.font = o["font"] | 20;`)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1: Écrire les tests qui échouent**

Ajouter dans `test/test_core/test_main.cpp`, juste après `test_ctxapply_bar_value` (≈ ligne 469) :

```cpp
void test_bar_label_style_parsed(void) {
    Dashboard d{}; char err[80];
    const char* j = "{\"components\":{\"b\":{\"type\":\"bar\",\"label\":\"RAM\","
                    "\"label_color\":\"#FF0000\",\"label_font\":20,\"label_align\":\"BOTTOM_MID\"}},"
                    "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"b\"}]}]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, j, err, sizeof(err)));
    int i = dash_find(&d, "b");
    TEST_ASSERT_EQUAL_HEX32(0xFF0000, d.components[i].label_color);
    TEST_ASSERT_EQUAL_INT(20, d.components[i].label_font);
    TEST_ASSERT_EQUAL_INT(A_BOTTOM_MID, d.components[i].label_align);
}
void test_bar_label_style_defaults(void) {
    Dashboard d{}; char err[80];
    const char* j = "{\"components\":{\"b\":{\"type\":\"bar\",\"label\":\"RAM\"}},"
                    "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"b\"}]}]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, j, err, sizeof(err)));
    int i = dash_find(&d, "b");
    TEST_ASSERT_EQUAL_HEX32(0x9AA0AA, d.components[i].label_color);
    TEST_ASSERT_EQUAL_INT(14, d.components[i].label_font);
    TEST_ASSERT_EQUAL_INT(A_TOP_MID, d.components[i].label_align);
}
```

Et enregistrer les deux dans `main()` (la fonction qui enchaîne les `RUN_TEST(...)`), à côté de `RUN_TEST(test_ctxapply_bar_value);` :

```cpp
    RUN_TEST(test_bar_label_style_parsed);
    RUN_TEST(test_bar_label_style_defaults);
```

- [ ] **Step 2: Lancer les tests pour vérifier l'échec**

Run: `pio test -e native -f test_core`
Expected: FAIL — `label_color`/`label_font`/`label_align` n'existent pas encore dans `struct Component` → erreur de compilation.

- [ ] **Step 3: Étendre `struct Component`**

Dans `src/dashboard.h`, dans `struct Component`, juste après la ligne `uint16_t font;`, ajouter :

```cpp
    uint16_t font;
    uint32_t label_color;            // bar : couleur du libelle (defaut 0x9AA0AA)
    uint16_t label_font;             // bar : taille de police du libelle (defaut 14)
    Anchor   label_align;            // bar : position du libelle autour de la barre (defaut A_TOP_MID)
```

- [ ] **Step 4: Parser les 3 champs**

Dans `src/dashboard.cpp`, juste après la ligne `c.font = o["font"] | 20;`, ajouter :

```cpp
        c.font        = o["font"] | 20;
        c.label_color = parse_hex_color(o["label_color"] | "#9AA0AA", 0x9AA0AA);
        c.label_font  = o["label_font"] | 14;
        c.label_align = parse_anchor(o["label_align"] | "TOP_MID");
```

- [ ] **Step 5: Lancer les tests pour vérifier le succès**

Run: `pio test -e native -f test_core`
Expected: PASS — 77 tests (75 + 2).

- [ ] **Step 6: Commit**

```bash
git add src/dashboard.h src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "feat(Rich_Telemetry firmware): parser label_color/label_font/label_align de la Bar"
```

---

## Task 3 : Firmware — rendu LVGL du label (`LV_ALIGN_OUT_*`)

**Files:**
- Modify: `src/view.cpp` (table `ALIGN_OUT_MAP` + `build_bar`)

Pas de test unitaire (rendu LVGL non testable en natif) : la vérification est la compilation puis la validation on-device (Task 6).

- [ ] **Step 1: Ajouter `ALIGN_OUT_MAP`**

Dans `src/view.cpp`, juste après le tableau `ALIGN_MAP[]` (≈ ligne 25), ajouter (l'ordre suit l'enum `Anchor` de `dashboard.h` : `A_CENTER, A_TOP_MID, A_BOTTOM_MID, A_LEFT_MID, A_RIGHT_MID, A_TOP_LEFT, A_TOP_RIGHT, A_BOTTOM_LEFT, A_BOTTOM_RIGHT`) :

```cpp
// Parallele a ALIGN_MAP, mais en alignement EXTERIEUR (label autour de son parent).
// A_CENTER n'a pas d'equivalent OUT -> repli sur OUT_TOP_MID (= comportement historique du label de barre).
static const lv_align_t ALIGN_OUT_MAP[] = {
    LV_ALIGN_OUT_TOP_MID,
    LV_ALIGN_OUT_TOP_MID, LV_ALIGN_OUT_BOTTOM_MID, LV_ALIGN_OUT_LEFT_MID, LV_ALIGN_OUT_RIGHT_MID,
    LV_ALIGN_OUT_TOP_LEFT, LV_ALIGN_OUT_TOP_RIGHT, LV_ALIGN_OUT_BOTTOM_LEFT, LV_ALIGN_OUT_BOTTOM_RIGHT
};

static const int16_t BAR_LABEL_GAP = 6;   // ecart fixe label<->barre (conserve le rendu actuel pour TOP_MID)
```

- [ ] **Step 2: Remplacer le rendu hardcodé dans `build_bar`**

Dans `src/view.cpp`, `build_bar()`, remplacer le bloc actuel :

```cpp
    if (c.label[0]) {
        lv_obj_t* bl = lv_label_create(parent);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(bl, lv_color_hex(0x9AA0AA), 0);
        lv_label_set_text(bl, c.label);
        lv_obj_align_to(bl, b, LV_ALIGN_OUT_TOP_MID, 0, -6);
        *sub1 = bl;
    }
```

par :

```cpp
    if (c.label[0]) {
        lv_obj_t* bl = lv_label_create(parent);
        lv_obj_set_style_text_font(bl, pick_font(c.label_font), 0);
        lv_obj_set_style_text_color(bl, lv_color_hex(c.label_color), 0);
        lv_label_set_text(bl, c.label);
        int16_t gx = 0, gy = 0;
        switch (c.label_align) {
            case A_BOTTOM_MID: case A_BOTTOM_LEFT: case A_BOTTOM_RIGHT: gy =  BAR_LABEL_GAP; break;
            case A_LEFT_MID:  gx = -BAR_LABEL_GAP; break;
            case A_RIGHT_MID: gx =  BAR_LABEL_GAP; break;
            default:          gy = -BAR_LABEL_GAP; break;   // TOP_* et repli A_CENTER
        }
        lv_obj_align_to(bl, b, ALIGN_OUT_MAP[c.label_align], gx, gy);
        *sub1 = bl;
    }
```

- [ ] **Step 3: Compiler**

Run: `pio run -e esp32s3`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/view.cpp
git commit -m "feat(Rich_Telemetry firmware): rendre le label de la Bar selon label_color/font/align"
```

---

## Task 4 : Designer — registre, inspecteur, constante des 8 positions

**Files:**
- Modify: `designer/js/geometry.js` (export `ANCHORS_OUT`)
- Modify: `designer/js/registry.js` (`bar.defaults` + `bar.compFields`)
- Modify: `designer/js/inspector.js` (kind `anchorOut`)
- Test: `designer/tests/registry.test.js`

- [ ] **Step 1: Écrire le test qui échoue**

Ajouter à la fin de `designer/tests/registry.test.js` :

```js
import { ANCHORS_OUT } from '../js/geometry.js';

test('geometry : ANCHORS_OUT = 8 positions sans CENTER', () => {
  assert.equal(ANCHORS_OUT.length, 8);
  assert.equal(ANCHORS_OUT.includes('CENTER'), false);
});

test('registre : bar expose le style de label (couleur/police/align)', () => {
  const keys = COMPONENTS.bar.compFields.map(f => f[0]);
  assert.ok(keys.includes('label_color'));
  assert.ok(keys.includes('label_font'));
  assert.ok(keys.includes('label_align'));
  const d = COMPONENTS.bar.defaults();
  assert.equal(d.label_align, 'TOP_MID');
  assert.equal(d.label_font, 14);
});
```

Note : `import { test }`, `assert` et `COMPONENTS` sont déjà importés en tête de `registry.test.js` ; n'ajouter QUE la ligne `import { ANCHORS_OUT } ...` (en haut du fichier, à côté des autres imports) et les deux `test(...)`.

- [ ] **Step 2: Lancer pour vérifier l'échec**

Run: `cd designer && node --test`
Expected: FAIL — `ANCHORS_OUT` n'est pas exporté + `bar.compFields` n'a pas encore les clés.

- [ ] **Step 3: Exporter `ANCHORS_OUT`**

Dans `designer/js/geometry.js`, juste après la ligne `export const ANCHORS = [...];`, ajouter :

```js
export const ANCHORS_OUT = ['TOP_LEFT','TOP_MID','TOP_RIGHT','LEFT_MID','RIGHT_MID','BOTTOM_LEFT','BOTTOM_MID','BOTTOM_RIGHT'];
```

- [ ] **Step 4: Étendre le registre `bar`**

Dans `designer/js/registry.js`, remplacer le bloc `bar.defaults` et `bar.compFields` :

```js
    defaults: () => ({ type: 'bar', label: 'Bar', min: 0, max: 100, color: '#38BDF8' }),
```
par :
```js
    defaults: () => ({ type: 'bar', label: 'Bar', min: 0, max: 100, color: '#38BDF8', label_color: '#9AA0AA', label_font: 14, label_align: 'TOP_MID' }),
```

et :
```js
    compFields: [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color'], ['bind', 'Variable (pull)', 'asciitext']],
```
par :
```js
    compFields: [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color'], ['label_color', 'Couleur label', 'color'], ['label_font', 'Police label', 'font'], ['label_align', 'Alignement label', 'anchorOut'], ['bind', 'Variable (pull)', 'asciitext']],
```

- [ ] **Step 5: Ajouter le kind `anchorOut` à l'inspecteur**

Dans `designer/js/inspector.js`, modifier l'import de `geometry.js` :
```js
import { ANCHORS } from './geometry.js';
```
en :
```js
import { ANCHORS, ANCHORS_OUT } from './geometry.js';
```

Puis dans `makeInput()`, juste après la branche `} else if (kind === 'anchor') { ... }` (le bloc qui itère `ANCHORS`), ajouter :

```js
  } else if (kind === 'anchorOut') {
    el = document.createElement('select');
    for (const a of ANCHORS_OUT) { const o = document.createElement('option'); o.value = a; o.textContent = a; if (a === (value || 'TOP_MID')) o.selected = true; el.appendChild(o); }
    el.addEventListener('change', () => onChange(el.value));
```

- [ ] **Step 6: Lancer pour vérifier le succès**

Run: `cd designer && node --test`
Expected: PASS — 157 tests (155 + 2).

- [ ] **Step 7: Commit**

```bash
git add designer/js/geometry.js designer/js/registry.js designer/js/inspector.js designer/tests/registry.test.js
git commit -m "feat(Rich_Telemetry designer): champs de style de label (couleur/police/align) sur la Bar"
```

---

## Task 5 : Designer — rendu DOM du label autour du track

**Files:**
- Modify: `designer/js/render.js` (`buildBar`)
- Modify: `designer/style.css` (`.w-bar` + `.w-bar-label*`)

Validation au navigateur (convention du projet : `canvas.js`/`render.js` sont « Vérifiés au navigateur »).

- [ ] **Step 1: Réécrire `buildBar`**

Dans `designer/js/render.js`, remplacer la fonction `buildBar` par :

```js
export function buildBar(comp, placement, mock = MOCKS.bar) {
  const wrap = document.createElement('div');
  wrap.className = 'w w-bar';
  const track = document.createElement('div');
  track.className = 'w-bar-track';
  track.style.width  = (placement.width  || 200) + 'px'; // défauts firmware (view.cpp)
  track.style.height = (placement.height || 16)  + 'px';
  const fill = document.createElement('div');
  fill.className = 'w-bar-fill';
  fill.style.width = (barFill(mock.value, comp.min ?? 0, comp.max ?? 100) * 100) + '%';
  fill.style.background = comp.color || '#38BDF8';
  track.appendChild(fill);
  wrap.appendChild(track);                    // track d'abord = référence de taille du wrap
  if (comp.label) {                           // label hors flux (absolu) → ne fausse pas le placement de la barre
    const lbl = document.createElement('div');
    lbl.className = 'w-bar-label w-bar-label--' + (comp.label_align || 'TOP_MID');
    lbl.textContent = comp.label;
    lbl.style.color = comp.label_color || '#9AA0AA';
    lbl.style.fontSize = (comp.label_font || 14) + 'px';
    wrap.appendChild(lbl);
  }
  return wrap;
}
```

- [ ] **Step 2: Réécrire le CSS du label**

Dans `designer/style.css`, remplacer la ligne :

```css
.w-bar-label { font: 14px Montserrat, system-ui, sans-serif; color: #9AA0AA; margin-bottom: 4px; text-align: center; }
```

par :

```css
.w-bar { position: relative; }
.w-bar-label { position: absolute; white-space: nowrap; line-height: 1; font-family: Montserrat, system-ui, sans-serif; pointer-events: none; }
.w-bar-label--TOP_MID      { bottom: 100%; left: 50%; transform: translateX(-50%); margin-bottom: 6px; }
.w-bar-label--TOP_LEFT     { bottom: 100%; left: 0;   margin-bottom: 6px; }
.w-bar-label--TOP_RIGHT    { bottom: 100%; right: 0;  margin-bottom: 6px; }
.w-bar-label--BOTTOM_MID   { top: 100%; left: 50%; transform: translateX(-50%); margin-top: 6px; }
.w-bar-label--BOTTOM_LEFT  { top: 100%; left: 0;  margin-top: 6px; }
.w-bar-label--BOTTOM_RIGHT { top: 100%; right: 0; margin-top: 6px; }
.w-bar-label--LEFT_MID     { right: 100%; top: 50%; transform: translateY(-50%); margin-right: 6px; }
.w-bar-label--RIGHT_MID    { left: 100%;  top: 50%; transform: translateY(-50%); margin-left: 6px; }
```

- [ ] **Step 3: Vérifier au navigateur**

Lancer le designer : depuis `Rich_Telemetry/`, `python3 -m http.server 8000 --bind 127.0.0.1`, ouvrir `http://127.0.0.1:8000/designer/`.
Sélectionner la barre « RAM », et via l'inspecteur faire varier **Couleur label**, **Police label** et **Alignement label**. Vérifier (drag-free) que :
- le label change de couleur et de taille ;
- les 8 positions placent le label autour du track (haut G/C/D, bas G/C/D, gauche/droite milieu) ;
- déplacer l'alignement **ne déplace pas** la barre elle-même sur le canvas (label hors flux).

Capture de contrôle (Playwright ou claude-in-chrome) recommandée : lire `getComputedStyle` du `.w-bar-label` pour 2-3 alignements et confirmer `position:absolute` + couleur/taille appliquées.

- [ ] **Step 4: Vérifier les tests designer (non-régression)**

Run: `cd designer && node --test`
Expected: PASS — 157 tests (le rendu DOM n'est pas couvert par les tests purs, mais aucun ne doit casser).

- [ ] **Step 5: Commit**

```bash
git add designer/js/render.js designer/style.css
git commit -m "feat(Rich_Telemetry designer): rendre le label de la Bar autour du track (8 positions + couleur/police)"
```

---

## Task 6 : Validation intégrée on-device (contrôleur)

**Files:** aucun (vérification). À exécuter par le contrôleur (device branché au Mac, IP `192.168.1.35`).

- [ ] **Step 1: Sauvegarder l'état du device AVANT tout flash**

⚠ `uploadfs` n'est PAS nécessaire ici (on ne touche pas au designer embarqué tant qu'on ne le redéploie pas). Mais le flash applicatif reboote le device.
- Backup layout : `curl --max-time 8 -s http://192.168.1.35/layout -o /tmp/bar_backup_layout.json`
- Backup des images de fond référencées : pour chaque `background_image` du layout, `curl --max-time 12 -s "http://192.168.1.35/bgimage?key=<clé>" -o /tmp/bg_<clé>.565` (cf. mémoire [[feedback-device-validation-workflow]] — ne jamais supposer qu'elles sont absentes).
- Device-check : `python3 tools/device_mac.py check guition_knob` (depuis la racine du monorepo).

- [ ] **Step 2: Flasher l'application**

Run: `./build.sh guition_knob Rich_Telemetry --upload` (depuis la racine du monorepo).
Expected: flash OK (le device-check passe).

- [ ] **Step 3: Pousser un layout de test couvrant les 8 positions + le défaut**

Construire un layout avec plusieurs barres, chacune un `label_align` différent (au moins `TOP_MID`, `BOTTOM_MID`, `LEFT_MID`, `RIGHT_MID`) + une barre **sans** les nouveaux champs (vérifie le défaut rétrocompat), + une barre avec `label_color`/`label_font` distincts. `POST /layout` puis `POST /update` (valeurs de test).

- [ ] **Step 4: Capturer et vérifier**

Run: `curl --max-time 20 -s http://192.168.1.35/screenshot -o /tmp/bar.bmp && sips -s format png /tmp/bar.bmp --out /tmp/bar.png`
Vérifier sur l'image : label bien positionné autour de chaque barre selon son `label_align`, couleur/police appliquées, et la barre sans champs identique à l'ancien rendu (gris 14 au-dessus). Envoyer le PNG à l'utilisateur (`SendUserFile`).

- [ ] **Step 5: Restaurer l'état initial**

- `POST /layout` avec `/tmp/bar_backup_layout.json`.
- Re-`POST /bgimage?key=<clé>` chaque image sauvegardée à l'étape 1.
- Confirmer via `GET /status` (mêmes pages/composants) + screenshot.

- [ ] **Step 6 (optionnel) : Redéployer le designer embarqué**

Si l'on veut que `http://192.168.1.35/designer/` serve la nouvelle UI : `bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs` (depuis le projet), puis **re-POST layout + re-POST toutes les bg images** (⚠ `uploadfs` les efface — cf. Step 1/5).

---

## Self-Review (effectuée)

- **Couverture spec :** schéma (Task 1) ✓, firmware parsing+défauts (Task 2) ✓, firmware rendu OUT+marge (Task 3) ✓, designer registre/inspecteur (Task 4) ✓, designer rendu+fidélité hors-flux (Task 5) ✓, défauts/rétrocompat (Tasks 2/4/5 + Task 6 Step 3) ✓, tests schéma+natif+designer (Tasks 1/2/4) ✓, validation on-device (Task 6) ✓.
- **Cohérence des types/noms :** `label_color`/`label_font`/`label_align` identiques partout (schéma, struct C++, parsing, registre, inspecteur, rendu). `ALIGN_OUT_MAP` indexée par l'enum `Anchor` (ordre vérifié). `anchorOut`/`ANCHORS_OUT` = mêmes 8 noms, sans `CENTER`. Défauts uniformes `#9AA0AA` / `14` / `TOP_MID`.
- **Pas de placeholder :** chaque step montre le code/commande exacts.
