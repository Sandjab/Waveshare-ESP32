# Rich_Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un dashboard de télémétrie piloté par config JSON pour le Guition K718 (écran rond 360×360), avec pages navigables, couronnes d'usage, anneau LED et son, le tout adressé par une API REST.

**Architecture:** Séparation **état / vue**. Le *modèle* (composants, pages, page active) est du C++ pur sans dépendance HW/LVGL → testé en natif. La *vue* LVGL et les composants physiques (anneau LED, son) consomment le modèle et sont vérifiés on-device. Mono-thread : les handlers HTTP modifient le modèle + lèvent des flags, le rendu/les ticks sont déférés au `loop()`.

**Tech Stack:** PlatformIO (Arduino-ESP32 + esp_lcd), LVGL 8.4, ArduinoJson 7, Adafruit NeoPixel, `esp_lcd_touch_cst816s`, LittleFS. Tests natifs via PlatformIO Unity.

**Spec de référence :** `docs/superpowers/specs/2026-06-13-rich-telemetry-design.md`

**Répertoire projet :** `devices/guition_knob/projects/Rich_Telemetry/` (chemins ci-dessous relatifs à la racine du monorepo).

---

## File Structure

```
devices/guition_knob/projects/Rich_Telemetry/
  platformio.ini                # env:esp32s3 (device) + env:native (tests cœur)
  README.md
  data/layout.json              # layout par défaut (fallback uploadfs)
  src/
    config.h                    # caps + constantes (HW-free)
    dashboard.h                 # types modèle + signatures (HW-free)
    dashboard.cpp               # modèle pur : parse layout, apply_update, countdown, nav (ArduinoJson)
    format.h / format.cpp       # format_remaining, format_value (HW-free)
    color.h  / color.cpp        # parse_hex_color, threshold_color (HW-free)
    nav_logic.h / nav_logic.cpp # nav_next, nav_prev (HW-free)
    view.h   / view.cpp         # LVGL : build pages + widgets, sync modèle→widgets, dots
    led_ring_comp.h/.cpp        # machine à états anneau WS2812
    sound_comp.h/.cpp           # moteur tonalités + file (I2S)
    nav_input.h/.cpp            # encodeur + (touch CST816 à l'étape finale) → goto_page
    touch_cst816.h/.cpp         # indev pointeur LVGL (étape finale)
    api.h    / api.cpp          # routes HTTP
    persist.h/ persist.cpp      # LittleFS load/save
    main.cpp                    # setup/loop, WiFi, mDNS, câblage
    lv_conf.h                   # copié de Basic_WiFi_Telemetry
    secrets.h / secrets.h.example
  test/
    test_core/test_main.cpp     # tests natifs Unity (format, color, nav, modèle)
  tools/push.py                 # client exemple
```

**Frontières clés :**
- `config.h`, `dashboard.h`, `format.h`, `color.h`, `nav_logic.h` : **aucun** `#include` Arduino/LVGL. Compilables en natif.
- `dashboard.cpp` : inclut **seulement** `ArduinoJson.h` (dispo en natif) + les headers purs.
- Tout ce qui touche LVGL/I2S/NeoPixel/WiFi vit dans `view/led_ring_comp/sound_comp/nav_input/touch_cst816/api/persist/main`.

---

## Task 1: Scaffolding projet + env natif

**Files:**
- Create: `devices/guition_knob/projects/Rich_Telemetry/platformio.ini`
- Create: `devices/guition_knob/projects/Rich_Telemetry/src/lv_conf.h` (copie)
- Create: `devices/guition_knob/projects/Rich_Telemetry/src/secrets.h.example`
- Create: `devices/guition_knob/projects/Rich_Telemetry/src/main.cpp` (stub)
- Create: `devices/guition_knob/projects/Rich_Telemetry/test/test_core/test_main.cpp` (stub)

- [ ] **Step 1: Créer l'arborescence et copier `lv_conf.h` + l'exemple secrets**

```bash
cd devices/guition_knob/projects/Rich_Telemetry
mkdir -p src test/test_core data tools
cp ../Basic_WiFi_Telemetry/src/lv_conf.h src/lv_conf.h
cp ../Basic_WiFi_Telemetry/src/secrets.h.example src/secrets.h.example
cp src/secrets.h.example src/secrets.h     # renseigner WIFI_SSID / WIFI_PASS
```

- [ ] **Step 2: Écrire `platformio.ini`**

```ini
[env:esp32s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.07/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
lib_extra_dirs =
    ../../lib
    ../../../../shared/lib
lib_deps =
    lvgl/lvgl@^8.4.0
    bblanchon/ArduinoJson@^7.0.0
    adafruit/Adafruit NeoPixel
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
    -DLV_CONF_INCLUDE_SIMPLE
    -Isrc

# Tests du cœur logique pur (aucune dépendance HW/LVGL).
[env:native]
platform = native
test_framework = unity
lib_deps = bblanchon/ArduinoJson@^7.0.0
build_src_filter = -<*> +<dashboard.cpp> +<format.cpp> +<color.cpp> +<nav_logic.cpp>
build_flags = -DRT_NATIVE_TEST -Isrc -std=gnu++17
```

- [ ] **Step 3: Écrire un `main.cpp` stub qui compile**

```cpp
#include <Arduino.h>
#include "guition_lvgl.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nGuition JC3636K718 - Rich_Telemetry (stub)");
    guition_lvgl_init();
}

void loop() {
    lv_timer_handler();
    delay(5);
}
```

- [ ] **Step 4: Écrire un test natif stub (sinon `pio test` échoue sur dossier vide)**

```cpp
#include <unity.h>
void test_placeholder(void) { TEST_ASSERT_TRUE(true); }
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
```

- [ ] **Step 5: Vérifier la compilation device**

Run: `./build.sh guition Rich_Telemetry`
Expected: build SUCCESS (depuis la racine du monorepo).

- [ ] **Step 6: Vérifier les tests natifs**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native`
Expected: `test_placeholder PASSED`, `1 Tests 0 Failures`.

- [ ] **Step 7: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry
git commit -m "Rich_Telemetry: scaffold project + native test env"
```

---

## Task 2: `config.h` + `dashboard.h` (types du modèle, HW-free)

**Files:**
- Create: `devices/guition_knob/projects/Rich_Telemetry/src/config.h`
- Create: `devices/guition_knob/projects/Rich_Telemetry/src/dashboard.h`

Ces fichiers ne contiennent que des `#define`, structs, enums et signatures. Aucun `#include` Arduino/LVGL.

- [ ] **Step 1: Écrire `config.h`**

```cpp
#pragma once
#define MAX_COMPONENTS          32
#define MAX_PAGES               8
#define MAX_PLACEMENTS_PER_PAGE 12
#define MAX_THRESHOLDS          4
#define ID_LEN                  24
#define TEXT_LEN                32
#define CAPTION_LEN             24
#define UNKNOWN_CSV_LEN         128

#define HTTP_PORT               80
#define MDNS_HOST               "guition"
#define WIFI_BOOT_TIMEOUT_MS    20000
#define LAYOUT_PATH             "/layout.json"
```

- [ ] **Step 2: Écrire `dashboard.h`**

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"

enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND };
enum LedMode  { LED_OFF, LED_SOLID, LED_PROGRESS, LED_SPINNER, LED_BLINK, LED_BREATHE };
enum Anchor   { A_CENTER, A_TOP_MID, A_BOTTOM_MID, A_LEFT_MID, A_RIGHT_MID,
                A_TOP_LEFT, A_TOP_RIGHT, A_BOTTOM_LEFT, A_BOTTOM_RIGHT };

struct Threshold { float limit; uint32_t color; };

struct Component {
    char     id[ID_LEN];
    CompType type;

    // --- config (style/données, sans position) ---
    char     label[TEXT_LEN];     // readout : texte gauche / bar : titre
    char     unit[8];
    char     text[TEXT_LEN];      // label statique : texte initial
    uint32_t color;               // couleur principale 0xRRGGBB
    int32_t  vmin, vmax;          // plage bar/ring (défaut 0/100)
    bool     pill, center_pct, countdown;
    Threshold thresholds[MAX_THRESHOLDS];
    int      threshold_count;
    uint16_t font;                // label : taille px (14/20/28)
    uint8_t  led_brightness_cfg;  // led_ring : luminosité défaut

    // --- état (modifié par /update) ---
    int32_t  value;               // bar : valeur ; ring : pct
    char     vstr[TEXT_LEN];      // chaîne d'affichage formatée (label/readout)
    uint32_t reset_in_s;          // ring countdown
    char     caption[CAPTION_LEN];// ring : texte du bas (formaté)
    // led_ring
    LedMode  led_mode; uint32_t led_color; uint8_t led_value, led_brightness; uint16_t led_period_ms;
    // sound : événement fire-once
    bool     snd_pending; uint16_t snd_tone; uint16_t snd_ms; char snd_name[12];

    bool     dirty;               // état changé depuis la dernière synchro vue
};

struct Placement {
    int     comp_index;           // index dans Dashboard.components[]
    Anchor  anchor; int16_t dx, dy; int16_t width, height;   // géométrie générique
    int16_t radius, thickness, gap_deg, start_angle;          // géométrie ring
};

struct Page {
    char      name[ID_LEN];
    Placement places[MAX_PLACEMENTS_PER_PAGE];
    int       place_count;
};

struct Dashboard {
    char      title[TEXT_LEN];
    uint32_t  background;
    bool      nav_wrap;
    Component components[MAX_COMPONENTS];
    int       comp_count;
    Page      pages[MAX_PAGES];
    int       page_count;
    int       active_page;
    bool      layout_dirty;       // structure changée -> reconstruire les vues
    bool      values_dirty;       // au moins un composant.dirty
};

// --- API du modèle (dashboard.cpp) ---
int  dash_find(const Dashboard* d, const char* id);                       // index ou -1
bool dash_set_layout(Dashboard* d, const char* json, char* err, size_t errn);
int  dash_apply_update(Dashboard* d, const char* json, char* unknown_csv, size_t n);
void dash_tick_countdown(Dashboard* d, uint32_t elapsed_s);
```

- [ ] **Step 3: Vérifier que ça compile (inclusion seule)**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && cc -fsyntax-only -xc++ -std=gnu++17 -Isrc src/dashboard.h`
Expected: aucune erreur.

- [ ] **Step 4: Commit**

```bash
git add devices/guition_knob/projects/Rich_Telemetry/src/config.h devices/guition_knob/projects/Rich_Telemetry/src/dashboard.h
git commit -m "Rich_Telemetry: model types (config.h + dashboard.h)"
```

---

## Task 3: `format.{h,cpp}` — formatage compact (TDD natif)

**Files:**
- Create: `src/format.h`, `src/format.cpp`
- Test: `test/test_core/test_main.cpp` (ajout)

