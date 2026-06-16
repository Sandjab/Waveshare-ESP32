# Phase 2b — Vtable de dispatch des composants Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remplacer les trois `switch(c.type)` de dispatch (`apply_one` côté modèle + les deux `switch` de `view.cpp`, build et sync) par des **tables indexées par `CompType`**, sans changer le comportement, pour que « ajouter un type = ajouter une ligne + écrire son rendu ».

**Architecture:** Deux tables physiques, séparées par la frontière native/device. **Modèle** (`dashboard.cpp`, compilé en natif, sans LVGL) : `APPLY[]` de pointeurs `comp_apply_fn` remplace le `switch` d'`apply_one`. **Vue** (`view.cpp`, device, LVGL) : `VIEW[]` de `struct ViewVTable{build,sync}` remplace les deux `switch`. Une seule struct unifiée est impossible : `dashboard.h` doit rester sans type LVGL (build natif), et une instance unique ne peut référencer des fonctions définies dans deux cibles de build distinctes. L'enum `CompType` reste l'index partagé ; une sentinelle `COMP_COUNT` arme un `static_assert` de taille sur chaque table (oubli de ligne = build cassé). Les types physiques (`led_ring`/`sound`) ont `build/sync = nullptr` (rendus par leur tick dédié, sautés par le moteur). Le parse statique reste plat, la `struct Component` reste plate (décision de la spec).

**Tech Stack:** C++17 (gnu++17), PlatformIO (`env:native` Unity + `env:esp32s3` Arduino/LVGL v8.4), ArduinoJson v7.

**Hors scope (à ne pas toucher) :** le `switch(c.type)` de `context_apply` (`dashboard.cpp`, ajouté en Pull P2, postérieur à la spec — non cité par le handoff 2b) ; `dash_tick_countdown` (test mono-type, pas un dispatch) ; `parse_type`/`COMP_NAMES` (déjà table en 2a) ; le parse plat des props. Le track suivant (chart/meter) étendra `context_apply` par un `case` de plus.

**Convention de validation (rappel handoff) :** modèle native-testable (`pio test -e native`) ; rendu LVGL **non** native-testable → flash + contrôle visuel. clangd local signale de faux `ArduinoJson.h/Arduino.h not found` → ignorer, `pio` fait foi. `cd devices/guition_knob/projects/Rich_Telemetry` avant toute commande `pio`.

---

## File Structure

| Fichier | Rôle | Changement 2b |
|---|---|---|
| `src/dashboard.h` | Modèle (enum + structs, sans LVGL) | +1 ligne : sentinelle `COMP_COUNT` en fin d'enum |
| `src/dashboard.cpp` | Modèle (parse, apply, contexte) | `apply_one` `switch` → 6 fns `apply_*` + table `APPLY[]` + `static_assert` |
| `src/view.cpp` | Vue LVGL (build + sync) | 2 `switch` → fns `build_*`/`sync_*` + `struct ViewVTable` + table `VIEW[]` + `static_assert` |
| `test/test_core/test_main.cpp` | Tests natifs du cœur | +2 tests de caractérisation (`apply` de led_ring/sound) + 2 `RUN_TEST` |

Aucun autre fichier n'est touché. Aucun changement de signature publique (`dash_apply_update`, `view_rebuild`, `view_sync` inchangés) → `main.cpp`, `api.cpp`, `net_pull.cpp` intouchés.

---

## Task 1 : Filet de caractérisation pour l'`apply` des types physiques

**Pourquoi :** la refacto réécrit les branches `COMP_LED_RING`/`COMP_SOUND` d'`apply_one`, aujourd'hui **sans aucune couverture native**. On verrouille leur comportement actuel AVANT de bouger le code (caractérisation : ces tests passent contre le code actuel, puis devront rester verts après la refacto). Les types `label/readout/bar/ring` sont déjà couverts par `test_update_*`.

**Files:**
- Modify: `test/test_core/test_main.cpp` (ajout de 2 fns + 2 `RUN_TEST` dans `main`)

- [ ] **Step 1 : Ajouter les deux tests de caractérisation**

Insérer juste avant `void test_next_mid(void)` (≈ l.220) dans `test/test_core/test_main.cpp` :

