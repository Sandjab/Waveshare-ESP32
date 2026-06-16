# Composants `chart` + `meter` (firmware) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ajouter deux types d'affichage LVGL v8.4 — `chart` (sparkline d'historique) et `meter` (jauge à aiguille + zones d'arc) — comme **premiers nouveaux types via la vtable** (`APPLY[]`/`VIEW[]`) issue de la Phase 2b.

**Architecture:** `chart` pousse un **scalaire** par `/update` ; l'**historique vit dans le modèle** (`Component`) sous forme d'une fenêtre glissante de N points, recopiée dans la série LVGL à chaque `view_sync` (mirroir idempotent — `lv_chart_set_next_value` ne l'est pas). `meter` pousse un scalaire → aiguille ; il **réutilise** `vmin`/`vmax`/`value`/`thresholds` (zones d'arc). Les deux s'ajoutent par : valeur d'enum, ligne `COMP_NAMES`, champs de struct (chart only), lecture du parse plat, ligne `APPLY[]`, ligne `VIEW[]`, branche `context_apply`. La `struct Component` reste plate.

**Tech Stack:** C++17, PlatformIO (`env:native` Unity + `env:esp32s3` Arduino/LVGL **v8.4**), ArduinoJson v7.

**Séquencement firmware-d'abord (précédent du projet) :** le **schema `layout.schema.json` et le designer ne sont PAS touchés ici**. Comme `sources`/`bind` aujourd'hui, le firmware parse `chart`/`meter` avant que le schema ne les déclare ; le test de conformité natif ne couvre que les *types du schema* (les 6 d'origine), donc ajouter `chart`/`meter` à `COMP_NAMES` ne le casse pas, et le test de conformité **JS du designer reste vert** (le schema inchangé). **P3 (designer) ajoutera `comp_chart`/`comp_meter` au schema + `registry.js` ensemble**, réconciliant les deux côtés. Ne PAS éditer `schema/` ni `designer/` dans ce plan.

**Décisions de design prises ici (au-delà de la spec, à signaler) :**
- **Historique = fenêtre glissante** (`memmove` d'un buffer `int16_t hist[CHART_MAX_POINTS]` + `hist_count`), pas un index circulaire. Comportement observable identique (garder les N derniers, ordre chronologique), code trivialement native-testable et mirroir direct vers `y_points`. `memmove` de ≤60 `int16` par `/update` = négligeable.
- **`chart` lié (`bind`) : append uniquement au changement de valeur.** `context_apply` tourne toutes les 100 ms ; appender à chaque tick noierait le buffer. On compare à `c.value` (dernière valeur appendée) et on n'append que si différent. Le **push explicite** `/update {chart:v}` append **toujours** (un POST = un point voulu), même valeur répétée. Sémantiques volontairement différentes (polled vs explicite).
- **`meter` géométrie placeable** (anchor + width/height), pas forcé-centré. Échelle 270° (`angle_range=270`, `rotation=135`) ouverte en bas.
- **Handle aiguille du meter stocké dans le slot `s_sub1`** (cast via `void*`) : `lv_meter` n'a pas de getter d'indicateur, contrairement au chart (`lv_chart_get_series_next`). Le slot existe pour ça (jamais déréférencé comme `lv_obj_t`).

**Convention de validation (rappel) :** modèle native-testable ; rendu LVGL non native-testable → flash + visuel. clangd local = faux positifs (`lvgl.h/ArduinoJson.h not found`) → ignorer, `pio` fait foi. `cd devices/guition_knob/projects/Rich_Telemetry` avant les commandes `pio`.

---

## File Structure

| Fichier | Changement |
|---|---|
| `src/config.h` | +1 ligne : `#define CHART_MAX_POINTS 60` |
| `src/dashboard.h` | enum `CompType` : +`COMP_CHART`,`COMP_METER` (avant `COMP_COUNT`) ; struct `Component` : +`int chart_points` (config) +`int16_t hist[CHART_MAX_POINTS]`/`int hist_count` (état) |
| `src/dashboard.cpp` | `COMP_NAMES` +2 ; parse `points` ; helper `chart_push` ; `apply_chart`/`apply_meter` ; `APPLY[]` +2 ; `context_apply` +2 branches |
| `src/lv_conf.h` | activer `LV_USE_CHART 1` + `LV_USE_METER 1` |
| `src/view.cpp` | `build_chart`/`sync_chart`/`build_meter`/`sync_meter` ; `VIEW[]` +2 |
| `test/test_core/test_main.cpp` | +5 tests modèle (ring buffer, parse points, apply meter, ctx chart/meter) + RUN_TEST |

`MAX_COMPONENTS=32` × `int16_t[60]` (+ counts) ≈ +4 Ko par `Dashboard`, ×2 instances ≈ +8 Ko RAM. Acceptable (RAM à 42.7 %). Aucune signature publique ne change.

---

## Task 1 : Modèle + tests natifs (chart history, parse, apply, context)

**⚠ Build esp32s3 cassé entre Task 1 et Task 2 — attendu.** Ajouter `COMP_CHART`/`COMP_METER` à l'enum porte `COMP_COUNT` à 9 ; la table `VIEW[]` (dans `view.cpp`, **non** compilée en natif) en a encore 7 → son `static_assert` casserait `pio run -e esp32s3`. **Ne PAS lancer `pio run -e esp32s3` dans cette task** ; il est réparé en Task 2. La table `APPLY[]` (modèle), elle, est mise à 9 ici → natif OK.

**Files:**
- Modify: `src/config.h`, `src/dashboard.h`, `src/dashboard.cpp`, `test/test_core/test_main.cpp`

- [ ] **Step 1 : `CHART_MAX_POINTS` dans config.h**

Dans `src/config.h`, ajouter après `#define MAX_THRESHOLDS          4` :

```cpp
#define CHART_MAX_POINTS        60
```

- [ ] **Step 2 : enum + champs de struct (dashboard.h)**

Dans `src/dashboard.h`, remplacer la ligne de l'enum :

```cpp
enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND, COMP_COUNT };
```

par (insérer `COMP_CHART, COMP_METER` avant `COMP_COUNT`) :

```cpp
enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND, COMP_CHART, COMP_METER, COMP_COUNT };
```

Dans `struct Component`, ajouter `chart_points` à la fin du bloc `// --- config ---` (juste après la ligne `char bind[ID_LEN];`) :

```cpp
    int      chart_points;           // chart : longueur de la fenêtre d'historique (défaut 30, borné CHART_MAX_POINTS)
```

Et ajouter le buffer d'historique à la fin du bloc `// --- etat (modifie par /update) ---` (juste après la ligne `bool snd_pending; ...`) :

```cpp
    int16_t  hist[CHART_MAX_POINTS]; int hist_count;   // chart : fenêtre glissante, hist[0..hist_count-1] = chronologique
```

- [ ] **Step 3 : COMP_NAMES + parse `points` (dashboard.cpp)**

Dans `src/dashboard.cpp`, ajouter `chart`/`meter` à `COMP_NAMES` (l'ordre n'importe pas, c'est un lookup nom→type) :

```cpp
static const struct { const char* name; CompType type; } COMP_NAMES[] = {
    { "label",    COMP_LABEL    }, { "readout",  COMP_READOUT  }, { "bar",   COMP_BAR   },
    { "ring",     COMP_RING     }, { "led_ring", COMP_LED_RING }, { "sound", COMP_SOUND },
    { "chart",    COMP_CHART    }, { "meter",    COMP_METER    },
};
```

Dans `dash_set_layout`, dans la boucle de parse des composants, ajouter la lecture de `points` juste après la ligne `strlcpy(c.bind, o["bind"] | "", sizeof(c.bind));` :

```cpp
        c.chart_points = o["points"] | 30;
        if (c.chart_points > CHART_MAX_POINTS) c.chart_points = CHART_MAX_POINTS;
        if (c.chart_points < 1)                c.chart_points = 1;
```

- [ ] **Step 4 : helper `chart_push` + `apply_chart`/`apply_meter` + table (dashboard.cpp)**

Dans `src/dashboard.cpp`, juste **avant** `typedef void (*comp_apply_fn)(Component&, JsonVariantConst);`, ajouter le helper partagé :

```cpp
// Fenêtre glissante d'historique du chart : garde les chart_points dernières valeurs,
// hist[0..hist_count-1] en ordre chronologique. Utilisé par /update (apply_chart) et bind (context_apply).
static void chart_push(Component& c, int16_t v) {
    int n = c.chart_points;
    if (n > CHART_MAX_POINTS) n = CHART_MAX_POINTS;
    if (n < 1) n = 1;
    if (c.hist_count < n) {
        c.hist[c.hist_count++] = v;
    } else {
        memmove(c.hist, c.hist + 1, (size_t)(n - 1) * sizeof(int16_t));
        c.hist[n - 1] = v;
    }
}
```

Ajouter les deux handlers `apply_*` juste après `apply_sound` (avant la table `APPLY[]`) :

```cpp
static void apply_chart(Component& c, JsonVariantConst v) {
    chart_push(c, (int16_t)v.as<int>());      // push explicite : toujours un point
}
static void apply_meter(Component& c, JsonVariantConst v) {
    c.value = v.as<int>();                    // scalaire -> aiguille (comme bar)
}
```

Ajouter les deux lignes à la table `APPLY[]`, après `/* COMP_SOUND */ apply_sound,` :

```cpp
    /* COMP_CHART    */ apply_chart,
    /* COMP_METER    */ apply_meter,
```

(Le `static_assert(... == COMP_COUNT ...)` existant passe : 9 entrées == `COMP_COUNT`.)

- [ ] **Step 5 : `context_apply` — bind pour chart/meter (dashboard.cpp)**

Dans `context_apply`, dans le `switch (c.type)`, ajouter ces deux `case` juste avant `default: break;` :

```cpp
            case COMP_METER:                            // scalaire -> aiguille (comme bar)
                if (v.type == CTX_NUM) {
                    int32_t nv = (int32_t)v.num;
                    if (c.value != nv) { c.value = nv; changed = true; }
                }
                break;
            case COMP_CHART:                            // append SEULEMENT au changement (évite le flood du tick 100 ms)
                if (v.type == CTX_NUM) {
                    int32_t nv = (int32_t)v.num;
                    if (c.value != nv) { chart_push(c, (int16_t)nv); c.value = nv; changed = true; }
                }
                break;
```

- [ ] **Step 6 : tests natifs du modèle (test_main.cpp)**

Insérer ces 5 tests juste avant `void test_next_mid(void)` :

```cpp
// --- chart : fenêtre glissante d'historique (native-testable) ---
void test_chart_ring_keeps_last_n(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d,
        "{\"components\":{\"g\":{\"type\":\"chart\",\"points\":30}},"
        "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"g\"}]}]}", err, sizeof(err));
    int i = dash_find(&d, "g");
    char body[24];
    for (int v = 1; v <= 35; v++) { snprintf(body, sizeof(body), "{\"g\":%d}", v); dash_apply_update(&d, body, unk, sizeof(unk)); }
    TEST_ASSERT_EQUAL_INT(30, d.components[i].hist_count);
    TEST_ASSERT_EQUAL_INT(6,  d.components[i].hist[0]);    // v1..v5 sont tombées
    TEST_ASSERT_EQUAL_INT(35, d.components[i].hist[29]);
}
void test_chart_points_parsed_and_clamped(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, "{\"components\":{\"g\":{\"type\":\"chart\",\"points\":999}},\"pages\":[]}", err, sizeof(err));
    TEST_ASSERT_EQUAL_INT(CHART_MAX_POINTS, d.components[dash_find(&d,"g")].chart_points);
    Dashboard d2{}; char err2[80];
    dash_set_layout(&d2, "{\"components\":{\"g\":{\"type\":\"chart\"}},\"pages\":[]}", err2, sizeof(err2));
    TEST_ASSERT_EQUAL_INT(30, d2.components[dash_find(&d2,"g")].chart_points);
}
void test_update_meter_value(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d,
        "{\"components\":{\"m\":{\"type\":\"meter\",\"min\":0,\"max\":100}},"
        "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"m\"}]}]}", err, sizeof(err));
    dash_apply_update(&d, "{\"m\":72}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(72, d.components[dash_find(&d,"m")].value);
}
void test_ctxapply_meter_value(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("meter", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":55}", 1); context_apply(&d);
    TEST_ASSERT_EQUAL_INT(55, d.components[dash_find(&d,"x")].value);
}
void test_ctxapply_chart_appends_on_change(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("chart", ",\"points\":5"), err, sizeof(err));
    int i = dash_find(&d, "x");
    dash_set_context(&d, "{\"v\":10}", 1); context_apply(&d);
    dash_set_context(&d, "{\"v\":10}", 2); context_apply(&d);   // même valeur -> pas de 2e append
    TEST_ASSERT_EQUAL_INT(1, d.components[i].hist_count);
    dash_set_context(&d, "{\"v\":20}", 3); context_apply(&d);   // change -> append
    TEST_ASSERT_EQUAL_INT(2,  d.components[i].hist_count);
    TEST_ASSERT_EQUAL_INT(10, d.components[i].hist[0]);
    TEST_ASSERT_EQUAL_INT(20, d.components[i].hist[1]);
}
```

Enregistrer les 5 dans `main()`, après `RUN_TEST(test_update_sound_sets_pending);` :

```cpp
    RUN_TEST(test_chart_ring_keeps_last_n);
    RUN_TEST(test_chart_points_parsed_and_clamped);
    RUN_TEST(test_update_meter_value);
    RUN_TEST(test_ctxapply_meter_value);
    RUN_TEST(test_ctxapply_chart_appends_on_change);
```

- [ ] **Step 7 : Lancer les tests natifs (PAS de build esp32s3)**

Run : `cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio test -e native`
Expected : `66 test cases: 66 succeeded` / `PASSED` (61 + 5). Si un test échoue ou le build natif casse, STOP et reporter BLOCKED avec la sortie. **Ne pas lancer `pio run -e esp32s3` (cassé par design jusqu'à Task 2).**

- [ ] **Step 8 : Commit**

```bash
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32
git add devices/guition_knob/projects/Rich_Telemetry/src/config.h \
        devices/guition_knob/projects/Rich_Telemetry/src/dashboard.h \
        devices/guition_knob/projects/Rich_Telemetry/src/dashboard.cpp \
        devices/guition_knob/projects/Rich_Telemetry/test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: chart/meter — modele (enum, history, apply, context) + tests natifs"
```

---

## Task 2 : Vue LVGL + activation lv_conf (build_/sync_ chart & meter)

**Files:**
- Modify: `src/lv_conf.h`, `src/view.cpp`

- [ ] **Step 1 : Activer les widgets extra (lv_conf.h)**

Dans `src/lv_conf.h`, ajouter avant `#define LV_BUILD_EXAMPLES      0` :

```cpp
// Widgets "extra" utilises par chart/meter
#define LV_USE_CHART           1
#define LV_USE_METER           1
```

- [ ] **Step 2 : build/sync chart & meter (view.cpp)**

Dans `src/view.cpp`, ajouter ce bloc **juste après** `sync_ring` (sa `}` fermante) et **avant** la `struct ViewVTable { ... };` :

```cpp
// --- chart : l'historique vit dans le modèle (Component.hist) ; build crée le widget,
// sync mirroir hist -> y_points (lv_chart_set_next_value n'est PAS idempotent). ---
static void build_chart(lv_obj_t* parent, Component& c, Placement& q,
                        lv_obj_t** main, lv_obj_t**, lv_obj_t**) {
    lv_obj_t* chart = lv_chart_create(parent);
    lv_obj_set_size(chart, q.width ? q.width : 200, q.height ? q.height : 100);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    int n = c.chart_points;
    if (n > CHART_MAX_POINTS) n = CHART_MAX_POINTS;
    if (n < 1) n = 1;
    lv_chart_set_point_count(chart, n);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, c.vmin, c.vmax);
    lv_chart_add_series(chart, lv_color_hex(c.color), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_align(chart, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = chart;
}
static void sync_chart(Component& c, Placement&, lv_obj_t* chart, lv_obj_t*, lv_obj_t*) {
    lv_chart_series_t* ser = lv_chart_get_series_next(chart, NULL);   // pas de stockage : on relit la 1re série
    if (!ser) return;
    int n = c.chart_points;
    if (n > CHART_MAX_POINTS) n = CHART_MAX_POINTS;
    if (n < 1) n = 1;
    for (int i = 0; i < n; i++)
        ser->y_points[i] = (i < c.hist_count) ? c.hist[i] : LV_CHART_POINT_NONE;
    lv_chart_refresh(chart);
}

// --- meter : jauge à aiguille ; thresholds réutilisés en zones d'arc.
// Handle aiguille stocké dans le slot sub1 (pas de getter d'indicateur côté lv_meter). ---
static void build_meter(lv_obj_t* parent, Component& c, Placement& q,
                        lv_obj_t** main, lv_obj_t** sub1, lv_obj_t**) {
    lv_obj_t* meter = lv_meter_create(parent);
    int sz = q.width ? q.width : 160;
    lv_obj_set_size(meter, sz, q.height ? q.height : sz);
    lv_meter_scale_t* scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 21, 2, 8, lv_color_hex(0x4B5563));
    lv_meter_set_scale_major_ticks(meter, scale, 5, 3, 12, lv_color_hex(0x9CA3AF), 10);
    lv_meter_set_scale_range(meter, scale, c.vmin, c.vmax, 270, 135);   // arc 270° ouvert en bas
    // zones d'arc depuis thresholds : bande i = (prev, limit[i]] couleur i ; prev démarre à vmin
    int prev = c.vmin;
    for (int i = 0; i < c.threshold_count; i++) {
        lv_meter_indicator_t* arc = lv_meter_add_arc(meter, scale, 5, lv_color_hex(c.thresholds[i].color), 0);
        lv_meter_set_indicator_start_value(meter, arc, prev);
        lv_meter_set_indicator_end_value(meter, arc, (int)c.thresholds[i].limit);
        prev = (int)c.thresholds[i].limit;
    }
    lv_meter_indicator_t* needle = lv_meter_add_needle_line(meter, scale, 4, lv_color_hex(c.color), -10);
    lv_meter_set_indicator_value(meter, needle, c.value);
    lv_obj_align(meter, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = meter;
    *sub1 = (lv_obj_t*)(void*)needle;     // handle aiguille pour sync (cast opaque, jamais déréférencé en lv_obj_t)
}
static void sync_meter(Component& c, Placement&, lv_obj_t* meter, lv_obj_t* sub1, lv_obj_t*) {
    lv_meter_indicator_t* needle = (lv_meter_indicator_t*)(void*)sub1;
    if (needle) lv_meter_set_indicator_value(meter, needle, c.value);
}
```

- [ ] **Step 3 : Lignes de table VIEW[] (view.cpp)**

Dans la table `VIEW[]`, ajouter après `/* COMP_SOUND    */ { nullptr,    nullptr      },` :

```cpp
    /* COMP_CHART    */ { build_chart, sync_chart },
    /* COMP_METER    */ { build_meter, sync_meter },
```

(Le `static_assert(... == COMP_COUNT ...)` existant passe maintenant : 9 entrées == `COMP_COUNT`.)

- [ ] **Step 4 : Compiler le firmware**

Run : `cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio run -e esp32s3`
Expected : `SUCCESS`. RAM/Flash en hausse vs 42.7 %/25.0 % (buffers hist + code chart/meter) — reporter les chiffres réels. Si échec (API LVGL, mémoire, static_assert), STOP et reporter BLOCKED avec l'erreur exacte.

- [ ] **Step 5 : Confirmer le natif (non-régression modèle)**

Run : `cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32/devices/guition_knob/projects/Rich_Telemetry && ~/.platformio/penv/bin/pio test -e native`
Expected : `66 test cases: 66 succeeded`.

- [ ] **Step 6 : Commit**

```bash
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32
git add devices/guition_knob/projects/Rich_Telemetry/src/lv_conf.h \
        devices/guition_knob/projects/Rich_Telemetry/src/view.cpp
git commit -m "Rich_Telemetry: chart/meter — rendu LVGL v8.4 (build/sync) + LV_USE_CHART/METER"
```

---

## Task 3 : Validation sur device (chart + meter)

**Pré-requis :** Guition `knobGuitionNoir1` branchée (`/dev/cu.usbmodem8301`). Contrôleur flashe + curl, utilisateur valide le visuel.

- [ ] **Step 1 : Flasher** — `./build.sh auto Rich_Telemetry --upload` (depuis la racine).

- [ ] **Step 2 : IP DHCP** — lire `IP=` au boot via pyserial (`~/.platformio/penv/bin/python`, pulse DTR/RTS). Confirmer `curl -s http://<IP>/status`.

- [ ] **Step 3 : Pousser un layout chart + meter**

```bash
curl -s -X POST http://<IP>/layout -H 'Content-Type: application/json' -d '{
  "title":"chart/meter","background":"#0B0B0F",
  "components":{
    "m1":{"type":"meter","color":"#E5E7EB","min":0,"max":100,
          "thresholds":[[60,"#22C55E"],[85,"#F59E0B"],[100,"#EF4444"]]},
    "c1":{"type":"chart","color":"#A78BFA","min":0,"max":100,"points":40}
  },
  "pages":[{"name":"main","place":[
    {"ref":"m1","anchor":"TOP_MID","dy":24,"width":180,"height":180},
    {"ref":"c1","anchor":"BOTTOM_MID","dy":-24,"width":300,"height":92}
  ]}]
}'
```
Expected : `{"ok":true}`.

- [ ] **Step 4 : Alimenter le meter + une série pour le chart**

```bash
# meter dans la zone orange (entre 60 et 85)
curl -s -X POST http://<IP>/update -H 'Content-Type: application/json' -d '{"m1":72}'
# série chart (chaque POST = un point ; on en pousse ~12 pour voir la sparkline monter/descendre)
for v in 10 25 40 30 55 70 60 80 65 90 50 35; do
  curl -s -X POST http://<IP>/update -H 'Content-Type: application/json' -d "{\"c1\":$v}" >/dev/null
done
curl -s http://<IP>/status
```

- [ ] **Step 5 : Contrôle visuel (utilisateur)**

Critère de succès :
- **meter** `m1` (en haut) : jauge à aiguille, **aiguille pointant ~72/100**, **zones d'arc colorées** vert (0-60) / orange (60-85) / rouge (85-100), ticks d'échelle.
- **chart** `c1` (en bas) : **ligne sparkline** violette tracée à partir des 12 points poussés (monte/descend), sur la plage 0-100.
- Pas de crash (`/status` répond, `uptime_s` continu).
- Bonus : pousser encore quelques `c1` et vérifier que la ligne **défile** (les vieux points sortent à gauche une fois 40 atteints).

Tout widget vide / écran noir / reset = à investiguer (mémoire LVGL, API) avant de clore.

- [ ] **Step 6 : Restaurer le layout d'usine**

```bash
curl -s -X POST http://<IP>/layout -H 'Content-Type: application/json' -d '{"title":"Claude","background":"#0B0B0F","nav":{"wrap":true},"components":{"w5h":{"type":"ring","color":"#38BDF8","pill":true,"countdown":true},"w7d":{"type":"ring","color":"#A78BFA","pill":true,"countdown":true},"led":{"type":"led_ring"},"buzz":{"type":"sound"}},"pages":[{"name":"usage","place":[{"ref":"w5h","radius":176,"thickness":16,"gap_deg":70},{"ref":"w7d","radius":141,"thickness":16,"gap_deg":70}]}]}'
curl -s http://<IP>/status   # attendu components:4, sources:[]
```

---

## Self-Review

**1. Couverture de la spec (`specs/2026-06-16-chart-meter-components-design.md`) :**
- chart = scalaire poussé, historique dans le modèle, ring buffer mirroir idempotent → `chart_push` + `sync_chart` (y_points + refresh). ✅
- chart config `color`/`min`/`max`/`points` (défaut 30, borné) → parse + clamp. ✅
- chart géométrie placeable (anchor/dx/dy/width/height) → `build_chart` via `ALIGN_MAP`/`q`. ✅
- API v8.4 chart (`set_type LINE`, `set_point_count`, `set_range` avec axe, `add_series`, `y_points`+`refresh`) → vérifiées Context7, utilisées telles quelles. ✅
- meter = scalaire→aiguille, idempotent ; `color`/`min`/`max`/`thresholds` en zones d'arc ; pas de lecture centrale ; placeable → `build_meter`/`sync_meter`. ✅
- API v8.4 meter (`add_scale`, `set_scale_ticks`, `set_scale_major_ticks`, `set_scale_range`, `add_arc`, `set_indicator_start/end_value`, `add_needle_line`, `set_indicator_value`) → vérifiées Context7. ✅
- LV_USE_CHART/METER activés dans lv_conf. ✅
- Ring buffer native-testé (push 35 → garder 30, ordre). ✅
- Schema/designer **non touchés** (firmware-d'abord ; P3 réconcilie) — déviation assumée vs « touchpoints » de la spec, documentée et cohérente avec le précédent `sources`/`bind`. ✅
- bind couvert en `context_apply` (chart=append-on-change, meter=valeur). ✅ (sémantique append-on-change = décision documentée.)

**2. Scan placeholders :** aucun TODO/TBD ; tout le code est complet ; commandes + sorties attendues présentes.

**3. Cohérence types/signatures :**
- `build_*`/`sync_*` respectent les signatures de `ViewVTable` (de la 2b) : `build(lv_obj_t*,Component&,Placement&,lv_obj_t**,lv_obj_t**,lv_obj_t**)`, `sync(Component&,Placement&,lv_obj_t*,lv_obj_t*,lv_obj_t*)`. ✅
- `comp_apply_fn` = `void(*)(Component&,JsonVariantConst)` — `apply_chart`/`apply_meter` conformes. ✅
- `COMP_COUNT` passe de 7 à 9 ; `APPLY[]` (Task 1) et `VIEW[]` (Task 2) ont 9 entrées chacune → les deux `static_assert` passent **après** leurs tasks respectives. La fenêtre Task1→Task2 où esp32s3 ne build pas est **explicitement flaggée** (Step 1 de Task 1). ✅
- `chart_push` partagé par `apply_chart` (/update) et `context_apply` (bind) — défini avant les deux. ✅
- `hist`/`hist_count`/`chart_points` ajoutés à la struct plate ; `CHART_MAX_POINTS` dans config.h, visible partout (config.h inclus via dashboard.h). ✅