`format_remaining` : `"45s"`, `"50m"`, `"1h50"`, `"5j6h"`. `format_value` : `"42 %"`, `"9.2 GB"`, `"42"`.

- [ ] **Step 1: Écrire les tests (échouent)**

Remplacer le contenu de `test/test_core/test_main.cpp` par :

```cpp
#include <unity.h>
#include <string.h>
#include "format.h"

static char buf[32];

void test_remaining_seconds(void)  { format_remaining(45, buf, sizeof(buf));    TEST_ASSERT_EQUAL_STRING("45s",  buf); }
void test_remaining_min_boundary(void){ format_remaining(60, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("1m",   buf); }
void test_remaining_minutes(void)  { format_remaining(50*60, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("50m",  buf); }
void test_remaining_hour(void)     { format_remaining(3600, buf, sizeof(buf));  TEST_ASSERT_EQUAL_STRING("1h00", buf); }
void test_remaining_h_m(void)      { format_remaining(6600, buf, sizeof(buf));  TEST_ASSERT_EQUAL_STRING("1h50", buf); }
void test_remaining_days(void)     { format_remaining(453600, buf, sizeof(buf));TEST_ASSERT_EQUAL_STRING("5j6h", buf); }
void test_remaining_zero(void)     { format_remaining(0, buf, sizeof(buf));     TEST_ASSERT_EQUAL_STRING("0s",   buf); }

void test_value_unit(void)    { format_value(42, "%",  buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("42 %", buf); }
void test_value_float(void)   { format_value(9.2, "GB",buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("9.2 GB", buf); }
void test_value_no_unit(void) { format_value(42, "",   buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("42", buf); }

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_remaining_seconds);
    RUN_TEST(test_remaining_min_boundary);
    RUN_TEST(test_remaining_minutes);
    RUN_TEST(test_remaining_hour);
    RUN_TEST(test_remaining_h_m);
    RUN_TEST(test_remaining_days);
    RUN_TEST(test_remaining_zero);
    RUN_TEST(test_value_unit);
    RUN_TEST(test_value_float);
    RUN_TEST(test_value_no_unit);
    return UNITY_END();
}
```

- [ ] **Step 2: Lancer — doit échouer (compilation : `format.h` absent)**

Run: `pio test -e native`
Expected: échec de compilation (`format.h: No such file`).

- [ ] **Step 3: Écrire `format.h`**

```cpp
#pragma once
#include <stddef.h>
#include <stdint.h>

void format_remaining(uint32_t seconds, char* out, size_t n);
void format_value(double v, const char* unit, char* out, size_t n);
```

- [ ] **Step 4: Écrire `format.cpp`**

```cpp
#include "format.h"
#include <stdio.h>
#include <string.h>

void format_remaining(uint32_t s, char* out, size_t n) {
    if (s >= 86400) {                       // jours + heures
        snprintf(out, n, "%luj%luh", (unsigned long)(s / 86400),
                 (unsigned long)((s % 86400) / 3600));
    } else if (s >= 3600) {                 // heures + minutes (mm sur 2 chiffres)
        snprintf(out, n, "%luh%02lu", (unsigned long)(s / 3600),
                 (unsigned long)((s % 3600) / 60));
    } else if (s >= 60) {                   // minutes
        snprintf(out, n, "%lum", (unsigned long)(s / 60));
    } else {                                // secondes
        snprintf(out, n, "%lus", (unsigned long)s);
    }
}

void format_value(double v, const char* unit, char* out, size_t n) {
    char num[24];
    if (v == (long long)v) snprintf(num, sizeof(num), "%lld", (long long)v);
    else                   snprintf(num, sizeof(num), "%.1f", v);
    if (unit && unit[0]) snprintf(out, n, "%s %s", num, unit);
    else                 snprintf(out, n, "%s", num);
}
```

- [ ] **Step 5: Lancer — doit passer**

Run: `pio test -e native`
Expected: `10 Tests 0 Failures`.

- [ ] **Step 6: Commit**

```bash
git add src/format.h src/format.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: format_remaining + format_value (native TDD)"
```

---

## Task 4: `color.{h,cpp}` — couleurs hex + seuils (TDD natif)

**Files:**
- Create: `src/color.h`, `src/color.cpp`
- Test: `test/test_core/test_main.cpp` (ajout)

`parse_hex_color("#38BDF8", fb)` → `0x38BDF8`. `threshold_color` : renvoie la couleur du **premier** seuil dont `value < limit`, sinon `base`.

- [ ] **Step 1: Ajouter les tests (avant `int main`)**

Ajouter en tête `#include "color.h"` et ces tests, + leurs `RUN_TEST` dans `main` :

```cpp
void test_hex_parse(void)     { TEST_ASSERT_EQUAL_HEX32(0x38BDF8, parse_hex_color("#38BDF8", 0)); }
void test_hex_no_hash(void)   { TEST_ASSERT_EQUAL_HEX32(0xA1B2C3, parse_hex_color("A1B2C3", 0)); }
void test_hex_fallback(void)  { TEST_ASSERT_EQUAL_HEX32(0x123456, parse_hex_color("nope", 0x123456)); }

void test_threshold_below(void) {
    Threshold t[3] = {{70,0x22C55E},{90,0xF59E0B},{100,0xEF4444}};
    TEST_ASSERT_EQUAL_HEX32(0x22C55E, threshold_color(t,3,63,0x000000));   // 63 < 70
}
void test_threshold_mid(void) {
    Threshold t[3] = {{70,0x22C55E},{90,0xF59E0B},{100,0xEF4444}};
    TEST_ASSERT_EQUAL_HEX32(0xF59E0B, threshold_color(t,3,85,0x000000));   // 70<=85<90
}
void test_threshold_over(void) {
    Threshold t[3] = {{70,0x22C55E},{90,0xF59E0B},{100,0xEF4444}};
    TEST_ASSERT_EQUAL_HEX32(0xEF4444, threshold_color(t,3,95,0x000000));   // 90<=95<100
}
void test_threshold_none(void) {
    Threshold t[1] = {{70,0x22C55E}};
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, threshold_color(t,0,50,0xABCDEF));   // aucun seuil -> base
}
```

`color.h` inclut `dashboard.h` pour le type `Threshold`. Ajoute `#include "dashboard.h"` en tête du test si nécessaire (déjà tiré par `color.h`).

- [ ] **Step 2: Lancer — échoue (compilation)**

Run: `pio test -e native`
Expected: échec (`color.h` absent).

- [ ] **Step 3: Écrire `color.h`**

```cpp
#pragma once
#include <stdint.h>
#include "dashboard.h"

uint32_t parse_hex_color(const char* s, uint32_t fallback);
uint32_t threshold_color(const Threshold* t, int n, float value, uint32_t base);
```

- [ ] **Step 4: Écrire `color.cpp`**

```cpp
#include "color.h"
#include <stdlib.h>

uint32_t parse_hex_color(const char* s, uint32_t fallback) {
    if (!s) return fallback;
    if (*s == '#') s++;
    char* end = nullptr;
    unsigned long v = strtoul(s, &end, 16);
    if (end == s || *end != '\0') return fallback;
    return (uint32_t)(v & 0xFFFFFF);
}

uint32_t threshold_color(const Threshold* t, int n, float value, uint32_t base) {
    for (int i = 0; i < n; i++)
        if (value < t[i].limit) return t[i].color;
    return base;
}
```

- [ ] **Step 5: Lancer — passe**

Run: `pio test -e native`
Expected: `17 Tests 0 Failures`.

- [ ] **Step 6: Commit**

```bash
git add src/color.h src/color.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: hex color + threshold color (native TDD)"
```

---

## Task 5: `nav_logic.{h,cpp}` — navigation circulaire (TDD natif)

**Files:**
- Create: `src/nav_logic.h`, `src/nav_logic.cpp`
- Test: `test/test_core/test_main.cpp` (ajout)

- [ ] **Step 1: Ajouter les tests (+ `#include "nav_logic.h"` + `RUN_TEST`)**

```cpp
void test_next_mid(void)     { TEST_ASSERT_EQUAL_INT(2, nav_next(1, 3, true)); }
void test_next_wrap(void)    { TEST_ASSERT_EQUAL_INT(0, nav_next(2, 3, true)); }
void test_next_clamp(void)   { TEST_ASSERT_EQUAL_INT(2, nav_next(2, 3, false)); }
void test_prev_wrap(void)    { TEST_ASSERT_EQUAL_INT(2, nav_prev(0, 3, true)); }
void test_prev_clamp(void)   { TEST_ASSERT_EQUAL_INT(0, nav_prev(0, 3, false)); }
void test_single_page(void)  { TEST_ASSERT_EQUAL_INT(0, nav_next(0, 1, true)); }
void test_empty(void)        { TEST_ASSERT_EQUAL_INT(0, nav_next(0, 0, true)); }
```

- [ ] **Step 2: Lancer — échoue (compilation)**

Run: `pio test -e native`
Expected: échec (`nav_logic.h` absent).

- [ ] **Step 3: Écrire `nav_logic.h`**

```cpp
#pragma once
int nav_next(int idx, int count, bool wrap);
int nav_prev(int idx, int count, bool wrap);
```

- [ ] **Step 4: Écrire `nav_logic.cpp`**

```cpp
#include "nav_logic.h"

int nav_next(int idx, int count, bool wrap) {
    if (count <= 1) return idx < 0 ? 0 : idx % (count > 0 ? count : 1);
    if (idx + 1 < count) return idx + 1;
    return wrap ? 0 : count - 1;
}

int nav_prev(int idx, int count, bool wrap) {
    if (count <= 1) return 0;
    if (idx - 1 >= 0) return idx - 1;
    return wrap ? count - 1 : 0;
}
```

- [ ] **Step 5: Lancer — passe**

Run: `pio test -e native`
Expected: `24 Tests 0 Failures`.

- [ ] **Step 6: Commit**

```bash
git add src/nav_logic.h src/nav_logic.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: circular nav logic (native TDD)"
```

---

## Task 6: `dashboard.cpp` — parse du layout (TDD natif)

**Files:**
- Create: `src/dashboard.cpp`
- Test: `test/test_core/test_main.cpp` (ajout)

`dash_set_layout` parse le JSON dans les structs, valide (type connu, caps), et **ne remplace `*d` que si tout est valide** (sinon renvoie `false` + message, `*d` inchangé).

- [ ] **Step 1: Ajouter les tests (+ `#include "dashboard.h"` + `RUN_TEST`)**