```cpp
// --- apply des types physiques (caracterisation : verrouille le comportement avant la refacto 2b) ---
void test_update_led_ring_mode_color_value(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    const char* L = "{\"components\":{\"led\":{\"type\":\"led_ring\"}},\"pages\":[]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, L, err, sizeof(err)));
    dash_apply_update(&d,
        "{\"led\":{\"mode\":\"progress\",\"color\":\"#FF8800\",\"value\":42,\"period_ms\":500}}",
        unk, sizeof(unk));
    int i = dash_find(&d, "led");
    TEST_ASSERT_EQUAL_INT(LED_PROGRESS, d.components[i].led_mode);
    TEST_ASSERT_EQUAL_HEX32(0xFF8800, d.components[i].led_color);
    TEST_ASSERT_EQUAL_UINT8(42, d.components[i].led_value);
    TEST_ASSERT_EQUAL_UINT16(500, d.components[i].led_period_ms);
}
void test_update_sound_sets_pending(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    const char* L = "{\"components\":{\"buzz\":{\"type\":\"sound\"}},\"pages\":[]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, L, err, sizeof(err)));
    dash_apply_update(&d, "{\"buzz\":{\"tone\":880,\"ms\":200,\"name\":\"beep\"}}", unk, sizeof(unk));
    int i = dash_find(&d, "buzz");
    TEST_ASSERT_TRUE(d.components[i].snd_pending);
    TEST_ASSERT_EQUAL_UINT16(880, d.components[i].snd_tone);
    TEST_ASSERT_EQUAL_UINT16(200, d.components[i].snd_ms);
    TEST_ASSERT_EQUAL_STRING("beep", d.components[i].snd_name);
}
```

- [ ] **Step 2 : Enregistrer les deux tests**

Dans `main()`, après `RUN_TEST(test_update_unknown_reported_not_applied);` (≈ l.414) :

```cpp
    RUN_TEST(test_update_led_ring_mode_color_value);
    RUN_TEST(test_update_sound_sets_pending);
```

- [ ] **Step 3 : Lancer les tests natifs — tout doit être vert (baseline)**

Run : `cd devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio test -e native`
Expected : `61 Tests 0 Failures 0 Ignored` puis `PASSED` (59 existants + 2 nouveaux ; ils décrivent le comportement actuel donc passent immédiatement).

- [ ] **Step 4 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: 2b — tests de caracterisation apply led_ring/sound (filet refacto)"
```

---

## Task 2 : Modèle — `apply_one` `switch` → table `APPLY[]`

**Files:**
- Modify: `src/dashboard.h:7` (enum `CompType` : ajouter `COMP_COUNT`)
- Modify: `src/dashboard.cpp:137-180` (remplacer la fonction `apply_one`)

- [ ] **Step 1 : Ajouter la sentinelle `COMP_COUNT` à l'enum**

Dans `src/dashboard.h`, remplacer la ligne 7 :

```cpp
enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND };
```

par :

```cpp
enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND, COMP_COUNT };
```

`COMP_COUNT` (=7) est une borne, jamais un type réel : `parse_type` ne le renvoie pas, le schema ne le déclare pas, les `switch` restants (`context_apply`, `dash_tick_countdown`) ont un `default`/test d'égalité → non impactés.

- [ ] **Step 2 : Remplacer `apply_one` par 6 fonctions + table + dispatch**

Dans `src/dashboard.cpp`, remplacer **tout le bloc de la fonction `apply_one`** (de `static void apply_one(Component& c, JsonVariantConst v) {` ligne 137 jusqu'à son `}` ligne 180 inclus) par :

```cpp
// Vtable modèle : un handler /update par type, indexé par CompType. Chaque branche est
// l'ancien `case` d'apply_one, à l'identique. Ajouter un type = une fn + une ligne de table.
typedef void (*comp_apply_fn)(Component&, JsonVariantConst);

