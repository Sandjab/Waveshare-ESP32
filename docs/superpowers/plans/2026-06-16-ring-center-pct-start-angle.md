# Anneau Rich_Telemetry — `center_pct` + `start_angle` — Plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Activer le rendu des deux champs `ring` réservés depuis la v1 — `center_pct` (lecture `valeur + unité` au centre de l'anneau) et `start_angle` (orientation de l'ouverture) — sans casser les layouts existants.

**Architecture:** Le parsing des deux champs existe déjà (`dashboard.cpp`). Tout le travail est dans le rendu LVGL (`src/view.cpp`, fonctions `ring_place_labels` / `build_ring` / branche `COMP_RING` de `view_sync`) plus la mise à jour du contrat partagé (`schema/layout.schema.json`) et du `README.md`. `center_pct` réutilise le slot d'affichage du `pill` (les deux affichent la valeur, donc exclusifs) ; `start_angle` décale l'arc et place les labels par trigonométrie. `start_angle = 0` reproduit exactement le rendu actuel.

**Tech Stack:** C++ / Arduino-ESP32 / LVGL 8.4 / PlatformIO. Tests natifs : Unity (`pio test -e native`). Build firmware : `./build.sh` (racine du monorepo).

---

## Structure des fichiers

| Fichier | Rôle | Action |
|---|---|---|
| `devices/guition_knob/projects/Rich_Telemetry/test/test_core/test_main.cpp` | Tests natifs Unity | Modifier — ajouter 3 tests garde-contrat de parsing |
| `devices/guition_knob/projects/Rich_Telemetry/src/view.cpp` | Rendu LVGL | Modifier — `ring_place_labels`, `build_ring`, `view_sync/COMP_RING`, includes |
| `devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json` | Contrat partagé firmware↔designer | Modifier — descriptions `center_pct`, `start_angle` |
| `devices/guition_knob/projects/Rich_Telemetry/README.md` | Doc projet | Modifier — section anneau |

Tous les chemins ci-dessous sont relatifs à `devices/guition_knob/projects/Rich_Telemetry/`. Toutes les commandes `git`/`build.sh`/`pio` se lancent depuis la **racine du monorepo** sauf indication contraire.

---

## Task 1 : Tests garde-contrat (parsing `center_pct` + `start_angle`)