```cpp
static const char* LAYOUT_OK =
  "{\"title\":\"T\",\"background\":\"#0B0B0F\",\"nav\":{\"wrap\":true},"
  "\"components\":{"
    "\"w5h\":{\"type\":\"ring\",\"color\":\"#38BDF8\",\"countdown\":true,"
             "\"thresholds\":[[70,\"#22C55E\"],[90,\"#F59E0B\"]]},"
    "\"cpu\":{\"type\":\"readout\",\"label\":\"CPU\",\"unit\":\"%\"}},"
  "\"pages\":[{\"name\":\"usage\",\"place\":["
    "{\"ref\":\"w5h\",\"radius\":140,\"thickness\":16,\"gap_deg\":70},"
    "{\"ref\":\"cpu\",\"anchor\":\"CENTER\"}]}]}";

void test_layout_parse_counts(void) {
    Dashboard d{}; char err[80];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_OK, err, sizeof(err)));
    TEST_ASSERT_EQUAL_INT(2, d.comp_count);
    TEST_ASSERT_EQUAL_INT(1, d.page_count);
    TEST_ASSERT_EQUAL_INT(2, d.pages[0].place_count);
    TEST_ASSERT_TRUE(d.nav_wrap);
}
void test_layout_types_and_geom(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int iw = dash_find(&d, "w5h");
    TEST_ASSERT_EQUAL_INT(COMP_RING, d.components[iw].type);
    TEST_ASSERT_TRUE(d.components[iw].countdown);
    TEST_ASSERT_EQUAL_INT(2, d.components[iw].threshold_count);
    TEST_ASSERT_EQUAL_HEX32(0x38BDF8, d.components[iw].color);
    TEST_ASSERT_EQUAL_INT(140, d.pages[0].places[0].radius);   // w5h placé en 1er
    TEST_ASSERT_EQUAL_INT(A_CENTER, d.pages[0].places[1].anchor);
}
void test_layout_unknown_type_rejected(void) {
    Dashboard d{}; char err[80];
    const char* bad = "{\"components\":{\"x\":{\"type\":\"frobnicator\"}},\"pages\":[]}";
    TEST_ASSERT_FALSE(dash_set_layout(&d, bad, err, sizeof(err)));
}
void test_layout_invalid_keeps_old(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));      // d valide
    dash_set_layout(&d, "{ not json", err, sizeof(err));   // doit échouer
    TEST_ASSERT_EQUAL_INT(2, d.comp_count);                // inchangé
}
```

- [ ] **Step 2: Lancer — échoue (lien : symboles `dash_*` absents)**

Run: `pio test -e native`
Expected: erreur de link (`undefined reference to dash_set_layout`).

- [ ] **Step 3: Écrire `dashboard.cpp` (parse + find ; apply_update/countdown ajoutés aux tâches suivantes)**

```cpp
#include "dashboard.h"
#include "color.h"
#include <ArduinoJson.h>
#include <string.h>

int dash_find(const Dashboard* d, const char* id) {
    for (int i = 0; i < d->comp_count; i++)
        if (strncmp(d->components[i].id, id, ID_LEN) == 0) return i;
    return -1;
}

static CompType parse_type(const char* s) {
    if (!s) return COMP_NONE;
    if (!strcmp(s,"label"))    return COMP_LABEL;
    if (!strcmp(s,"readout"))  return COMP_READOUT;
    if (!strcmp(s,"bar"))      return COMP_BAR;
    if (!strcmp(s,"ring"))     return COMP_RING;
    if (!strcmp(s,"led_ring")) return COMP_LED_RING;
    if (!strcmp(s,"sound"))    return COMP_SOUND;
    return COMP_NONE;
}

static Anchor parse_anchor(const char* s) {
    if (!s) return A_CENTER;
    if (!strcmp(s,"TOP_MID"))      return A_TOP_MID;
    if (!strcmp(s,"BOTTOM_MID"))   return A_BOTTOM_MID;
    if (!strcmp(s,"LEFT_MID"))     return A_LEFT_MID;
    if (!strcmp(s,"RIGHT_MID"))    return A_RIGHT_MID;
    if (!strcmp(s,"TOP_LEFT"))     return A_TOP_LEFT;
    if (!strcmp(s,"TOP_RIGHT"))    return A_TOP_RIGHT;
    if (!strcmp(s,"BOTTOM_LEFT"))  return A_BOTTOM_LEFT;
    if (!strcmp(s,"BOTTOM_RIGHT")) return A_BOTTOM_RIGHT;
    return A_CENTER;
}

// Parse dans un Dashboard temporaire ; ne copie dans *d qu'en cas de succès complet.
bool dash_set_layout(Dashboard* d, const char* json, char* err, size_t errn) {
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, json);
    if (e) { snprintf(err, errn, "JSON: %s", e.c_str()); return false; }

    Dashboard t{};
    strlcpy(t.title, doc["title"] | "", sizeof(t.title));
    t.background = parse_hex_color(doc["background"] | "#000000", 0x000000);
    t.nav_wrap   = doc["nav"]["wrap"] | true;

    JsonObjectConst comps = doc["components"].as<JsonObjectConst>();
    if (comps.isNull()) { snprintf(err, errn, "components manquant"); return false; }
    for (JsonPairConst kv : comps) {
        if (t.comp_count >= MAX_COMPONENTS) { snprintf(err, errn, "trop de composants"); return false; }
        Component& c = t.components[t.comp_count];
        strlcpy(c.id, kv.key().c_str(), sizeof(c.id));
        JsonObjectConst o = kv.value().as<JsonObjectConst>();
        c.type = parse_type(o["type"] | "");
        if (c.type == COMP_NONE) { snprintf(err, errn, "type inconnu pour '%s'", c.id); return false; }
        strlcpy(c.label, o["label"] | "", sizeof(c.label));
        strlcpy(c.unit,  o["unit"]  | "", sizeof(c.unit));
        strlcpy(c.text,  o["text"]  | "", sizeof(c.text));
        strlcpy(c.vstr,  o["text"]  | "", sizeof(c.vstr));     // label : valeur initiale = text
        c.color       = parse_hex_color(o["color"] | "#FFFFFF", 0xFFFFFF);
        c.vmin        = o["min"] | 0;
        c.vmax        = o["max"] | 100;
        c.pill        = o["pill"] | false;
        c.center_pct  = o["center_pct"] | false;
        c.countdown   = o["countdown"] | false;
        c.font        = o["font"] | 20;
        c.led_brightness_cfg = o["brightness"] | 64;
        JsonArrayConst th = o["thresholds"].as<JsonArrayConst>();
        for (JsonArrayConst pair : th) {
            if (c.threshold_count >= MAX_THRESHOLDS) break;
            c.thresholds[c.threshold_count].limit = pair[0].as<float>();
            c.thresholds[c.threshold_count].color = parse_hex_color(pair[1] | "#FFFFFF", 0xFFFFFF);
            c.threshold_count++;
        }
        t.comp_count++;
    }

    JsonArrayConst pages = doc["pages"].as<JsonArrayConst>();
    for (JsonObjectConst pg : pages) {
        if (t.page_count >= MAX_PAGES) { snprintf(err, errn, "trop de pages"); return false; }
        Page& p = t.pages[t.page_count];
        strlcpy(p.name, pg["name"] | "", sizeof(p.name));
        for (JsonObjectConst pl : pg["place"].as<JsonArrayConst>()) {
            if (p.place_count >= MAX_PLACEMENTS_PER_PAGE) { snprintf(err, errn, "trop de placements"); return false; }
            const char* ref = pl["ref"] | "";
            int ci = dash_find(&t, ref);
            if (ci < 0) { snprintf(err, errn, "ref inconnue '%s'", ref); return false; }
            Placement& q = p.places[p.place_count];
            q.comp_index  = ci;
            q.anchor      = parse_anchor(pl["anchor"] | "CENTER");
            q.dx          = pl["dx"] | 0;       q.dy     = pl["dy"] | 0;
            q.width       = pl["width"] | 0;    q.height = pl["height"] | 0;
            q.radius      = pl["radius"] | 0;   q.thickness = pl["thickness"] | 16;
            q.gap_deg     = pl["gap_deg"] | 70; q.start_angle = pl["start_angle"] | 0;
            p.place_count++;
        }
        t.page_count++;
    }

    t.active_page  = 0;
    t.layout_dirty = true;
    *d = t;                      // remplacement atomique seulement si tout a réussi
    return true;
}
```

- [ ] **Step 4: Lancer — passe**

Run: `pio test -e native`
Expected: `28 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: layout parse with validate-before-swap (native TDD)"
```

---

## Task 7: `dash_apply_update` — mise à jour partielle (TDD natif, invariant central)

**Files:**
- Modify: `src/dashboard.cpp` (ajout des fonctions)
- Test: `test/test_core/test_main.cpp` (ajout)

Sémantique : seuls les `id` présents dans le JSON sont modifiés ; les autres restent **inchangés**. Les `id` inconnus sont listés dans `unknown_csv` et **non** appliqués. Chaque composant touché → `dirty=true`, `d->values_dirty=true`.

- [ ] **Step 1: Ajouter les tests (+ `RUN_TEST`)**

```cpp
void test_update_partial_leaves_others(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int icpu = dash_find(&d,"cpu"), iw = dash_find(&d,"w5h");
    d.components[iw].value = 10;                       // valeur préexistante
    int nupd = dash_apply_update(&d, "{\"cpu\":42}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(1, nupd);
    TEST_ASSERT_EQUAL_STRING("42 %", d.components[icpu].vstr);   // cpu mis à jour
    TEST_ASSERT_EQUAL_INT(10, d.components[iw].value);          // w5h INCHANGÉ (invariant)
    TEST_ASSERT_TRUE(d.values_dirty);
}
void test_update_ring_object(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int iw = dash_find(&d,"w5h");
    dash_apply_update(&d, "{\"w5h\":{\"pct\":63,\"reset_in_s\":6600}}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(63, d.components[iw].value);
    TEST_ASSERT_EQUAL_UINT32(6600, d.components[iw].reset_in_s);
    TEST_ASSERT_EQUAL_STRING("1h50", d.components[iw].caption);   // formaté immédiatement
}
void test_update_unknown_reported_not_applied(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    int nupd = dash_apply_update(&d, "{\"ghost\":1,\"cpu\":5}", unk, sizeof(unk));
    TEST_ASSERT_EQUAL_INT(1, nupd);
    TEST_ASSERT_EQUAL_STRING("ghost", unk);
}
```

- [ ] **Step 2: Lancer — échoue (lien)**

Run: `pio test -e native`
Expected: `undefined reference to dash_apply_update`.

- [ ] **Step 3: Ajouter `dash_apply_update` à `dashboard.cpp`**

Ajouter `#include "format.h"` en tête, puis :