static void apply_label(Component& c, JsonVariantConst v) {
    strlcpy(c.vstr, v.as<const char*>() ? v.as<const char*>() : c.vstr, sizeof(c.vstr));
}
static void apply_readout(Component& c, JsonVariantConst v) {
    if (v.is<const char*>()) strlcpy(c.vstr, v.as<const char*>(), sizeof(c.vstr));
    else format_value(v.as<double>(), c.unit, c.vstr, sizeof(c.vstr));
}
static void apply_bar(Component& c, JsonVariantConst v) {
    c.value = v.as<int>();
}
static void apply_ring(Component& c, JsonVariantConst v) {
    c.value      = v["pct"] | c.value;
    c.reset_in_s = v["reset_in_s"] | c.reset_in_s;
    if (v["caption"].is<const char*>()) {
        strlcpy(c.caption, v["caption"].as<const char*>(), sizeof(c.caption));
    } else if (c.countdown) {
        format_remaining(c.reset_in_s, c.caption, sizeof(c.caption));
    }
}
static void apply_led_ring(Component& c, JsonVariantConst v) {
    const char* m = v["mode"] | "";
    if      (!strcmp(m,"off"))      c.led_mode = LED_OFF;
    else if (!strcmp(m,"solid"))    c.led_mode = LED_SOLID;
    else if (!strcmp(m,"progress")) c.led_mode = LED_PROGRESS;
    else if (!strcmp(m,"spinner"))  c.led_mode = LED_SPINNER;
    else if (!strcmp(m,"blink"))    c.led_mode = LED_BLINK;
    else if (!strcmp(m,"breathe"))  c.led_mode = LED_BREATHE;
    if (v["color"].is<const char*>()) c.led_color = parse_hex_color(v["color"], c.led_color);
    c.led_value      = v["value"]      | c.led_value;
    c.led_brightness = v["brightness"] | c.led_brightness_cfg;
    c.led_period_ms  = v["period_ms"]  | (c.led_period_ms ? c.led_period_ms : 1000);
}
static void apply_sound(Component& c, JsonVariantConst v) {
    c.snd_pending = true;
    c.snd_tone = v["tone"] | 0;
    c.snd_ms   = v["ms"]   | 150;
    strlcpy(c.snd_name, v["name"] | "", sizeof(c.snd_name));
}

static const comp_apply_fn APPLY[] = {
    /* COMP_NONE     */ nullptr,
    /* COMP_LABEL    */ apply_label,
    /* COMP_READOUT  */ apply_readout,
    /* COMP_BAR      */ apply_bar,
    /* COMP_RING     */ apply_ring,
    /* COMP_LED_RING */ apply_led_ring,
    /* COMP_SOUND    */ apply_sound,
};
static_assert(sizeof(APPLY) / sizeof(APPLY[0]) == COMP_COUNT,
              "APPLY desync avec CompType : ajoute la ligne du nouveau type");

static void apply_one(Component& c, JsonVariantConst v) {
    if (c.type > COMP_NONE && (unsigned)c.type < COMP_COUNT && APPLY[c.type])
        APPLY[c.type](c, v);
}
```

`apply_one` garde son nom et sa signature → `dash_apply_update` (l.194, `apply_one(d->components[ci], kv.value());`) est inchangé.

- [ ] **Step 3 : Lancer les tests natifs — tout doit rester vert**

Run : `cd devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio test -e native`
Expected : `61 Tests 0 Failures 0 Ignored` / `PASSED`. (Refacto à comportement constant : les 4 types couverts par `test_update_*`/`test_ctxapply_*` + les 2 physiques de la Task 1 confirment la fidélité.)

- [ ] **Step 4 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/dashboard.h \
        devices/guition_knob/projects/Rich_Telemetry/src/dashboard.cpp
git commit -m "Rich_Telemetry: 2b — apply_one switch -> table APPLY[] (modele, COMP_COUNT)"
```

---

## Task 3 : Vue — les deux `switch` de `view.cpp` → table `VIEW[]`

**Files:**
- Modify: `src/view.cpp` (ajout des fns `build_*`/`sync_*` + `struct ViewVTable` + table `VIEW[]` avant `view_rebuild` ; remplacement des deux `switch` par un dispatch de table)

Note : `build_ring` (l.68) a **déjà** la signature voulue (`lv_obj_t*, Component&, Placement&, lv_obj_t**, lv_obj_t**, lv_obj_t**`) → il est utilisé tel quel dans la table, sans le réécrire. `ring_place_labels` (l.47) et `pick_font` (l.21) restent inchangés.

- [ ] **Step 1 : Ajouter les fns de vue + la table, juste avant `view_rebuild`**

Insérer le bloc suivant **entre la fin de `build_ring` (l.113, juste après son `}`) et le commentaire `// Swipe -> navigation.` (l.115)** :