Les deux champs sont **déjà parsés** (`dashboard.cpp:65` et `:95`) — ces tests ne passent donc pas par une phase rouge : ils **verrouillent le contrat** (si quelqu'un retire le parsing, le build natif casse) et servent de socle vérifié avant de toucher au rendu.

**Files:**
- Modify: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Ajouter le layout de test + les 3 tests**

Insérer juste après la fonction `test_layout_types_and_geom` (vers la ligne 66) :

```cpp
static const char* LAYOUT_RING_OPTS =
  "{\"title\":\"T\",\"background\":\"#000000\","
  "\"components\":{\"g\":{\"type\":\"ring\",\"color\":\"#38BDF8\","
                        "\"center_pct\":true,\"unit\":\"C\"}},"
  "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"g\","
             "\"radius\":140,\"thickness\":16,\"gap_deg\":70,\"start_angle\":90}]}]}";

void test_ring_center_pct_parsed(void) {
    Dashboard d{}; char err[80];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_RING_OPTS, err, sizeof(err)));
    int ig = dash_find(&d, "g");
    TEST_ASSERT_TRUE(d.components[ig].center_pct);
    TEST_ASSERT_EQUAL_STRING("C", d.components[ig].unit);
}
void test_ring_start_angle_parsed(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_RING_OPTS, err, sizeof(err));
    TEST_ASSERT_EQUAL_INT(90, d.pages[0].places[0].start_angle);
}
void test_ring_start_angle_default_zero(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));   // LAYOUT_OK ne définit pas start_angle
    TEST_ASSERT_EQUAL_INT(0, d.pages[0].places[0].start_angle);
}
```

- [ ] **Step 2 : Enregistrer les tests dans le runner**

Dans `int main(...)`, juste après la ligne `RUN_TEST(test_layout_types_and_geom);` :

```cpp
    RUN_TEST(test_ring_center_pct_parsed);
    RUN_TEST(test_ring_start_angle_parsed);
    RUN_TEST(test_ring_start_angle_default_zero);
```

- [ ] **Step 3 : Lancer les tests natifs**

Depuis `devices/guition_knob/projects/Rich_Telemetry/` :

Run: `pio test -e native`
Expected: tous les tests PASS (les 3 nouveaux inclus), `36 Tests 0 Failures`.

- [ ] **Step 4 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: tests garde-contrat parsing center_pct + start_angle"
```

---

## Task 2 : Rendu `center_pct` + `start_angle` (`view.cpp`)

Non couvert par les tests natifs (rendu LVGL, pas compilé en `native`). Vérification = **le firmware compile** ; validation visuelle en Task 4.

**Files:**
- Modify: `src/view.cpp` (includes ; `ring_place_labels` ~41-45 ; `build_ring` ~47-76 ; `view_sync`/`COMP_RING` ~214-225)

- [ ] **Step 1 : Ajouter les includes**

En tête de `src/view.cpp`, après `#include <cstdio>` (ligne 6), ajouter :

```cpp
#include <math.h>
#include "format.h"
```

- [ ] **Step 2 : Réécrire `ring_place_labels` (signature + trigonométrie)**

Remplacer **toute** la fonction `ring_place_labels` (lignes ~41-45) par :

```cpp
// Place les labels d'une couronne autour de son ouverture. L'ouverture est centrée
// sur l'angle (90 + start_angle) en convention LVGL (0=droite, 90=bas, horaire).
// - cap (légende/countdown) : DANS l'ouverture.
// - slot2 : soit la pastille (à l'opposé de l'ouverture), soit la lecture centrale
//   (au centre géométrique) selon slot2_center.
// À rappeler après chaque set_text (LVGL recentre sur la taille réelle du label).
static void ring_place_labels(lv_obj_t* arc, lv_obj_t* cap, lv_obj_t* slot2,
                              const Placement& q, bool slot2_center) {
    const float DEG2RAD = 0.01745329252f;
    int r = q.radius - q.thickness;
    if (cap) {
        float a = (90 + q.start_angle) * DEG2RAD;          // dans l'ouverture
        lv_obj_align_to(cap, arc, LV_ALIGN_CENTER,
                        (int)roundf(r * cosf(a)), (int)roundf(r * sinf(a)));
    }
    if (slot2) {
        if (slot2_center) {
            lv_obj_align_to(slot2, arc, LV_ALIGN_CENTER, 0, 0);   // centre
        } else {
            float a = (270 + q.start_angle) * DEG2RAD;     // opposé de l'ouverture
            lv_obj_align_to(slot2, arc, LV_ALIGN_CENTER,
                            (int)roundf(r * cosf(a)), (int)roundf(r * sinf(a)));
        }
    }
}
```

Note de validation : `start_angle = 0` ⇒ `cap` à `(0, +r)` (bas) et `slot2` pastille à `(0, -r)` (haut) — identique au comportement v1 (`off = radius - thickness`).

- [ ] **Step 3 : Mettre à jour `build_ring` (arc orienté + slot central/pastille)**

Remplacer **toute** la fonction `build_ring` (lignes ~47-76) par :

```cpp
static void build_ring(lv_obj_t* parent, Component& c, Placement& q,
                       lv_obj_t** main, lv_obj_t** cap, lv_obj_t** pill) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, q.radius * 2, q.radius * 2);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(arc, 90 + q.start_angle + q.gap_deg / 2,
                              90 + q.start_angle - q.gap_deg / 2);
    lv_arc_set_range(arc, c.vmin, c.vmax);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
    *main = arc;

    *cap = lv_label_create(parent);
    lv_obj_set_style_text_font(*cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*cap, lv_color_hex(c.color), 0);
    lv_label_set_text(*cap, "");

    if (c.center_pct) {                       // lecture centrale (prioritaire sur la pastille)
        *pill = lv_label_create(parent);
        lv_obj_set_style_text_font(*pill, pick_font(c.font), 0);
        lv_obj_set_style_text_color(*pill, lv_color_hex(c.color), 0);
        lv_label_set_text(*pill, "");
    } else if (c.pill) {                       // pastille de pourcentage (inchangé)
        *pill = lv_label_create(parent);
        lv_obj_set_style_bg_opa(*pill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(*pill, lv_color_hex(c.color), 0);
        lv_obj_set_style_text_color(*pill, lv_color_hex(0x04121A), 0);
        lv_obj_set_style_radius(*pill, 13, 0);
        lv_obj_set_style_pad_hor(*pill, 8, 0); lv_obj_set_style_pad_ver(*pill, 3, 0);
        lv_label_set_text(*pill, "0%");
    }
    ring_place_labels(arc, *cap, (c.center_pct || c.pill) ? *pill : nullptr, q, c.center_pct);
}
```

- [ ] **Step 4 : Mettre à jour la branche `COMP_RING` de `view_sync`**

Remplacer le bloc `case COMP_RING: { … }` (lignes ~214-225) par :

```cpp
                case COMP_RING: {
                    uint32_t col = threshold_color(c.thresholds, c.threshold_count, c.value, c.color);
                    lv_obj_set_style_arc_color(w, lv_color_hex(col), LV_PART_INDICATOR);
                    lv_arc_set_value(w, c.value);
                    if (s_sub1[p][i]) lv_label_set_text(s_sub1[p][i], c.caption);
                    if (s_sub2[p][i]) {
                        if (c.center_pct) {
                            char cb[24]; format_value((double)c.value, c.unit, cb, sizeof(cb));
                            lv_label_set_text(s_sub2[p][i], cb);
                        } else {
                            char pb[8]; snprintf(pb, sizeof(pb), "%ld%%", (long)c.value);
                            lv_label_set_text(s_sub2[p][i], pb);
                            lv_obj_set_style_bg_color(s_sub2[p][i], lv_color_hex(col), 0);
                        }
                    }
                    ring_place_labels(w, s_sub1[p][i], s_sub2[p][i], q, c.center_pct);
                    break;
                }
```

- [ ] **Step 5 : Compiler le firmware**

Run: `./build.sh guition_knob Rich_Telemetry`
Expected: `SUCCESS` (compilation + link OK, aucun warning d'erreur sur `view.cpp`).

- [ ] **Step 6 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/view.cpp
git commit -m "Rich_Telemetry: rendu center_pct (lecture centrale) + start_angle (ouverture orientable)"
```

---

## Task 3 : Contrat partagé (`schema`) + doc (`README`)

**Files:**
- Modify: `schema/layout.schema.json` (descriptions `center_pct` ~127, `start_angle` ~176)
- Modify: `README.md` (section anneau)

- [ ] **Step 1 : Schéma — `center_pct`**

Remplacer la description de `center_pct` (le fichier est en ASCII sans accents — conserver ce style) :

```json
        "center_pct": { "type": "boolean", "description": "Affiche value+unit au centre de l'anneau, en grand (police = font du composant). Exclusif avec pill (center_pct prioritaire). Unites non-ASCII (degre, micro) non rendues." }
```

- [ ] **Step 2 : Schéma — `start_angle`**

Remplacer la description de `start_angle` :

```json
        "start_angle": { "type": "integer", "description": "Oriente l'ouverture : offset en degres, horaire, depuis le bas. 0=bas, 90=gauche, 180=haut, 270=droite. Defaut 0." }
```

- [ ] **Step 3 : README — section anneau**

Remplacer la ligne `- \`center_pct\` : réservé (non rendu en v1).` par :

```markdown
- `center_pct` : `true` → affiche `valeur + unité` au centre de l'anneau, en grand (police = champ `font` du composant, p. ex. `28`). Exclusif avec `pill` (si les deux sont à `true`, `center_pct` gagne). Limite : unités non-ASCII (`°`, `µ`) non rendues — utiliser `%`, `C`, `V`, `rpm`…
```

Puis remplacer la ligne de géométrie `… \`gap_deg\` (angle d'ouverture). L'ouverture est fixée en bas en v1.` par :

```markdown
Géométrie (sur le placement dans `pages`) : `radius`, `thickness`, `gap_deg` (angle d'ouverture), `start_angle` (oriente l'ouverture : offset en degrés horaire depuis le bas — `0`=bas, `90`=gauche, `180`=haut, `270`=droite ; défaut `0`).
```

- [ ] **Step 4 : Vérifier que le schéma reste un JSON valide**

Depuis `devices/guition_knob/projects/Rich_Telemetry/` :

Run: `python3 -m json.tool schema/layout.schema.json > /dev/null && echo OK`
Expected: `OK` (pas d'erreur de parsing).

- [ ] **Step 5 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/schema/layout.schema.json \
        devices/guition_knob/projects/Rich_Telemetry/README.md
git commit -m "Rich_Telemetry: documenter center_pct + start_angle (schema + README)"
```

---

## Task 4 : Validation visuelle sur device

Rendu LVGL → seule la cible valide réellement. Suit le workflow device habituel : build/flash + `curl`, puis contrôle visuel par l'utilisateur. Le device est une **ressource exclusive** (une seule session flashe à la fois). L'IP est en DHCP (variable) → la récupérer via la série au boot ou `GET /status`.

**Files:** aucun (validation seule).

- [ ] **Step 1 : Flasher**

Run: `./build.sh auto Rich_Telemetry --upload`
Expected: upload OK ; au boot la série imprime l'IP WiFi (la noter dans `$IP`).

- [ ] **Step 2 : Pousser un layout de test (center_pct + start_angle + pill ensemble)**

`pill` est mis à `true` en même temps que `center_pct` pour vérifier l'exclusivité (seul le centre doit s'afficher).

```bash
curl -X POST http://$IP/layout -H 'Content-Type: application/json' -d '{
  "title":"RingTest","background":"#0B0B0F",
  "components":{"t":{"type":"ring","color":"#38BDF8","center_pct":true,"pill":true,"unit":"C",
                     "thresholds":[[40,"#22C55E"],[80,"#F59E0B"]]}},
  "pages":[{"name":"t","place":[{"ref":"t","radius":150,"thickness":18,"gap_deg":70,"start_angle":90}]}]}'
```

- [ ] **Step 3 : Pousser une valeur**

```bash
curl -X POST http://$IP/update -H 'Content-Type: application/json' -d '{"t":61}'
```

- [ ] **Step 4 : Contrôle visuel (utilisateur)** — cocher chaque point :
  - Ouverture de l'anneau **à gauche** (start_angle=90), pas en bas.
  - **`61 C`** affiché en grand au **centre** (espace entre valeur et unité).
  - **Aucune pastille** en périphérie (center_pct l'emporte sur pill).
  - Couleur de l'indicateur = vert→orange selon la valeur (threshold).

- [ ] **Step 5 : Non-régression — repousser le layout par défaut (sans `start_angle`)**

Ce JSON est le layout par défaut (`view_default_layout()` dans `view.cpp`) — aucun `start_angle`, donc il valide que « champ absent = `0` = rendu v1 ».

```bash
curl -X POST http://$IP/layout -H 'Content-Type: application/json' -d '{
  "title":"Claude","background":"#0B0B0F","nav":{"wrap":true},
  "components":{
    "w5h":{"type":"ring","color":"#38BDF8","pill":true,"countdown":true},
    "w7d":{"type":"ring","color":"#A78BFA","pill":true,"countdown":true},
    "led":{"type":"led_ring"},"buzz":{"type":"sound"}},
  "pages":[{"name":"usage","place":[
    {"ref":"w5h","radius":176,"thickness":16,"gap_deg":70},
    {"ref":"w7d","radius":141,"thickness":16,"gap_deg":70}]}]}'
```
  - **Contrôle visuel** : les deux anneaux concentriques s'affichent **comme avant** — ouverture en bas, countdown dans l'ouverture, pastille `%` en haut. (Confirme que `start_angle` absent = `0` = rendu v1 inchangé.)

- [ ] **Step 6 : Restaurer / clore**

Aucun commit (validation). Si un layout de test a été persisté, repousser le layout voulu ou laisser l'utilisateur décider de l'état final affiché.

---

## Auto-revue du plan

- **Couverture spec** : `start_angle` orientation (Task 2 Step 3) ✓ ; labels suivent l'ouverture (Task 2 Step 2) ✓ ; `center_pct` = `value`+`unit` via `format_value` (Task 2 Step 4) ✓ ; exclusivité center/pill (Task 2 Step 3-4) ✓ ; rétro-compat `start_angle=0` (Task 1 Step 1 test + note Task 2 Step 2) ✓ ; limite ASCII (Task 3 doc) ✓ ; schema + README (Task 3) ✓ ; vérif device (Task 4) ✓.
- **Placeholders** : aucun — code complet à chaque step.
- **Cohérence des types** : `ring_place_labels(lv_obj_t*, lv_obj_t*, lv_obj_t*, const Placement&, bool)` — même signature aux 3 emplacements (def + 2 appels). `format_value(double, const char*, char*, size_t)` conforme à `format.h`. `c.center_pct` (bool), `q.start_angle` (int16_t), `c.unit` (char[8]) conformes à `dashboard.h`.