```cpp
static void apply_one(Component& c, JsonVariantConst v) {
    switch (c.type) {
        case COMP_LABEL:
            strlcpy(c.vstr, v.as<const char*>() ? v.as<const char*>() : c.vstr, sizeof(c.vstr));
            break;
        case COMP_READOUT:
            if (v.is<const char*>()) strlcpy(c.vstr, v.as<const char*>(), sizeof(c.vstr));
            else format_value(v.as<double>(), c.unit, c.vstr, sizeof(c.vstr));
            break;
        case COMP_BAR:
            c.value = v.as<int>();
            break;
        case COMP_RING:
            c.value      = v["pct"] | c.value;
            c.reset_in_s = v["reset_in_s"] | c.reset_in_s;
            if (v["caption"].is<const char*>()) {
                strlcpy(c.caption, v["caption"].as<const char*>(), sizeof(c.caption));
            } else if (c.countdown) {
                format_remaining(c.reset_in_s, c.caption, sizeof(c.caption));
            }
            break;
        case COMP_LED_RING: {
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
            break;
        }
        case COMP_SOUND:
            c.snd_pending = true;
            c.snd_tone = v["tone"] | 0;
            c.snd_ms   = v["ms"]   | 150;
            strlcpy(c.snd_name, v["name"] | "", sizeof(c.snd_name));
            break;
        default: break;
    }
}

int dash_apply_update(Dashboard* d, const char* json, char* unknown_csv, size_t n) {
    unknown_csv[0] = '\0';
    JsonDocument doc;
    if (deserializeJson(doc, json)) return -1;        // -1 = JSON invalide
    int updated = 0;
    for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
        int ci = dash_find(d, kv.key().c_str());
        if (ci < 0) {
            size_t len = strlen(unknown_csv);
            snprintf(unknown_csv + len, n - len, "%s%s", len ? "," : "", kv.key().c_str());
            continue;
        }
        apply_one(d->components[ci], kv.value());
        d->components[ci].dirty = true;
        d->values_dirty = true;
        updated++;
    }
    return updated;
}
```

- [ ] **Step 4: Lancer — passe**

Run: `pio test -e native`
Expected: `31 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: partial update semantics (native TDD, core invariant)"
```

---

## Task 8: `dash_tick_countdown` — décompte serveur (TDD natif)

**Files:**
- Modify: `src/dashboard.cpp`
- Test: `test/test_core/test_main.cpp` (ajout)

Décrémente `reset_in_s` des rings `countdown` de `elapsed_s` (sans passer sous 0), reformate `caption`, marque `dirty`.

- [ ] **Step 1: Ajouter les tests (+ `RUN_TEST`)**

```cpp
void test_countdown_decrements_and_formats(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    dash_apply_update(&d, "{\"w5h\":{\"pct\":63,\"reset_in_s\":3601}}", unk, sizeof(unk));
    int iw = dash_find(&d,"w5h");
    d.components[iw].dirty = false;
    dash_tick_countdown(&d, 1);                       // 3601 -> 3600
    TEST_ASSERT_EQUAL_UINT32(3600, d.components[iw].reset_in_s);
    TEST_ASSERT_EQUAL_STRING("1h00", d.components[iw].caption);
    TEST_ASSERT_TRUE(d.components[iw].dirty);
}
void test_countdown_floor_zero(void) {
    Dashboard d{}; char err[80], unk[UNKNOWN_CSV_LEN];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));
    dash_apply_update(&d, "{\"w5h\":{\"pct\":99,\"reset_in_s\":3}}", unk, sizeof(unk));
    dash_tick_countdown(&d, 10);                      // ne descend pas sous 0
    TEST_ASSERT_EQUAL_UINT32(0, d.components[dash_find(&d,"w5h")].reset_in_s);
}
```

- [ ] **Step 2: Lancer — échoue (lien)**

Run: `pio test -e native`
Expected: `undefined reference to dash_tick_countdown`.

- [ ] **Step 3: Ajouter à `dashboard.cpp`**

```cpp
void dash_tick_countdown(Dashboard* d, uint32_t elapsed_s) {
    for (int i = 0; i < d->comp_count; i++) {
        Component& c = d->components[i];
        if (c.type != COMP_RING || !c.countdown) continue;
        if (c.reset_in_s == 0) continue;
        c.reset_in_s = (c.reset_in_s > elapsed_s) ? c.reset_in_s - elapsed_s : 0;
        format_remaining(c.reset_in_s, c.caption, sizeof(c.caption));
        c.dirty = true;
        d->values_dirty = true;
    }
}
```

- [ ] **Step 4: Lancer — passe**

Run: `pio test -e native`
Expected: `33 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: server-side countdown tick (native TDD)"
```

---

## Task 9: `main.cpp` — boot (écran + WiFi + mDNS)

**Files:**
- Modify: `src/main.cpp`

Réutilise le boilerplate WiFi/mDNS de `../Basic_WiFi_Telemetry/src/main.cpp` (fonctions `wifi_connect`, surveillance WiFi 1 Hz dans `loop`). On câblera l'UI/API aux tâches suivantes.

**Premier flash** : à l'usine la carte tourne le firmware vendor → forcer le ROM bootloader (maintenir BOOT, brancher USB, relâcher) ; cf. `devices/guition_knob/CLAUDE.md` § « First flash ». Une fois notre firmware en place, l'auto-reset fonctionne.

- [ ] **Step 1: Écrire `main.cpp`**

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "guition_lvgl.h"
#include "config.h"
#include "secrets.h"

static WebServer server(HTTP_PORT);
static bool g_wifi_up = false;

static bool wifi_connect() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_BOOT_TIMEOUT_MS) {
        delay(200); Serial.print("."); lv_timer_handler();
    }
    Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

static void start_services() {
    static bool started = false;
    if (started) return;
    started = true;
    if (MDNS.begin(MDNS_HOST)) MDNS.addService("http", "tcp", HTTP_PORT);
    server.begin();                         // routes ajoutées en Task 11
    Serial.printf("[http] :%d  http://%s.local\n", HTTP_PORT, MDNS_HOST);
}

void setup() {
    Serial.begin(115200); delay(200);
    Serial.println("\nGuition JC3636K718 - Rich_Telemetry");
    guition_lvgl_init();
    lv_timer_handler();
    g_wifi_up = wifi_connect();
    if (g_wifi_up) {
        Serial.printf("[wifi] IP=%s\n", WiFi.localIP().toString().c_str());
        start_services();
    } else {
        Serial.println("[wifi] ECHEC (verifie secrets.h)");
    }
}

void loop() {
    server.handleClient();
    static uint32_t last = 0;
    if (millis() - last > 1000) {
        last = millis();
        bool now = (WiFi.status() == WL_CONNECTED);
        if (now && !g_wifi_up) start_services();
        g_wifi_up = now;
    }
    lv_timer_handler();
    delay(5);
}
```

- [ ] **Step 2: Build**

Run: `./build.sh guition Rich_Telemetry`
Expected: SUCCESS.

- [ ] **Step 3: Flash + observer le boot**

Run: `./build.sh auto Rich_Telemetry --upload`
Puis ouvrir le moniteur série (`pio device monitor -b 115200`).
Expected (évidence) : logs `Rich_Telemetry`, puis `[wifi] IP=192.168.x.x` et `[http] :80`. Écran allumé (noir LVGL, pas encore d'UI).

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "Rich_Telemetry: boot (display + WiFi + mDNS + HTTP server)"
```

---

## Task 10: `view.{h,cpp}` — construction des pages et widgets

**Files:**
- Create: `src/view.h`, `src/view.cpp`
- Create/Modify: `data/layout.json` + default compilé dans `view.cpp`
- Modify: `src/main.cpp` (câblage)