```cpp
// build/sync extraits des anciens switch de view_rebuild/view_sync, à l'identique.
// Signature commune : 3 slots LVGL (main + 2 sous-objets) car ring/bar sont multi-objets.
static void build_text(lv_obj_t* parent, Component& c, Placement& q,
                       lv_obj_t** main, lv_obj_t**, lv_obj_t**) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, pick_font(c.font), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(c.color), 0);
    lv_label_set_text(l, "");
    lv_obj_align(l, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = l;
}
static void build_bar(lv_obj_t* parent, Component& c, Placement& q,
                      lv_obj_t** main, lv_obj_t** sub1, lv_obj_t**) {
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_set_size(b, q.width ? q.width : 200, q.height ? q.height : 16);
    lv_bar_set_range(b, c.vmin, c.vmax);
    lv_obj_set_style_bg_color(b, lv_color_hex(c.color), LV_PART_INDICATOR);
    lv_obj_align(b, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = b;
    if (c.label[0]) {
        lv_obj_t* bl = lv_label_create(parent);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(bl, lv_color_hex(0x9AA0AA), 0);
        lv_label_set_text(bl, c.label);
        lv_obj_align_to(bl, b, LV_ALIGN_OUT_TOP_MID, 0, -6);
        *sub1 = bl;
    }
}

static void sync_label(Component& c, Placement&, lv_obj_t* w, lv_obj_t*, lv_obj_t*) {
    lv_label_set_text(w, c.vstr);
}
static void sync_readout(Component& c, Placement&, lv_obj_t* w, lv_obj_t*, lv_obj_t*) {
    if (c.label[0]) {
        char rb[TEXT_LEN * 2];
        snprintf(rb, sizeof(rb), "%s %s", c.label, c.vstr);
        lv_label_set_text(w, rb);
    } else {
        lv_label_set_text(w, c.vstr);
    }
}
static void sync_bar(Component& c, Placement&, lv_obj_t* w, lv_obj_t*, lv_obj_t*) {
    lv_bar_set_value(w, c.value, LV_ANIM_OFF);
}
static void sync_ring(Component& c, Placement& q, lv_obj_t* w, lv_obj_t* sub1, lv_obj_t* sub2) {
    uint32_t col = threshold_color(c.thresholds, c.threshold_count, c.value, c.color);
    lv_obj_set_style_arc_color(w, lv_color_hex(col), LV_PART_INDICATOR);
    lv_arc_set_value(w, c.value);
    if (sub1) lv_label_set_text(sub1, c.caption);
    if (sub2) {
        if (c.center_pct) {
            char cb[24]; format_value((double)c.value, c.unit, cb, sizeof(cb));
            lv_label_set_text(sub2, cb);
            uint32_t ccol = c.center_color_set ? c.center_color : col;  // surcharge explicite, sinon suit le seuil
            lv_obj_set_style_text_color(sub2, lv_color_hex(ccol), 0);
        } else {
            char pb[8]; snprintf(pb, sizeof(pb), "%ld%%", (long)c.value);
            lv_label_set_text(sub2, pb);
            lv_obj_set_style_bg_color(sub2, lv_color_hex(col), 0);
        }
    }
    ring_place_labels(w, sub1, sub2, q, c.center_pct);
}

// Vtable vue indexée par CompType. Types physiques (led_ring/sound) : build/sync = nullptr
// (rendus par leur tick dédié -> le moteur les saute). label/readout partagent build_text.
struct ViewVTable {
    void (*build)(lv_obj_t* parent, Component& c, Placement& q,
                  lv_obj_t** main, lv_obj_t** sub1, lv_obj_t** sub2);
    void (*sync)(Component& c, Placement& q,
                 lv_obj_t* main, lv_obj_t* sub1, lv_obj_t* sub2);
};
static const ViewVTable VIEW[] = {
    /* COMP_NONE     */ { nullptr,    nullptr      },
    /* COMP_LABEL    */ { build_text, sync_label   },
    /* COMP_READOUT  */ { build_text, sync_readout },
    /* COMP_BAR      */ { build_bar,  sync_bar     },
    /* COMP_RING     */ { build_ring, sync_ring    },
    /* COMP_LED_RING */ { nullptr,    nullptr      },
    /* COMP_SOUND    */ { nullptr,    nullptr      },
};
static_assert(sizeof(VIEW) / sizeof(VIEW[0]) == COMP_COUNT,
              "VIEW desync avec CompType : ajoute la ligne du nouveau type");
```

- [ ] **Step 2 : Remplacer le `switch` de construction dans `view_rebuild`**