La vue maintient, en parallèle du modèle, les objets LVGL : un conteneur par page + les widgets par placement. Elle expose `view_rebuild(d)` (reconstruit tout depuis le modèle), `view_sync(d)` (applique l'état des composants `dirty` à leurs widgets), `view_show_page(d, idx)`.

**Mapping géométrie → LVGL :** ancrage via `lv_obj_align` (`A_CENTER`→`LV_ALIGN_CENTER`, etc.). Ring : `lv_arc`, gap en bas. Convention angulaire LVGL : 0°=3 h, 90°=bas, sens horaire. Gap de `gap_deg` centré en bas ⇒ `bg_angles(90 + gap/2, 90 - gap/2)` (LVGL gère le wrap).

- [ ] **Step 1: Écrire `view.h`**

```cpp
#pragma once
#include "dashboard.h"

void view_rebuild(Dashboard* d);          // détruit + reconstruit toutes les pages/widgets
void view_sync(Dashboard* d);             // applique les composants dirty à leurs widgets
void view_show_page(Dashboard* d, int idx);
const char* view_default_layout();        // JSON du layout par défaut compilé
```

- [ ] **Step 2: Écrire `view.cpp`**

```cpp
#include "view.h"
#include "color.h"
#include <lvgl.h>
#include <string.h>

// Vues parallèles au modèle : 1 conteneur par page, widgets indexés par (page, placement).
static lv_obj_t* s_page_cont[MAX_PAGES];
static lv_obj_t* s_widget[MAX_PAGES][MAX_PLACEMENTS_PER_PAGE];   // objet principal
static lv_obj_t* s_sub1  [MAX_PAGES][MAX_PLACEMENTS_PER_PAGE];   // ex : caption ring / valeur bar
static lv_obj_t* s_sub2  [MAX_PAGES][MAX_PLACEMENTS_PER_PAGE];   // ex : pill ring
static lv_obj_t* s_dots = nullptr;

static const lv_align_t ALIGN_MAP[] = {
    LV_ALIGN_CENTER, LV_ALIGN_TOP_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_LEFT_MID,
    LV_ALIGN_RIGHT_MID, LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_RIGHT, LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_RIGHT
};

static const lv_font_t* pick_font(uint16_t px) {
    if (px >= 28) return &lv_font_montserrat_28;
    if (px >= 20) return &lv_font_montserrat_20;
    return &lv_font_montserrat_14;
}

const char* view_default_layout() {
    return
      "{\"title\":\"Claude\",\"background\":\"#0B0B0F\",\"nav\":{\"wrap\":true},"
      "\"components\":{"
        "\"w5h\":{\"type\":\"ring\",\"color\":\"#38BDF8\",\"pill\":true,\"countdown\":true},"
        "\"w7d\":{\"type\":\"ring\",\"color\":\"#A78BFA\",\"pill\":true,\"countdown\":true},"
        "\"led\":{\"type\":\"led_ring\"},\"buzz\":{\"type\":\"sound\"}},"
      "\"pages\":[{\"name\":\"usage\",\"place\":["
        "{\"ref\":\"w5h\",\"radius\":140,\"thickness\":16,\"gap_deg\":70},"
        "{\"ref\":\"w7d\",\"radius\":105,\"thickness\":16,\"gap_deg\":70}]}]}";
}

static void build_ring(lv_obj_t* parent, Component& c, Placement& q,
                       lv_obj_t** main, lv_obj_t** cap, lv_obj_t** pill) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, q.radius * 2, q.radius * 2);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(arc, 90 + q.gap_deg / 2, 90 - q.gap_deg / 2);
    lv_arc_set_range(arc, c.vmin, c.vmax);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
    *main = arc;

    *cap = lv_label_create(parent);
    lv_obj_set_style_text_font(*cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*cap, lv_color_hex(c.color), 0);
    lv_label_set_text(*cap, "");
    lv_obj_align_to(*cap, arc, LV_ALIGN_CENTER, 0, q.radius - q.thickness - 14);  // dans l'ouverture du bas

    if (c.pill) {
        *pill = lv_label_create(parent);
        lv_obj_set_style_bg_opa(*pill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(*pill, lv_color_hex(c.color), 0);
        lv_obj_set_style_text_color(*pill, lv_color_hex(0x04121A), 0);
        lv_obj_set_style_radius(*pill, 13, 0);
        lv_obj_set_style_pad_hor(*pill, 8, 0); lv_obj_set_style_pad_ver(*pill, 3, 0);
        lv_label_set_text(*pill, "0%");
        lv_obj_align_to(*pill, arc, LV_ALIGN_TOP_MID, 0, -2);
    }
}

void view_rebuild(Dashboard* d) {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(d->background), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    memset(s_widget, 0, sizeof(s_widget));
    memset(s_sub1, 0, sizeof(s_sub1)); memset(s_sub2, 0, sizeof(s_sub2));

    for (int p = 0; p < d->page_count; p++) {
        lv_obj_t* cont = lv_obj_create(scr);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        s_page_cont[p] = cont;

        for (int i = 0; i < d->pages[p].place_count; i++) {
            Placement& q = d->pages[p].places[i];
            Component& c = d->components[q.comp_index];
            switch (c.type) {
                case COMP_RING:
                    build_ring(cont, c, q, &s_widget[p][i], &s_sub1[p][i], &s_sub2[p][i]);
                    break;
                case COMP_LABEL:
                case COMP_READOUT: {
                    lv_obj_t* l = lv_label_create(cont);
                    lv_obj_set_style_text_font(l, pick_font(c.font), 0);
                    lv_obj_set_style_text_color(l, lv_color_hex(c.color), 0);
                    lv_label_set_text(l, "");
                    lv_obj_align(l, ALIGN_MAP[q.anchor], q.dx, q.dy);
                    s_widget[p][i] = l;
                    break;
                }
                case COMP_BAR: {
                    lv_obj_t* b = lv_bar_create(cont);
                    lv_obj_set_size(b, q.width ? q.width : 200, q.height ? q.height : 16);
                    lv_bar_set_range(b, c.vmin, c.vmax);
                    lv_obj_set_style_bg_color(b, lv_color_hex(c.color), LV_PART_INDICATOR);
                    lv_obj_align(b, ALIGN_MAP[q.anchor], q.dx, q.dy);
                    s_widget[p][i] = b;
                    break;
                }
                default: break;   // led_ring / sound : pas de vue écran
            }
        }
    }
    view_show_page(d, d->active_page);
    d->layout_dirty = false;
    for (int i = 0; i < d->comp_count; i++) d->components[i].dirty = true;  // force 1ère synchro
    view_sync(d);
}

void view_show_page(Dashboard* d, int idx) {
    if (idx < 0 || idx >= d->page_count) return;
    d->active_page = idx;
    for (int p = 0; p < d->page_count; p++) {
        if (p == idx) lv_obj_clear_flag(s_page_cont[p], LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag(s_page_cont[p], LV_OBJ_FLAG_HIDDEN);
    }
    // points indicateurs (Task 13 affine le style ; ici on (re)crée la rangée)
}

void view_sync(Dashboard* d) {
    for (int p = 0; p < d->page_count; p++) {
        for (int i = 0; i < d->pages[p].place_count; i++) {
            Placement& q = d->pages[p].places[i];
            Component& c = d->components[q.comp_index];
            if (!c.dirty) continue;
            lv_obj_t* w = s_widget[p][i];
            if (!w) continue;
            switch (c.type) {
                case COMP_LABEL:
                case COMP_READOUT:
                    lv_label_set_text(w, c.vstr);
                    break;
                case COMP_BAR:
                    lv_bar_set_value(w, c.value, LV_ANIM_OFF);
                    break;
                case COMP_RING: {
                    uint32_t col = threshold_color(c.thresholds, c.threshold_count, c.value, c.color);
                    lv_obj_set_style_arc_color(w, lv_color_hex(col), LV_PART_INDICATOR);
                    lv_arc_set_value(w, c.value);
                    if (s_sub1[p][i]) lv_label_set_text(s_sub1[p][i], c.caption);
                    if (s_sub2[p][i]) {
                        char pb[8]; snprintf(pb, sizeof(pb), "%ld%%", (long)c.value);
                        lv_label_set_text(s_sub2[p][i], pb);
                        lv_obj_set_style_bg_color(s_sub2[p][i], lv_color_hex(col), 0);
                    }
                    break;
                }
                default: break;
            }
        }
    }
    for (int i = 0; i < d->comp_count; i++) d->components[i].dirty = false;
    d->values_dirty = false;
}
```

- [ ] **Step 3: Câbler dans `main.cpp`**

Ajouter `#include "view.h"` et un `Dashboard` global. Dans `setup()`, après `guition_lvgl_init()` :

```cpp
static Dashboard g_dash;
// ... dans setup(), après guition_lvgl_init():
char err[80];
dash_set_layout(&g_dash, view_default_layout(), err, sizeof(err));
view_rebuild(&g_dash);
```

Dans `loop()`, avant `lv_timer_handler()` :

```cpp
if (g_dash.layout_dirty) view_rebuild(&g_dash);
if (g_dash.values_dirty) view_sync(&g_dash);
```

- [ ] **Step 4: Build + flash + observer**

Run: `./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload`
Expected (évidence écran) : deux couronnes concentriques sur fond noir (cyan externe, violet interne), à 0 % (vides), pilules « 0% » en haut. Pas encore de données.

- [ ] **Step 5: Commit**

```bash
git add src/view.h src/view.cpp src/main.cpp
git commit -m "Rich_Telemetry: LVGL view (pages + ring/label/readout/bar widgets)"
```

---

## Task 11: `api.{h,cpp}` — `POST /update`, `GET /status`, `GET /`

**Files:**
- Create: `src/api.h`, `src/api.cpp`
- Modify: `src/main.cpp` (enregistrement des routes)

Les handlers parsent dans le modèle (`dash_apply_update`) et lèvent les flags ; le `loop()` synchronise la vue. Pas d'appel LVGL dans les handlers.

- [ ] **Step 1: Écrire `api.h`**

```cpp
#pragma once
#include <WebServer.h>
#include "dashboard.h"
void api_register(WebServer& server, Dashboard* d);
```

- [ ] **Step 2: Écrire `api.cpp`**

```cpp
#include "api.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "config.h"

static Dashboard* D = nullptr;
static WebServer* S = nullptr;

static void h_update() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    char unk[UNKNOWN_CSV_LEN];
    int n = dash_apply_update(D, S->arg("plain").c_str(), unk, sizeof(unk));
    if (n < 0) { S->send(400, "text/plain", "Invalid JSON\n"); return; }
    JsonDocument res; res["ok"] = true; res["updated"] = n;
    if (unk[0]) res["unknown"] = unk;
    String out; serializeJson(res, out); out += "\n";
    S->send(200, "application/json", out);
}

static void h_status() {
    JsonDocument doc;
    doc["ip"]         = WiFi.localIP().toString();
    doc["hostname"]   = String(MDNS_HOST) + ".local";
    doc["rssi"]       = WiFi.RSSI();
    doc["uptime_s"]   = (uint32_t)(millis() / 1000);
    doc["page"]       = D->active_page;
    doc["pages"]      = D->page_count;
    doc["components"] = D->comp_count;
    String out; serializeJson(doc, out); out += "\n";
    S->send(200, "application/json", out);
}

static void h_root() {
    String ip = WiFi.localIP().toString();
    String html =
        "<!doctype html><meta charset=utf-8><title>Rich_Telemetry</title>"
        "<h2>Guition K718 - Rich_Telemetry</h2>"
        "<p>POST /update (valeurs partielles), POST /layout, POST /page.</p>"
        "<pre>curl -X POST http://" + ip + "/update -H 'Content-Type: application/json' \\\n"
        "  -d '{\"w5h\":{\"pct\":63,\"reset_in_s\":6600}}'</pre>"
        "<p><a href=/status>/status</a> &middot; <a href=/layout>/layout</a></p>";
    S->send(200, "text/html", html);
}

void api_register(WebServer& server, Dashboard* d) {
    S = &server; D = d;
    server.on("/update", HTTP_POST, h_update);
    server.on("/status", HTTP_GET,  h_status);
    server.on("/",       HTTP_GET,  h_root);
    server.onNotFound([](){ S->send(404, "text/plain", "Not found\n"); });
}
```

- [ ] **Step 3: Câbler dans `main.cpp`**

Ajouter `#include "api.h"`, et dans `start_services()` avant `server.begin()` : `api_register(server, &g_dash);`

- [ ] **Step 4: Build + flash + smoke curl**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
# remplacer <ip> par l'IP affichée au boot (mDNS guition.local souvent filtré sur le LAN)
curl -X POST http://<ip>/update -H 'Content-Type: application/json' \
  -d '{"w5h":{"pct":63,"reset_in_s":6600},"w7d":{"pct":38,"reset_in_s":453600}}'
curl http://<ip>/status
```
Expected : `{"ok":true,"updated":2}` ; écran : couronne cyan remplie à 63 % avec `1h50` en bas + pill `63%`, violette à 38 % avec `5j6h`. `/status` renvoie l'IP, `page:0`, `components:4`.

- [ ] **Step 5: Commit**

```bash
git add src/api.h src/api.cpp src/main.cpp
git commit -m "Rich_Telemetry: REST /update + /status + help page"
```

---

## Task 12: `POST /layout` + `GET /layout` — reconstruction à chaud

**Files:**
- Modify: `src/api.cpp`, `src/main.cpp`

`POST /layout` : valide via `dash_set_layout` (qui ne remplace qu'en cas de succès), met `layout_dirty` (le `loop()` reconstruira). `GET /layout` renvoie le JSON courant tel quel — on conserve donc une copie du dernier JSON valide.

- [ ] **Step 1: Stocker le dernier layout JSON valide**

Dans `api.cpp`, ajouter un buffer statique et le handler. (Le buffer vit côté `main` pour être partagé avec la persistance Task 14 ; ici on le déclare `extern`.)

Dans `main.cpp` :
```cpp
String g_layout_json;   // dernier layout valide (pour GET /layout + persistance)
```
Dans `api.cpp`, en tête : `extern String g_layout_json;`

- [ ] **Step 2: Ajouter les handlers dans `api.cpp`**

```cpp
static void h_set_layout() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    String body = S->arg("plain");
    char err[80];
    if (!dash_set_layout(D, body.c_str(), err, sizeof(err))) {
        S->send(400, "application/json", String("{\"ok\":false,\"error\":\"") + err + "\"}\n");
        return;
    }
    g_layout_json = body;                 // dernier valide
    // D->layout_dirty est déjà true (posé par dash_set_layout) -> loop() reconstruit
    S->send(200, "application/json", "{\"ok\":true}\n");
}

static void h_get_layout() {
    S->send(200, "application/json", g_layout_json.length() ? g_layout_json : String("{}"));
}
```

Et dans `api_register` :
```cpp
server.on("/layout", HTTP_POST, h_set_layout);
server.on("/layout", HTTP_GET,  h_get_layout);
```

- [ ] **Step 3: Initialiser `g_layout_json` au boot**

Dans `main.cpp` setup, là où on charge le layout par défaut :
```cpp
g_layout_json = view_default_layout();
dash_set_layout(&g_dash, g_layout_json.c_str(), err, sizeof(err));
view_rebuild(&g_dash);
```

- [ ] **Step 4: Build + flash + smoke curl**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
curl -X POST http://<ip>/layout -H 'Content-Type: application/json' \
  -d '{"title":"Sys","background":"#0B0B0F","components":{"cpu":{"type":"readout","label":"CPU","unit":"%"}},"pages":[{"name":"s","place":[{"ref":"cpu","anchor":"CENTER","dy":-20}]}]}'
curl -X POST http://<ip>/update -d '{"cpu":42}'
curl http://<ip>/layout
```
Expected : l'écran bascule sur une page unique affichant `CPU 42 %` au centre ; `GET /layout` renvoie le JSON poussé. Un JSON invalide (`-d '{bad'`) → `400 {"ok":false,...}` et l'écran ne change pas.

- [ ] **Step 5: Commit**

```bash
git add src/api.cpp src/main.cpp
git commit -m "Rich_Telemetry: hot layout swap via POST/GET /layout"
```

---

## Task 13: Points indicateurs de page + bascule `view_show_page`

**Files:**
- Modify: `src/view.cpp`

Affiche une rangée de points en bas (`BOTTOM_MID`), point actif plus clair, recréée à chaque `view_rebuild` et mise à jour à chaque `view_show_page`.

- [ ] **Step 1: Ajouter la rangée de points dans `view_rebuild` (après la boucle pages)**

```cpp
    // points indicateurs (au-dessus de tout, hors conteneurs de page)
    if (s_dots) { lv_obj_del(s_dots); s_dots = nullptr; }
    if (d->page_count > 1) {
        s_dots = lv_obj_create(scr);
        lv_obj_remove_style_all(s_dots);
        lv_obj_set_size(s_dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(s_dots, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(s_dots, 8, 0);
        lv_obj_align(s_dots, LV_ALIGN_BOTTOM_MID, 0, -10);
        for (int p = 0; p < d->page_count; p++) {
            lv_obj_t* dot = lv_obj_create(s_dots);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 9, 9);
            lv_obj_set_style_radius(dot, 5, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x374151), 0);
        }
    }
```

- [ ] **Step 2: Mettre à jour les points dans `view_show_page`**

Remplacer le commentaire de fin par :
```cpp
    if (s_dots) {
        uint32_t n = lv_obj_get_child_cnt(s_dots);
        for (uint32_t p = 0; p < n; p++)
            lv_obj_set_style_bg_color(lv_obj_get_child(s_dots, p),
                lv_color_hex((int)p == idx ? 0xE5E7EB : 0x374151), 0);
    }
```

- [ ] **Step 3: Build + flash + observer (layout à 2+ pages)**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
curl -X POST http://<ip>/layout -H 'Content-Type: application/json' \
  -d '{"components":{"a":{"type":"label","text":"PAGE A","font":28},"b":{"type":"label","text":"PAGE B","font":28}},"pages":[{"name":"A","place":[{"ref":"a","anchor":"CENTER"}]},{"name":"B","place":[{"ref":"b","anchor":"CENTER"}]}]}'
```
Expected : « PAGE A » au centre, **deux points** en bas, le premier clair.

- [ ] **Step 4: Commit**

```bash
git add src/view.cpp
git commit -m "Rich_Telemetry: page dots indicator"
```

---

## Task 14: `persist.{h,cpp}` — LittleFS (chargement boot + sauvegarde)

**Files:**
- Create: `src/persist.h`, `src/persist.cpp`
- Modify: `src/main.cpp`, `src/api.cpp`
- Verify: partition FS dans `default_16MB.csv`

- [ ] **Step 1: Vérifier la présence d'une partition FS**

Run: `pio pkg exec -- python -c "print(open('$HOME/.platformio/platforms/espressif32/tools/partitions/default_16MB.csv').read())"` *(ou ouvrir le fichier de partitions résolu)*
Expected : une ligne `spiffs,  data, spiffs, ...` (LittleFS réutilise cette partition `data/spiffs`). Si absente, créer `partitions_rt.csv` avec une partition `spiffs` et pointer `board_build.partitions` dessus.

- [ ] **Step 2: Écrire `persist.h`**

```cpp
#pragma once
#include <Arduino.h>
bool persist_begin();                       // monte LittleFS (format si besoin)
bool persist_load(String& out);             // lit LAYOUT_PATH -> out ; false si absent
bool persist_save(const String& json);      // écrit LAYOUT_PATH
```

- [ ] **Step 3: Écrire `persist.cpp`**

```cpp
#include "persist.h"
#include <LittleFS.h>
#include "config.h"

bool persist_begin() { return LittleFS.begin(true); }   // true = formate si non monté

bool persist_load(String& out) {
    File f = LittleFS.open(LAYOUT_PATH, "r");
    if (!f) return false;
    out = f.readString();
    f.close();
    return out.length() > 0;
}

bool persist_save(const String& json) {
    File f = LittleFS.open(LAYOUT_PATH, "w");
    if (!f) return false;
    size_t w = f.print(json);
    f.close();
    return w == json.length();
}
```

- [ ] **Step 4: Charger au boot (défaut si absent) dans `main.cpp`**

Remplacer le bloc de chargement du layout dans `setup()` par :
```cpp
persist_begin();
if (!persist_load(g_layout_json) ||
    !dash_set_layout(&g_dash, g_layout_json.c_str(), err, sizeof(err))) {
    g_layout_json = view_default_layout();          // fallback compilé
    dash_set_layout(&g_dash, g_layout_json.c_str(), err, sizeof(err));
}
view_rebuild(&g_dash);
```
Ajouter `#include "persist.h"`.

- [ ] **Step 5: Persister sur `POST /layout`**

Dans `api.cpp` `h_set_layout`, après `g_layout_json = body;` :
```cpp
    if (!persist_save(g_layout_json)) { S->send(500, "text/plain", "FS write failed\n"); return; }
```
Ajouter `#include "persist.h"` dans `api.cpp` et `lib_deps` n'a rien à ajouter (LittleFS est dans le framework).

- [ ] **Step 6: Build + flash + test de persistance (reboot)**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
curl -X POST http://<ip>/layout -H 'Content-Type: application/json' \
  -d '{"components":{"k":{"type":"label","text":"PERSIST","font":28}},"pages":[{"name":"p","place":[{"ref":"k","anchor":"CENTER"}]}]}'
# débrancher/rebrancher la carte (ou bouton RESET), observer le boot
```
Expected : après reboot, l'écran affiche toujours « PERSIST » (chargé depuis LittleFS, sans push).

- [ ] **Step 7: Commit**

```bash
git add src/persist.h src/persist.cpp src/main.cpp src/api.cpp
git commit -m "Rich_Telemetry: persist layout to LittleFS (boot load + save)"
```

---

## Task 15: `led_ring_comp.{h,cpp}` — anneau WS2812

**Files:**
- Create: `src/led_ring_comp.h`, `src/led_ring_comp.cpp`
- Modify: `src/main.cpp`

Machine à états tickée ~30 Hz. Lit l'état du composant `COMP_LED_RING` du modèle. Utilise `rgb_ring.h`.

- [ ] **Step 1: Écrire `led_ring_comp.h`**

```cpp
#pragma once
#include "dashboard.h"
void led_ring_begin();
void led_ring_tick(Dashboard* d, uint32_t now_ms);   // anime selon le mode du composant led_ring
```

- [ ] **Step 2: Écrire `led_ring_comp.cpp`**

```cpp
#include "led_ring_comp.h"
#include "rgb_ring.h"

Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);

void led_ring_begin() { rgb_ring_init(64); }

static void rgb_from_hex(uint32_t hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (hex >> 16) & 0xFF; g = (hex >> 8) & 0xFF; b = hex & 0xFF;
}

void led_ring_tick(Dashboard* d, uint32_t now_ms) {
    int idx = -1;
    for (int i = 0; i < d->comp_count; i++)
        if (d->components[i].type == COMP_LED_RING) { idx = i; break; }
    if (idx < 0) return;
    Component& c = d->components[idx];
    rgb_ring.setBrightness(c.led_brightness ? c.led_brightness : 64);
    uint8_t r, g, b; rgb_from_hex(c.led_color ? c.led_color : 0xFFFFFF, r, g, b);
    const int N = RGB_RING_LED_COUNT;
    uint16_t period = c.led_period_ms ? c.led_period_ms : 1000;

    switch (c.led_mode) {
        case LED_OFF: rgb_ring_clear(); break;
        case LED_SOLID: rgb_ring_set_all(r, g, b); break;
        case LED_PROGRESS: {
            int lit = (c.led_value * N + 50) / 100;     // arrondi
            for (int i = 0; i < N; i++)
                if (i < lit) rgb_ring_set(i, r, g, b); else rgb_ring_set(i, 0, 0, 0);
            break;
        }
        case LED_SPINNER: {
            int head = (now_ms / (period / N ? period / N : 1)) % N;
            for (int i = 0; i < N; i++) rgb_ring_set(i, 0, 0, 0);
            rgb_ring_set(head, r, g, b);
            break;
        }
        case LED_BLINK: {
            bool on = (now_ms % period) < (period / 2);
            if (on) rgb_ring_set_all(r, g, b); else rgb_ring_clear();
            break;
        }
        case LED_BREATHE: {
            float ph = (now_ms % period) / (float)period;        // 0..1
            float k  = 0.5f * (1.0f - cosf(ph * 6.2831853f));     // 0..1..0
            rgb_ring_set_all((uint8_t)(r*k), (uint8_t)(g*k), (uint8_t)(b*k));
            break;
        }
    }
    rgb_ring_show();
}
```

Ajouter `#include <math.h>` en tête pour `cosf`.