Dans `view_rebuild`, remplacer la boucle interne (de `for (int i = 0; i < d->pages[p].place_count; i++) {` ≈ l.150 jusqu'au `}` fermant cette boucle ≈ l.186) — c.-à-d. tout le `switch (c.type) { ... }` et son contour — par :

```cpp
        for (int i = 0; i < d->pages[p].place_count; i++) {
            Placement& q = d->pages[p].places[i];
            Component& c = d->components[q.comp_index];
            if ((unsigned)c.type < COMP_COUNT && VIEW[c.type].build)
                VIEW[c.type].build(cont, c, q, &s_widget[p][i], &s_sub1[p][i], &s_sub2[p][i]);
        }
```

- [ ] **Step 3 : Remplacer le `switch` de mise à jour dans `view_sync`**

Dans `view_sync`, remplacer la boucle interne (de `for (int i = 0; i < d->pages[p].place_count; i++) {` ≈ l.228 jusqu'au `}` fermant cette boucle ≈ l.273), qui contient `if (!c.dirty) continue; ... switch (c.type) { ... }`, par :

```cpp
        for (int i = 0; i < d->pages[p].place_count; i++) {
            Placement& q = d->pages[p].places[i];
            Component& c = d->components[q.comp_index];
            if (!c.dirty) continue;
            lv_obj_t* w = s_widget[p][i];
            if (!w) continue;
            if ((unsigned)c.type < COMP_COUNT && VIEW[c.type].sync)
                VIEW[c.type].sync(c, q, w, s_sub1[p][i], s_sub2[p][i]);
        }
```

Le `if (!w) continue;` est conservé : un type physique a `s_widget == nullptr` (jamais construit) et serait de toute façon sauté par `VIEW[...].sync == nullptr`. La remise à zéro des `dirty` après la double boucle (l.275-276) reste inchangée.

- [ ] **Step 4 : Compiler le firmware (rendu non native-testable)**

Run : `cd devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio run -e esp32s3`
Expected : `SUCCESS`. (Repère du handoff P2 : RAM ~42.7 %, Flash ~25.0 % — un écart marqué signalerait un problème.)

- [ ] **Step 5 : Vérifier que le natif reste vert (non-régression du modèle)**

Run : `cd devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio test -e native`
Expected : `61 Tests 0 Failures 0 Ignored` / `PASSED`. (`view.cpp` n'est pas dans `build_src_filter` natif ; ce run confirme juste que rien du modèle n'a régressé.)

- [ ] **Step 6 : Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/view.cpp
git commit -m "Rich_Telemetry: 2b — switch build/sync de view.cpp -> table VIEW[]"
```

---

## Task 4 : Validation sur device — non-régression visuelle des 6 types

**Pré-requis :** carte Guition `knobGuitionNoir1` branchée (`/dev/cu.usbmodem8301`). Workflow handoff : le contrôleur flashe + série + `curl`, l'utilisateur valide le visuel.

- [ ] **Step 1 : Flasher**

Run (depuis la racine du repo) : `./build.sh auto Rich_Telemetry --upload`
Expected : build + flash OK ; le garde-fou MAC passe (device = guition_knob).

- [ ] **Step 2 : Récupérer l'IP DHCP au boot via la série**

Lire `IP=` au boot (handoff : pulse DTR/RTS pour rebooter et capter la ligne) avec `~/.platformio/penv/bin/python` + pyserial sur `/dev/cu.usbmodem8301` @115200. Noter l'IP (dernière connue : `192.168.1.35`, peut changer). Vérifier `curl -s http://<IP>/status` répond (uptime_s croissant, pas de crash).

- [ ] **Step 3 : Pousser un layout exerçant les 6 types**

```bash
curl -s -X POST http://<IP>/layout -H 'Content-Type: application/json' -d '{
  "title":"2b","background":"#0B0B0F",
  "components":{
    "r1":{"type":"ring","color":"#38BDF8","pill":true,"center_pct":true,"unit":"%",
          "thresholds":[[70,"#22C55E"],[90,"#F59E0B"]]},
    "ro":{"type":"readout","label":"CPU","unit":"%"},
    "lb":{"type":"label","color":"#E5E7EB"},
    "br":{"type":"bar","color":"#22C55E","label":"RAM","min":0,"max":100},
    "led":{"type":"led_ring"},
    "snd":{"type":"sound"}
  },
  "pages":[{"name":"main","place":[
    {"ref":"r1","radius":150,"thickness":16,"gap_deg":70},
    {"ref":"ro","anchor":"TOP_MID","dy":40},
    {"ref":"lb","anchor":"BOTTOM_MID","dy":-46},
    {"ref":"br","anchor":"CENTER","dy":78,"width":200,"height":16}
  ]}]
}'
```
Expected : `{"ok":true}`.

- [ ] **Step 4 : Pousser des valeurs sur les 6 types**

```bash
curl -s -X POST http://<IP>/update -H 'Content-Type: application/json' -d '{
  "r1":{"pct":63},
  "ro":42,
  "lb":"hello",
  "br":75,
  "led":{"mode":"solid","color":"#A78BFA"},
  "snd":{"tone":880,"ms":150}
}'
```
Expected : `{"ok":true,"updated":6}`.

- [ ] **Step 5 : Contrôle visuel (utilisateur)**

Critère de succès — chaque type rend **comme avant la refacto** :
- **ring** `r1` : arc à 63 %, couleur de seuil (vert <70), `%` central « 63 % », pastille pourcentage.
- **readout** `ro` : « CPU 42 % » en haut.
- **label** `lb` : « hello » en bas.
- **bar** `br` : barre verte à 75 % avec libellé « RAM » au-dessus, centrée légèrement bas.
- **led_ring** (anneau RGB physique) : allumé violet fixe.
- **sound** (buzzer) : un bip à l'envoi du `/update`.

Tout écart de rendu/placement vs le comportement connu = régression à corriger avant de clore la 2b.

- [ ] **Step 6 : Restaurer le layout d'usine**

Le device doit repartir dans l'état connu du handoff (layout `view_default_layout`, sans `sources`). Le plus simple : `curl -s -X POST http://<IP>/page` n'aide pas — re-pousser le défaut. Récupérer la chaîne exacte de `view_default_layout()` (`src/view.cpp:29-39`) et la POSTer sur `/layout`, **ou** flasher à nouveau après avoir effacé la persistance. Vérifier `curl -s http://<IP>/status` → `components:4`, `sources:[]`.

---

## Self-Review

**1. Couverture de la spec (Phase 2 = vtable) :**
- « `switch` d'`apply_one` → vtable » → Task 2 (table `APPLY[]`). ✅
- « les 2 `switch(c.type)` de `view.cpp` (build + sync) → vtable » → Task 3 (table `VIEW[]`). ✅
- « types physiques `build/sync = NULL`, le moteur saute les entrées nulles » → entrées `{nullptr,nullptr}` + gardes `if (...build)` / `if (...sync)`. ✅
- « parse statique reste plat, `struct Component` reste plate » → non touchés. ✅
- « `parse_type` → table » = déjà fait en 2a → hors de ce plan (noté). ✅
- Conformité Phase 2 : `test_schema_types_all_resolve` (existant) reste vert ; `static_assert` ajoute un garde-fou compile-time « oubli de ligne ». ✅
- Note de déviation assumée vs le croquis de la spec (1 struct unique) : 2 tables, justifié par la frontière native/device (documenté dans Architecture). `context_apply` hors scope (postérieur à la spec) : documenté.

**2. Scan placeholders :** aucun TODO/TBD ; chaque step de code montre le code complet ; les commandes ont leur sortie attendue. ✅

**3. Cohérence des types/signatures :**
- `comp_apply_fn` = `void(*)(Component&, JsonVariantConst)` — identique partout (typedef, 6 fns, table). ✅
- `ViewVTable.build` = `void(*)(lv_obj_t*, Component&, Placement&, lv_obj_t**, lv_obj_t**, lv_obj_t**)` — `build_text`/`build_bar` la respectent ; `build_ring` (existant) a exactement cette signature (noms de params différents, types identiques → assignable). ✅
- `ViewVTable.sync` = `void(*)(Component&, Placement&, lv_obj_t*, lv_obj_t*, lv_obj_t*)` — `sync_label`/`sync_readout`/`sync_bar`/`sync_ring` la respectent. ✅
- `COMP_COUNT` (=7) == nombre d'entrées de `APPLY[]` et `VIEW[]` (7 chacune, indices `COMP_NONE..COMP_SOUND`) → `static_assert` passe. ✅
- `apply_one` garde nom+signature → `dash_apply_update` inchangé ; `view_rebuild`/`view_sync` gardent leur signature → `main.cpp`/`api.cpp` inchangés. ✅