- [ ] **Step 3: Câbler dans `main.cpp`**

Ajouter `#include "led_ring_comp.h"`. Dans `setup()` après `view_rebuild` : `led_ring_begin();`. Dans `loop()`, à ~30 Hz :
```cpp
static uint32_t last_led = 0;
if (millis() - last_led >= 33) { last_led = millis(); led_ring_tick(&g_dash, millis()); }
```

- [ ] **Step 4: Build + flash + smoke (chaque mode)**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
curl -X POST http://<ip>/update -d '{"led":{"mode":"solid","color":"#22C55E","brightness":80}}'
curl -X POST http://<ip>/update -d '{"led":{"mode":"progress","value":50,"color":"#38BDF8"}}'
curl -X POST http://<ip>/update -d '{"led":{"mode":"spinner","color":"#A78BFA","period_ms":1200}}'
curl -X POST http://<ip>/update -d '{"led":{"mode":"blink","color":"#EF4444","period_ms":600}}'
curl -X POST http://<ip>/update -d '{"led":{"mode":"off"}}'
```
Expected : anneau vert plein ; puis ~6-7 LEDs allumées ; puis point violet tournant ; puis clignotement rouge ; puis éteint.

- [ ] **Step 5: Commit**

```bash
git add src/led_ring_comp.h src/led_ring_comp.cpp src/main.cpp
git commit -m "Rich_Telemetry: LED ring component (solid/progress/spinner/blink/breathe)"
```

---

## Task 16: `sound_comp.{h,cpp}` — tonalités non-bloquantes

**Files:**
- Create: `src/sound_comp.h`, `src/sound_comp.cpp`
- Modify: `src/main.cpp`

I2S → PCM5100A → ampli (PIN_PA_MUTE high). À chaque tick on écrit un petit buffer (non bloquant via timeout 0). Une tonalité enfilée se joue pour `snd_ms` puis s'arrête. Bips nommés → (fréquence, durée) prédéfinies.

- [ ] **Step 1: Écrire `sound_comp.h`**

```cpp
#pragma once
#include "dashboard.h"
void sound_begin();
void sound_tick(Dashboard* d);   // consomme les événements snd_pending, alimente l'I2S
```

- [ ] **Step 2: Écrire `sound_comp.cpp`**

```cpp
#include "sound_comp.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "guition_pins.h"

static constexpr uint32_t SR = 44100;
static constexpr size_t   FRAMES = 256;
static i2s_chan_handle_t  tx = nullptr;
static int16_t            buf[FRAMES * 2];
static float              phase = 0, inc = 0;
static int32_t            remaining_frames = 0;     // > 0 => en train de jouer

static void name_to_tone(const char* n, uint16_t& hz, uint16_t& ms) {
    if      (!strcmp(n,"ok"))    { hz = 880;  ms = 120; }
    else if (!strcmp(n,"alert")) { hz = 1175; ms = 250; }
    else if (!strcmp(n,"error")) { hz = 220;  ms = 400; }
    else                         { hz = 660;  ms = 150; }
}

void sound_begin() {
    pinMode(PIN_PA_MUTE, OUTPUT);
    digitalWrite(PIN_PA_MUTE, HIGH);
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&cc, &tx, nullptr);
    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SR),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = (gpio_num_t)PIN_I2S_BCK,
                      .ws = (gpio_num_t)PIN_I2S_WS, .dout = (gpio_num_t)PIN_I2S_DO,
                      .din = I2S_GPIO_UNUSED,
                      .invert_flags = { false, false, false } },
    };
    i2s_channel_init_std_mode(tx, &sc);
    i2s_channel_enable(tx);
}

void sound_tick(Dashboard* d) {
    // démarre un nouveau son si un composant sound a un événement en attente
    for (int i = 0; i < d->comp_count; i++) {
        Component& c = d->components[i];
        if (c.type == COMP_SOUND && c.snd_pending) {
            c.snd_pending = false;
            uint16_t hz = c.snd_tone, ms = c.snd_ms;
            if (c.snd_name[0]) name_to_tone(c.snd_name, hz, ms);
            inc = 2.0f * (float)M_PI * hz / SR;
            phase = 0;
            remaining_frames = (int32_t)((uint32_t)ms * SR / 1000);
        }
    }
    // alimente un buffer (non bloquant). silence si rien à jouer.
    for (size_t k = 0; k < FRAMES; k++) {
        int16_t s = 0;
        if (remaining_frames > 0) {
            s = (int16_t)(0.30f * 32767.0f * sinf(phase));
            phase += inc; if (phase >= 2*M_PI) phase -= 2*M_PI;
            remaining_frames--;
        }
        buf[k*2] = s; buf[k*2+1] = s;
    }
    size_t w; i2s_channel_write(tx, buf, sizeof(buf), &w, 0);   // timeout 0 = non bloquant
}
```

- [ ] **Step 3: Câbler dans `main.cpp`**

Ajouter `#include "sound_comp.h"`. Dans `setup()` : `sound_begin();`. Dans `loop()` : `sound_tick(&g_dash);` (à chaque itération — il faut alimenter l'I2S régulièrement).

⚠️ Conflit I2S : `sound_comp` et un éventuel mic utilisent `I2S_NUM_0`. Ici seul l'audio out l'utilise → OK.

- [ ] **Step 4: Build + flash + smoke (écouter)**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
curl -X POST http://<ip>/update -d '{"buzz":{"name":"alert"}}'
curl -X POST http://<ip>/update -d '{"buzz":{"tone":880,"ms":200}}'
```
Expected : un bip « alert » (~1175 Hz, 250 ms) puis un bip 880 Hz 200 ms. Le reste de l'UI reste fluide (pas de blocage).

- [ ] **Step 5: Commit**

```bash
git add src/sound_comp.h src/sound_comp.cpp src/main.cpp
git commit -m "Rich_Telemetry: non-blocking tone/beep sound component"
```

---

## Task 17: `nav_input.{h,cpp}` — encodeur + `POST /page`

**Files:**
- Create: `src/nav_input.h`, `src/nav_input.cpp`
- Modify: `src/main.cpp`, `src/api.cpp`

Encodeur via `bidi_switch_knob` (comme `Basic_Audio`). Delta lu dans `loop()` → `nav_next/prev` → `view_show_page`. `POST /page` accepte `{dir|index|name}`.

- [ ] **Step 1: Écrire `nav_input.h`**

```cpp
#pragma once
#include "dashboard.h"
void nav_begin();
void nav_tick(Dashboard* d);                 // lit l'encodeur, change de page si besoin
void nav_goto_dir(Dashboard* d, int delta);  // delta>0 suivant, <0 précédent (REST/encodeur)
```

- [ ] **Step 2: Écrire `nav_input.cpp`**

```cpp
#include "nav_input.h"
#include "nav_logic.h"
#include "view.h"
#include "bidi_switch_knob.h"
#include "guition_pins.h"

static knob_handle_t knob = nullptr;

void nav_begin() {
    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);
}

void nav_goto_dir(Dashboard* d, int delta) {
    if (d->page_count <= 1) return;
    int idx = delta > 0 ? nav_next(d->active_page, d->page_count, d->nav_wrap)
                        : nav_prev(d->active_page, d->page_count, d->nav_wrap);
    view_show_page(d, idx);
}

void nav_tick(Dashboard* d) {
    int delta = iot_knob_get_count_value(knob);
    if (delta == 0) return;
    iot_knob_clear_count_value(knob);
    nav_goto_dir(d, delta > 0 ? +1 : -1);     // un cran = une page
}
```

- [ ] **Step 3: `POST /page` dans `api.cpp`**

Ajouter `#include "nav_input.h"` et `#include "view.h"`, puis :
```cpp
static void h_page() {
    JsonDocument doc;
    if (!S->hasArg("plain") || deserializeJson(doc, S->arg("plain"))) {
        S->send(400, "text/plain", "Invalid JSON\n"); return;
    }
    if (doc["dir"].is<const char*>()) {
        nav_goto_dir(D, strcmp(doc["dir"], "prev") == 0 ? -1 : +1);
    } else if (doc["index"].is<int>()) {
        view_show_page(D, doc["index"]);
    } else if (doc["name"].is<const char*>()) {
        for (int p = 0; p < D->page_count; p++)
            if (strcmp(D->pages[p].name, doc["name"]) == 0) { view_show_page(D, p); break; }
    }
    JsonDocument res; res["page"] = D->active_page;
    res["name"] = D->pages[D->active_page].name;
    String out; serializeJson(res, out); out += "\n";
    S->send(200, "application/json", out);
}
```
Ajouter dans `api_register` : `server.on("/page", HTTP_POST, h_page);` et `#include <string.h>`.

- [ ] **Step 4: Câbler dans `main.cpp`**

`#include "nav_input.h"` ; `nav_begin();` dans `setup()` ; `nav_tick(&g_dash);` dans `loop()`.

- [ ] **Step 5: Build + flash + test (encodeur + REST), layout 3 pages**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
curl -X POST http://<ip>/layout -H 'Content-Type: application/json' \
  -d '{"components":{"a":{"type":"label","text":"A","font":28},"b":{"type":"label","text":"B","font":28},"c":{"type":"label","text":"C","font":28}},"pages":[{"name":"A","place":[{"ref":"a","anchor":"CENTER"}]},{"name":"B","place":[{"ref":"b","anchor":"CENTER"}]},{"name":"C","place":[{"ref":"c","anchor":"CENTER"}]}]}'
curl -X POST http://<ip>/page -d '{"dir":"next"}'
curl -X POST http://<ip>/page -d '{"name":"C"}'
```
Expected : tourner l'encodeur **horaire** avance (A→B→C→A), **anti-horaire** recule ; le point actif suit. `POST /page {"dir":"next"}` avance d'une page ; `{"name":"C"}` saute à C.

- [ ] **Step 6: Commit**

```bash
git add src/nav_input.h src/nav_input.cpp src/api.cpp src/main.cpp
git commit -m "Rich_Telemetry: page nav via encoder + POST /page"
```

---

## Task 18: `touch_cst816.{h,cpp}` — swipe (étape finale)

**Files:**
- Create: `src/touch_cst816.h`, `src/touch_cst816.cpp`
- Modify: `src/main.cpp`, `src/view.cpp`, `platformio.ini`

Bring-up CST816 via `esp_lcd_touch_cst816s`, exposé comme `indev` pointeur LVGL. Les gestures (`lv_indev_get_gesture_dir`) sur l'écran déclenchent la navigation.

- [ ] **Step 1: Ajouter la dépendance**

Dans `platformio.ini` `[env:esp32s3]` `lib_deps`, ajouter :
```ini
    espressif/esp_lcd_touch_cst816s
```

- [ ] **Step 2: Écrire `touch_cst816.h`**

```cpp
#pragma once
void touch_begin();    // I2C + CST816 + enregistrement indev pointeur LVGL
```

- [ ] **Step 3: Écrire `touch_cst816.cpp`**

```cpp
#include "touch_cst816.h"
#include <lvgl.h>
#include "driver/i2c.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lcd_panel_io.h"
#include "guition_pins.h"

static esp_lcd_touch_handle_t tp = nullptr;

static void touch_read_cb(lv_indev_drv_t*, lv_indev_data_t* data) {
    uint16_t x[1], y[1]; uint8_t cnt = 0;
    esp_lcd_touch_read_data(tp);
    bool pressed = esp_lcd_touch_get_coordinates(tp, x, y, nullptr, &cnt, 1);
    if (pressed && cnt > 0) {
        data->point.x = x[0]; data->point.y = y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void touch_begin() {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = PIN_I2C_SDA; conf.scl_io_num = PIN_I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE; conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    esp_lcd_panel_io_handle_t io = nullptr;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &io_cfg, &io);

    esp_lcd_touch_config_t tcfg = {};
    tcfg.x_max = LCD_H_RES; tcfg.y_max = LCD_V_RES;
    tcfg.rst_gpio_num = (gpio_num_t)PIN_TOUCH_RST;
    tcfg.int_gpio_num = (gpio_num_t)PIN_TOUCH_INT;
    esp_lcd_touch_new_i2c_cst816s(io, &tcfg, &tp);

    static lv_indev_drv_t drv;
    lv_indev_drv_init(&drv);
    drv.type = LV_INDEV_TYPE_POINTER;
    drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&drv);
}
```

- [ ] **Step 4: Gestures → navigation (dans `view.cpp` au moment du rebuild)**

Dans `view_rebuild`, après avoir réglé le fond de `scr`, attacher un handler de geste sur l'écran. Ajouter en haut de `view.cpp` :
```cpp
#include "nav_input.h"
static Dashboard* s_dash_for_gesture = nullptr;
static void gesture_cb(lv_event_t* e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (!s_dash_for_gesture) return;
    if (dir == LV_DIR_LEFT || dir == LV_DIR_TOP)        nav_goto_dir(s_dash_for_gesture, +1);
    else if (dir == LV_DIR_RIGHT || dir == LV_DIR_BOTTOM) nav_goto_dir(s_dash_for_gesture, -1);
}
```
Et dans `view_rebuild`, juste après le réglage du fond :
```cpp
    s_dash_for_gesture = d;
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);
```

- [ ] **Step 5: Câbler `touch_begin()` dans `main.cpp`**

`#include "touch_cst816.h"` ; appeler `touch_begin();` dans `setup()` après `guition_lvgl_init()` et **avant** `view_rebuild`.

- [ ] **Step 6: Build + flash + test swipe (layout 3 pages de la Task 17)**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
```
Expected : swipe **gauche** (ou haut) → page suivante ; swipe **droite** (ou bas) → précédente ; le point actif suit. Encodeur et REST marchent toujours.

> Si le CST816 ne répond pas : vérifier l'adresse I2C (scan), `PIN_TOUCH_RST/INT`, et l'ordre d'init (I2C avant `esp_lcd_touch_new_i2c_cst816s`). Loop brake : après 3 essais infructueux, s'arrêter et remonter les symptômes (cf. règle 4).

- [ ] **Step 7: Commit**

```bash
git add src/touch_cst816.h src/touch_cst816.cpp src/view.cpp src/main.cpp platformio.ini
git commit -m "Rich_Telemetry: CST816 touch swipe navigation"
```

---

## Task 19: Décompte 1 Hz dans `loop()` + câblage final des ticks

**Files:**
- Modify: `src/main.cpp`

Brancher `dash_tick_countdown` (1 Hz) pour que les couronnes `countdown` décomptent à l'écran sans push.

- [ ] **Step 1: Ajouter le tick 1 Hz dans `loop()`**

```cpp
static uint32_t last_sec = 0;
if (millis() - last_sec >= 1000) { last_sec = millis(); dash_tick_countdown(&g_dash, 1); }
```
(à placer avant `if (g_dash.values_dirty) view_sync(&g_dash);`)

- [ ] **Step 2: Build + flash + observer**

```bash
./build.sh guition Rich_Telemetry && ./build.sh auto Rich_Telemetry --upload
# pousser un layout avec une couronne countdown puis :
curl -X POST http://<ip>/update -d '{"w5h":{"pct":63,"reset_in_s":125}}'
```
Expected : le bas de la couronne affiche `2m`, puis `1m`, puis `59s`… qui décroît seul une fois par seconde, sans nouveau push.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "Rich_Telemetry: 1 Hz server-side countdown wired into loop"
```

---

## Task 20: `tools/push.py` + `data/layout.json` + `README.md`

**Files:**
- Create: `tools/push.py`
- Create: `data/layout.json`
- Create: `README.md`

- [ ] **Step 1: Écrire `data/layout.json` (le layout « fenêtres Claude »)**

```json
{
  "title": "Claude",
  "background": "#0B0B0F",
  "nav": { "wrap": true },
  "components": {
    "w5h": {"type":"ring","color":"#38BDF8","pill":true,"countdown":true,
            "thresholds":[[70,"#22C55E"],[90,"#F59E0B"],[100,"#EF4444"]]},
    "w7d": {"type":"ring","color":"#A78BFA","pill":true,"countdown":true},
    "led": {"type":"led_ring"},
    "buzz":{"type":"sound"}
  },
  "pages": [
    {"name":"usage","place":[
      {"ref":"w5h","radius":140,"thickness":16,"gap_deg":70},
      {"ref":"w7d","radius":105,"thickness":16,"gap_deg":70}]}
  ]
}
```

- [ ] **Step 2: Écrire `tools/push.py` (exemple : pousse des % factices décroissants)**

```python
#!/usr/bin/env python3
"""Pousse les fenêtres d'usage Claude (factices) vers Rich_Telemetry.
Usage: python3 tools/push.py http://<ip> [--interval 5]"""
import sys, time, argparse, requests

ap = argparse.ArgumentParser()
ap.add_argument("base")                       # ex http://192.168.1.42
ap.add_argument("--interval", type=float, default=5.0)
a = ap.parse_args()

pct5, pct7 = 10, 5
r5, r7 = 5*3600, 7*86400
while True:
    payload = {
        "w5h": {"pct": pct5, "reset_in_s": r5},
        "w7d": {"pct": pct7, "reset_in_s": r7},
        "led": {"mode": "progress", "value": pct5, "color": "#38BDF8"},
    }
    try:
        resp = requests.post(a.base + "/update", json=payload, timeout=3)
        print(resp.status_code, resp.text.strip())
    except Exception as e:
        print("err:", e)
    pct5 = min(100, pct5 + 3); pct7 = min(100, pct7 + 1)
    r5 = max(0, r5 - int(a.interval)); r7 = max(0, r7 - int(a.interval))
    time.sleep(a.interval)
```

- [ ] **Step 3: Écrire `README.md`**

Documenter : config WiFi (`secrets.h`), build/flash (`./build.sh guition Rich_Telemetry` / `./build.sh auto Rich_Telemetry --upload`), le modèle config (composants/pages/placements), le catalogue de types + leur valeur `/update`, les routes REST, la navigation (encodeur/swipe/REST), la persistance LittleFS, et la limite ASCII des polices. S'appuyer sur le README de `Basic_WiFi_Telemetry` pour le ton et la structure.

- [ ] **Step 4: Test end-to-end**

```bash
pip install requests
python3 tools/push.py http://<ip> --interval 3
```
Expected : les deux couronnes se remplissent progressivement, l'anneau LED suit `w5h` en mode progress, le décompte tourne.

- [ ] **Step 5: Commit**

```bash
git add README.md data/layout.json tools/push.py
git commit -m "Rich_Telemetry: default layout, push.py client, README"
```

---

## Task 21: Mise à jour de la doc monorepo

**Files:**
- Modify: `devices/guition_knob/README.md` (lister Rich_Telemetry)
- Modify: `devices/guition_knob/.claude/skills/guition-k718/` si un index de projets existe

- [ ] **Step 1: Ajouter Rich_Telemetry à la liste des projets Guition**

Repérer la section listant `Basic_*` dans `devices/guition_knob/README.md` et y ajouter une ligne décrivant `Rich_Telemetry` (dashboard config-driven + pages + ring/LED/son via REST).

- [ ] **Step 2: Commit**

```bash
git add devices/guition_knob/README.md
git commit -m "Guition README: list Rich_Telemetry"
```

---

## Self-Review (à exécuter après écriture, corrigé inline)

**Couverture spec → tâches :**
- §3 catalogue (label/readout/bar/ring) → Tasks 6/10 ; led_ring → 15 ; sound → 16.
- §4 ring (gap bas, pill, center_pct, thresholds, countdown) → 10 (build) + 7/8 (logique) + 19 (tick).
- §5 schémas JSON layout/update → 6/7 (parse/apply) + 11/12 (routes).
- §6 API (/update, /layout GET+POST, /page, /status, /) → 11/12/17.
- §7 nav (encodeur/REST/swipe, dots, wrap) → 13/17/18 + nav_logic (5).
- §8 cycle loop → 9/10/15/16/17/19.
- §9 persistance + erreurs (validate-before-swap, unknown, 400/500) → 6/11/12/14.
- §11 tests cœur pur → 3/4/5/6/7/8.

**Cohérence des types :** `Dashboard`/`Component`/`Placement`/`Page` définis en Task 2, utilisés tels quels partout. Fonctions : `dash_set_layout`/`dash_apply_update`/`dash_find`/`dash_tick_countdown` (Tasks 6-8), `nav_next/prev` (5), `view_rebuild/sync/show_page/default_layout` (10/13), `led_ring_tick` (15), `sound_tick` (16), `nav_begin/tick/goto_dir` (17), `touch_begin` (18) — signatures identiques entre déclaration et usage.

**Points à vérifier en cours d'implé (notés, pas des trous de design) :**
- Task 14 Step 1 : partition `spiffs` dans `default_16MB.csv` (sinon CSV custom).
- Task 18 : API exacte de `esp_lcd_touch_cst816s` selon la version tirée (signatures `esp_lcd_touch_get_coordinates` / config) — vérifier l'en-tête du composant après résolution `lib_deps`.
- LVGL `lv_arc_set_bg_angles` avec gap en bas : valider visuellement l'ouverture (Task 10 Step 4).
